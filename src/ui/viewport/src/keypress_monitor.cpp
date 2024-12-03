// SPDX-License-Identifier: Apache-2.0
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

    set_down_handler([=](down_msg &msg) {
        auto p = std::find(
            actor_grabbing_all_mouse_input_.begin(),
            actor_grabbing_all_mouse_input_.end(),
            caf::actor_cast<caf::actor>(msg.source));
        if (p != actor_grabbing_all_mouse_input_.end()) {
            actor_grabbing_all_mouse_input_.erase(p);
        }

        if (msg.source == actor_grabbing_all_keyboard_input_) {
            actor_grabbing_all_keyboard_input_ = caf::actor();
        } else {
            caf::actor_addr w = caf::actor_cast<caf::actor_addr>(msg.source);
            for (auto &p : active_hotkeys_) {
                p.second.watcher_died(w);
            }
        }
    });

    behavior_.assign(
        [=](utility::get_event_group_atom) -> caf::actor { return keyboard_events_group_; },
        [=](utility::get_event_group_atom, hotkey_event_atom) -> caf::actor {
            return hotkey_config_events_group_;
        },
        [=](all_keys_up_atom, const std::string &context) {
            held_keys_.clear();
            held_keys_changed(context);
        },
        [=](key_down_atom,
            int key,
            const std::string &context,
            const std::string &window,
            const bool auto_repeat) {
            if (actor_grabbing_all_keyboard_input_) {
                anon_send(
                    actor_grabbing_all_keyboard_input_,
                    key_down_atom_v,
                    key,
                    context,
                    auto_repeat);
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
                anon_send(actor_grabbing_all_keyboard_input_, text_entry_atom_v, text, context);
            } else {
                send(keyboard_events_group_, text_entry_atom_v, text, context);
            }
        },
        [=](mouse_event_atom, const PointerEvent &e) {
            if (actor_grabbing_all_mouse_input_.size()) {
                anon_send(actor_grabbing_all_mouse_input_.front(), mouse_event_atom_v, e);
            } else {
                send(keyboard_events_group_, mouse_event_atom_v, e);
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

        [=](register_hotkey_atom, const Hotkey &hk) {
            if (active_hotkeys_.find(hk.uuid()) != active_hotkeys_.end()) {

                // add a new watcher and/or update the hotkey sequence
                active_hotkeys_[hk.uuid()].update(hk);

            } else {

                active_hotkeys_[hk.uuid()] = hk;
            }

            for (auto &p : hk.watchers()) {
                monitor(caf::actor_cast<caf::actor>(p));
            }

            std::vector<Hotkey> hks;
            for (const auto &p : active_hotkeys_)
                hks.push_back(p.second);
            send(hotkey_config_events_group_, hotkey_event_atom_v, active_hotkeys_[hk.uuid()]);
            send(hotkey_config_events_group_, hotkey_event_atom_v, hks);
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
            return make_error(xstudio_error::error, "Invalid hotkey uuid");
        },

        [=](keypress_monitor::hotkey_event_atom,
            const utility::Uuid kotkey_uuid,
            const bool pressed,
            const std::string &context,
            const std::string &window) {
            send(
                hotkey_config_events_group_,
                hotkey_event_atom_v,
                kotkey_uuid,
                pressed,
                context,
                window);
        },

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const error &err) mutable { aout(this) << err << std::endl; });
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
        for (auto &p : active_hotkeys_) {
            p.second.update_state(
                held_keys_, context, window, auto_repeat, caf::actor_cast<caf::actor>(this));
        }
    }
}
