// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>

#include "xstudio/ui/keyboard.hpp"

#ifdef __apple__
#undef nil
#endif

#include <caf/actor_system_config.hpp>
#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/type_id.hpp>

using namespace caf;

namespace xstudio::ui::keypress_monitor {

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
    void all_keys_up(const std::string &context);
    void apply_hotkey_overrides(const utility::Uuid &hotkey_id = utility::Uuid());

  protected:
    caf::actor keyboard_events_group_, hotkey_config_events_group_;
    caf::behavior behavior_;
    std::set<int> held_keys_, previous_held_keys_;
    std::map<int, std::string> extra_key_names_;
    std::map<utility::Uuid, ui::Hotkey> active_hotkeys_;
    std::vector<caf::actor> actor_grabbing_all_mouse_input_;
    caf::actor actor_grabbing_all_keyboard_input_;
    utility::JsonStore hotkey_overrides_;
    bool overrides_changed_ = false;
};
} // namespace xstudio::ui::keypress_monitor
