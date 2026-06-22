// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "xstudio/ui/viewport/keypress_monitor.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"

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

    behavior_.assign(
        [=](utility::get_event_group_atom) -> caf::actor { return keyboard_events_group_; },
        [=](utility::get_event_group_atom, hotkey_event_atom) -> caf::actor {
            return hotkey_config_events_group_;
        },
        [=](all_keys_up_atom, const std::string &context) { all_keys_up(context); },
        [=](key_down_atom,
            int key,
            const std::string &context,
            const std::string &window,
            const bool auto_repeat) {
            if (actor_grabbing_all_keyboard_input_) {
                anon_mail(key_down_atom_v, key, context, auto_repeat)
                    .send(actor_grabbing_all_keyboard_input_);
            } else {
                held_keys_.insert(key);
                held_keys_changed(context, auto_repeat, window);
            }
        },
        [=](key_up_atom, int key, const std::string &context, const std::string &window) {
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

        [=](register_hotkey_atom, const Hotkey &hk) {
            if (active_hotkeys_.find(hk.uuid()) != active_hotkeys_.end()) {

                // add a new watcher and/or update the hotkey sequence
                active_hotkeys_[hk.uuid()].update(hk);

            } else {

                active_hotkeys_[hk.uuid()] = hk;
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

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const error &err) mutable {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        });
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


        std::stringstream ss;
        for (const auto &k : held_keys_) {
            auto p = Hotkey::key_names.find(k);
            if (p != Hotkey::key_names.end()) {
                ss << p->second << " ";
            }
        }
        if (ss.str() != pressed_keys_string_) {
            pressed_keys_string_ = ss.str();
            mail(hotkey_event_atom_v, pressed_keys_string_).send(hotkey_config_events_group_);
        }

        for (auto &p : active_hotkeys_) {
            p.second.update_state(
                held_keys_, context, window, auto_repeat, caf::actor_cast<caf::actor>(this));
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
