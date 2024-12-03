// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>

#include "xstudio/ui/keyboard.hpp"

#include <caf/actor_system_config.hpp>
#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/type_id.hpp>

using namespace caf;

namespace xstudio {
namespace ui {
    namespace keypress_monitor {

        class KeypressMonitor : public caf::event_based_actor {
          public:
            KeypressMonitor(caf::actor_config &cfg);
            ~KeypressMonitor() override = default;
            void on_exit() override;

          private:
            caf::behavior make_behavior() override { return behavior_; }
            void held_keys_changed(
                const std::string &context,
                const bool auto_repeat    = false,
                const std::string &window = std::string());

          protected:
            caf::actor keyboard_events_group_, hotkey_config_events_group_;
            caf::behavior behavior_;
            std::set<int> held_keys_;
            std::map<utility::Uuid, ui::Hotkey> active_hotkeys_;
            std::vector<caf::actor> actor_grabbing_all_mouse_input_;
            caf::actor actor_grabbing_all_keyboard_input_;
        };
    } // namespace keypress_monitor
} // namespace ui
} // namespace xstudio
