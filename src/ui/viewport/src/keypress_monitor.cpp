// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "xstudio/ui/viewport/keypress_monitor.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::ui::keypress_monitor;

/* This actor's job is to monitor key down and key up events and maintain an
up-to date list of keys that are held down, broacasting to interested actors
(particularly 'Module' type actors).*/

KeypressMonitor::KeypressMonitor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    // set_default_handler(caf::drop);
    system().registry().put(keyboard_events, this);
    keyboard_events_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(keyboard_events_group_);

    hotkey_config_events_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(hotkey_config_events_group_);

    try {
        auto prefs = global_store::GlobalStoreHelper(system());
        utility::JsonStore j;
        utility::join_broadcast(this, prefs.get_group(j));
        hotkey_overrides_ = global_store::preference_value<utility::JsonStore>(
            j, "/ui/keyboard/hotkey_overrides");
    } catch (std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }


    behavior_.assign(
        [=](utility::get_event_group_atom) -> caf::actor { return keyboard_events_group_; },
        [=](utility::get_event_group_atom, hotkey_event_atom) -> caf::actor {
            return hotkey_config_events_group_;
        },
        [=](all_keys_up_atom, const std::string &context) { all_keys_up(context); },
        [=](key_down_atom,
            const std::vector<std::string> &sequence,
            const utility::Uuid &hk_uuid) -> result<std::string> {
            // this checks if 'sequence' clashes with any existing hotkey (other than the one
            // with uuid hk_uuid, which is ignored in the check). If there is a clash, the name
            // of the clashing hotkey is returned. If there is no clash, an empty string is
            // returned. If there is an error, an error is returned.
            try {
                int modifiers = 0;
                int key       = 0;
                Hotkey::sequence_to_key_and_modifier(sequence, key, modifiers);
                for (const auto &hk : active_hotkeys_) {
                    if (hk.first != hk_uuid && hk.second.key_id() == key &&
                        hk.second.modifiers() == modifiers) {
                        return hk.second.hotkey_name();
                    }
                }
                if (active_hotkeys_.find(hk_uuid) != active_hotkeys_.end()) {
                    // if 'sequence' is already set on the given hotkey we return 'self' to
                    // indicate there is no change
                    if (active_hotkeys_[hk_uuid].key_id() == key &&
                        active_hotkeys_[hk_uuid].modifiers() == modifiers) {
                        return "self";
                    }
                }
            } catch (const std::exception &err) {
                return caf::make_error(xstudio_error::error, err.what());
            }
            return "";
        },
        [=](key_down_atom,
            int key,
            const std::string &key_text,
            const std::string &context,
            const std::string &window,
            const bool auto_repeat) {
            if (Hotkey::key_names.find(key) == Hotkey::key_names.end()) {
                extra_key_names_[key] = key_text;
            }

            if (actor_grabbing_all_keyboard_input_) {
                anon_mail(key_down_atom_v, key, context, auto_repeat)
                    .send(actor_grabbing_all_keyboard_input_);
            } else {

                if (key == 0x1000020) {
                    // See note below. Shift key is a problem here, as if the
                    // user holds '1' key then hits shift, we get a key down
                    // for the 1 key but also the ! key (when the shift is hit)
                    // but no key release for the 1 key. So to activate any hotkey
                    // with shift, the shift key must go down FIRST.
                    auto p = held_keys_.begin();
                    while (p != held_keys_.end()) {
                        if (Hotkey::key_to_modifier.find(*p) == Hotkey::key_to_modifier.end()) {
                            p = held_keys_.erase(p);
                        } else {
                            p++;
                        }
                    }
                }
                held_keys_.insert(key);
                held_keys_changed(context, auto_repeat, window);
            }
        },
        [=](key_up_atom, int key, const std::string &context, const std::string &window) {
            // Shift key is a special case.. . if user presses SHIFT+1 key, we get told
            // that the shift and ! keys are down. If the user lifts the shift key and
            // continues to hold the 1 key, we get a key up event for the shift key but
            // not for the ! key. We then get a key down event for the 1 key. Because
            // we don't know the keyboard layout, we don't know that the 1 key is
            // actually the ! key, so we have to do an all keys up to make sure we
            // don't have zombie keys in the held keys set.
            if (key == 0x1000020) {
                held_keys_.clear();
                held_keys_changed(context, false, window);
                return;
            }

            if (held_keys_.find(key) != held_keys_.end()) {
                held_keys_.erase(held_keys_.find(key));
                held_keys_changed(context);
            }
        },
        [=](text_entry_atom,
            const std::string &text,
            const std::string &context,
            const std::string &window) {
            if (actor_grabbing_all_keyboard_input_) {
                anon_mail(text_entry_atom_v, text, context)
                    .send(actor_grabbing_all_keyboard_input_);
            } else {
                mail(text_entry_atom_v, text, context).send(keyboard_events_group_);
            }
        },
        [=](mouse_event_atom, const PointerEvent &e) {
            if (actor_grabbing_all_mouse_input_.size()) {
                anon_mail(mouse_event_atom_v, e).send(actor_grabbing_all_mouse_input_.front());
            } else {
                mail(mouse_event_atom_v, e).send(keyboard_events_group_);
            }
        },
        [=](module::grab_all_keyboard_input_atom, caf::actor actor, const bool grab) {
            if (grab) {
                actor_grabbing_all_keyboard_input_ = actor;
            } else if (actor_grabbing_all_keyboard_input_ == actor) {
                actor_grabbing_all_keyboard_input_ = caf::actor();
            }
        },
        [=](module::grab_all_mouse_input_atom, caf::actor actor, const bool grab) {
            auto p = std::find(
                actor_grabbing_all_mouse_input_.begin(),
                actor_grabbing_all_mouse_input_.end(),
                actor);

            if (p != actor_grabbing_all_mouse_input_.end()) {
                actor_grabbing_all_mouse_input_.erase(p);
            }

            if (grab) {
                actor_grabbing_all_mouse_input_.insert(
                    actor_grabbing_all_mouse_input_.begin(), actor);
            }
        },

        [=](watch_hotkey_atom, const utility::Uuid &hk_uuid, caf::actor watcher) {
            auto p = active_hotkeys_.find(hk_uuid);
            if (p != active_hotkeys_.end()) {
                p->second.add_watcher(caf::actor_cast<caf::actor_addr>(watcher));
            }
        },

        [=](watch_hotkey_atom,
            const utility::Uuid &hk_uuid,
            caf::actor watcher,
            bool exclusive_watcher) {
            auto p = active_hotkeys_.find(hk_uuid);
            if (p != active_hotkeys_.end()) {
                p->second.exclusive_watcher(
                    caf::actor_cast<caf::actor_addr>(watcher), exclusive_watcher);
            }
        },

        [=](json_store::update_atom, register_hotkey_atom) {
            // this message was sent by ourselves (just below here)
            if (overrides_changed_) {
                // store the hotkey overrides in the global store, so they can be reapplied on
                // next startup
                overrides_changed_ = false;
                global_store::GlobalStoreHelper helper(system());
                helper.set_value(
                    hotkey_overrides_, "/ui/keyboard/hotkey_overrides", true, false);
            }
        },

        [=](register_hotkey_atom, const utility::Uuid &id, int key, int modifiers)
            -> result<bool> {
            // Not actually registering a hotkey here, but updating the key and modifier of an
            // existing hotkey
            if (active_hotkeys_.find(id) != active_hotkeys_.end()) {
                if (active_hotkeys_[id].update(key, modifiers)) {

                    mail(hotkey_event_atom_v, active_hotkeys_[id])
                        .send(hotkey_config_events_group_);

                    if (key == active_hotkeys_[id].default_key_id() &&
                        modifiers == active_hotkeys_[id].default_modifiers()) {
                        // overridden hotkey has been reset to default, so remove any
                        // override entry for this hotkey
                        auto p = hotkey_overrides_.find(to_string(id));
                        if (p != hotkey_overrides_.end()) {
                            hotkey_overrides_.erase(p);
                        }
                    } else {
                        hotkey_overrides_[to_string(id)] = {
                            {"name", active_hotkeys_[id].hotkey_name()},
                            {"key", key},
                            {"modifiers", modifiers}};
                    }

                    // now we must unassign other hotkeys that clash with the new key/modifier
                    // of this hotkey (other than itself).
                    for (auto &hk : active_hotkeys_) {
                        if (hk.first != id && hk.second.key_id() == key &&
                            hk.second.modifiers() == modifiers) {
                            if (hk.second.update(-1, 0)) {
                                mail(hotkey_event_atom_v, hk.second)
                                    .send(hotkey_config_events_group_);
                                hotkey_overrides_[to_string(hk.first)] = {
                                    {"name", hk.second.hotkey_name()},
                                    {"key", -1},
                                    {"modifiers", 0}};
                            }
                        }
                    }

                    if (!overrides_changed_) {
                        // schedule an update of the hotkey overrides in the global store
                        overrides_changed_ = true;
                        mail(json_store::update_atom_v, register_hotkey_atom_v)
                            .delay(std::chrono::seconds(5))
                            .send(caf::actor_cast<caf::actor>(this));
                    }
                    return true;

                } else {
                    return caf::make_error(xstudio_error::error, "Hotkey not changed");
                }
            }
            return caf::make_error(
                xstudio_error::error, "Invalid hotkey uuid '" + to_string(id) + "'");
        },

        [=](register_hotkey_atom, const Hotkey &hk) -> bool {
            bool result = false;
            if (active_hotkeys_.find(hk.uuid()) != active_hotkeys_.end()) {

                // add any new hotkey watchers to the existing hotkey
                result = active_hotkeys_[hk.uuid()].update(hk, false);

            } else {

                active_hotkeys_[hk.uuid()] = hk;
                apply_hotkey_overrides(hk.uuid());
                result = true;
            }

            for (auto &p : hk.watchers()) {
                monitor(
                    caf::actor_cast<caf::actor>(p),
                    [this, addr = caf::actor_cast<caf::actor>(p).address()](const error &) {
                        auto p = std::find(
                            actor_grabbing_all_mouse_input_.begin(),
                            actor_grabbing_all_mouse_input_.end(),
                            caf::actor_cast<caf::actor>(addr));
                        if (p != actor_grabbing_all_mouse_input_.end()) {
                            actor_grabbing_all_mouse_input_.erase(p);
                        }

                        if (addr == actor_grabbing_all_keyboard_input_) {
                            actor_grabbing_all_keyboard_input_ = caf::actor();
                        } else {
                            caf::actor_addr w = caf::actor_cast<caf::actor_addr>(addr);
                            for (auto &p : active_hotkeys_) {
                                p.second.watcher_died(w);
                            }
                        }
                    });
            }

            std::vector<Hotkey> hks;
            for (const auto &p : active_hotkeys_)
                hks.push_back(p.second);
            mail(hotkey_event_atom_v, active_hotkeys_[hk.uuid()])
                .send(hotkey_config_events_group_);
            mail(hotkey_event_atom_v, hks).send(hotkey_config_events_group_);
            return result;
        },

        [=](register_hotkey_atom) -> std::vector<Hotkey> {
            std::vector<Hotkey> hks;
            for (const auto &p : active_hotkeys_)
                hks.push_back(p.second);
            return hks;
        },

        [=](hotkey_atom, const utility::Uuid kotkey_uuid) -> result<Hotkey> {
            auto p = active_hotkeys_.find(kotkey_uuid);
            if (p != active_hotkeys_.end()) {
                return p->second;
            }
            return make_error(
                xstudio_error::error, "Invalid hotkey uuid '" + to_string(kotkey_uuid) + "'");
        },

        [=](hotkey_atom, const std::string &hotkey_name) -> result<Hotkey> {
            auto p = active_hotkeys_.begin();
            while (p != active_hotkeys_.end()) {
                if (p->second.hotkey_name() == hotkey_name) {
                    return p->second;
                }
                p++;
            }
            return make_error(
                xstudio_error::error, "Invalid hotkey name '" + hotkey_name + "'");
        },

        [=](keypress_monitor::hotkey_event_atom,
            const utility::Uuid kotkey_uuid,
            const bool pressed,
            const std::string &context,
            const std::string &window,
            const bool due_to_focus_change) {
            mail(
                hotkey_event_atom_v, kotkey_uuid, pressed, context, window, due_to_focus_change)
                .send(hotkey_config_events_group_);
        },

        [=](json_store::update_atom,
            const utility::JsonStore &change,
            const std::string &path,
            const utility::JsonStore & /*full*/) {
            if (path == "/ui/keyboard/hotkey_overrides/value") {
                if (hotkey_overrides_ != change) {
                    hotkey_overrides_ = change;
                    apply_hotkey_overrides();
                }
            }
        },

        [=](json_store::update_atom, const utility::JsonStore &) {},

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const error &err) mutable {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        });
}

void KeypressMonitor::apply_hotkey_overrides(const utility::Uuid &hotkey_id) {

    if (hotkey_id.is_null()) {
        for (auto &p : active_hotkeys_) {
            apply_hotkey_overrides(p.first);
        }
    } else if (active_hotkeys_.find(hotkey_id) != active_hotkeys_.end()) {
        Hotkey &hk = active_hotkeys_[hotkey_id];
        auto q     = hotkey_overrides_.find(to_string(hotkey_id));
        if (q != hotkey_overrides_.end()) {
            const nlohmann::json &hk_override = *q;
            int key                           = hk_override.value("key", hk.key_id());
            int modifiers                     = hk_override.value("modifiers", hk.modifiers());
            if (hk.update(key, modifiers)) {
                mail(hotkey_event_atom_v, hk).send(hotkey_config_events_group_);
            }
        }
    }
}

void KeypressMonitor::on_exit() {
    system().registry().erase(keyboard_events);
    actor_grabbing_all_keyboard_input_ = caf::actor();
    actor_grabbing_all_mouse_input_.clear();
}

void KeypressMonitor::held_keys_changed(
    const std::string &context, const bool auto_repeat, const std::string &window) {

    if (actor_grabbing_all_keyboard_input_) {
        /*auto addr = caf::actor_cast<caf::actor_addr>(actor_grabbing_all_keyboard_input_);
        for (auto & p : active_hotkeys_) {
            if (p.owner_ == addr) {
                p.update_state(held_keys_);
            }
        }*/
    } else {


        if (held_keys_ != previous_held_keys_) {

            previous_held_keys_ = held_keys_;
            std::vector<std::string> keys, modifiers;
            for (const auto &k : held_keys_) {

                if (Hotkey::key_to_modifier.find(k) != Hotkey::key_to_modifier.end()) {
                    modifiers.push_back(Hotkey::key_names.find(k)->second);
                    continue;
                }

                auto p = Hotkey::key_names.find(k);
                if (p != Hotkey::key_names.end()) {
                    keys.push_back(p->second);
                    continue;
                }
                auto q = extra_key_names_.find(k);
                if (q != extra_key_names_.end()) {
                    keys.push_back(q->second);
                }
            }
            mail(hotkey_event_atom_v, keys, modifiers).send(hotkey_config_events_group_);
        }

        // special 'configureation' context is used to indicate that hotkeys are being edited in
        // the hotkey configuration dialog. In this case we don't want to trigger hotkey
        // actions.
        if (context != "configuration") {
            for (auto &p : active_hotkeys_) {
                p.second.update_state(
                    held_keys_,
                    context,
                    window,
                    auto_repeat,
                    caf::actor_cast<caf::actor>(this));
            }
        }
    }
}

void KeypressMonitor::all_keys_up(const std::string &context) {

    held_keys_.clear();
    for (auto &p : active_hotkeys_) {
        p.second.update_state(
            held_keys_, context, "", false, caf::actor_cast<caf::actor>(this), true);
    }
}
