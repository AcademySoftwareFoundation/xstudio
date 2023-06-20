// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/uuid.hpp"
#include <set>
#include <caf/actor.hpp>
namespace xstudio {
namespace ui {

    enum KeyboardModifier {
        NoModifier          = 0x0,
        ShiftModifier       = 1 << 0,
        ControlModifier     = 1 << 1,
        AltModifier         = 1 << 2,
        MetaModifier        = 1 << 3,
        KeypadModifier      = 1 << 4,
        GroupSwitchModifier = 1 << 5,
        ZoomActionModifier  = 1 << 6,
        PanActionModifier   = 1 << 7
    };

    inline static const std::map<int, int> key_to_modifier = {
        {16777248, (int)ShiftModifier},
        {16777249, (int)ControlModifier},
        {16777251, (int)AltModifier},
        {16777249, (int)MetaModifier}};

    // This is very rough and ready, needs a lot more work!
    class Hotkey {

      public:
        Hotkey(
            const int k,
            const int mod,
            const std::string name,
            const std::string component,
            const std::string description,
            const std::string context,
            const bool auto_repeat,
            caf::actor_addr watcher);

        Hotkey(const Hotkey &o) = default;
        Hotkey()                = default;

        void add_watcher(caf::actor_addr);

        [[nodiscard]] const std::string &hotkey_name() const { return name_; }
        [[nodiscard]] const std::string &hotkey_origin() const { return component_; }
        [[nodiscard]] const std::string &hotkey_description() const { return description_; }
        [[nodiscard]] const std::string key() const;
        [[nodiscard]] int modifiers() const { return modifiers_; }
        [[nodiscard]] const utility::Uuid &uuid() const { return uuid_; }
        [[nodiscard]] std::string hotkey_sequence() const;

        bool update(const Hotkey &o);

        void update_state(
            const std::set<int> &current_keys,
            const std::string &context,
            const bool auto_repeat);

        static std::map<int, std::string> key_names;

      private:
        void notifty_watchers(const std::string &context);

        int key_       = 0;
        int modifiers_ = 0;
        bool pressed_  = false;
        utility::Uuid uuid_;
        std::string name_;
        std::string component_;
        std::string description_;
        std::string context_;
        bool auto_repeat_;
        std::vector<std::pair<caf::actor_addr, std::string>> watchers_;
    };

} // namespace ui
} // namespace xstudio