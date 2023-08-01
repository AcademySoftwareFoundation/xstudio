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

    // This is a straight clone of the Qt::Key enums but instead we provide string
    // names for each key. The reason is that the actual key press event comes from
    // qt and we pass the qt key ID - here in xSTUDIO backend we don't want
    // any qt dependency hence this map.
    inline std::map<int, std::string> Hotkey::key_names = {
        {0x01000000, "Escape"},
        {0x01000001, "Tab"},
        {0x01000002, "Backtab"},
        {0x01000003, "Backspace"},
        {0x01000004, "Return"},
        {0x01000005, "Enter"},
        {0x01000006, "Insert"},
        {0x01000007, "Delete"},
        {0x01000008, "Pause"},
        {0x01000009, "Print"},
        {0x0100000a, "SysReq"},
        {0x0100000b, "Clear"},
        {0x01000010, "Home"},
        {0x01000011, "End"},
        {0x01000012, "Left"},
        {0x01000013, "Up"},
        {0x01000014, "Right"},
        {0x01000015, "Down"},
        {0x01000016, "PageUp"},
        {0x01000017, "PageDown"},
        {0x01000020, "Shift"},
        {0x01000021, "Control"},
        {0x01000022, "Meta"},
        {0x01000023, "Alt"},
        {0x01001103, "AltGr"},
        {0x01000024, "CapsLock"},
        {0x01000025, "NumLock"},
        {0x01000026, "ScrollLock"},
        {0x01000030, "F1"},
        {0x01000031, "F2"},
        {0x01000032, "F3"},
        {0x01000033, "F4"},
        {0x01000034, "F5"},
        {0x01000035, "F6"},
        {0x01000036, "F7"},
        {0x01000037, "F8"},
        {0x01000038, "F9"},
        {0x01000039, "F10"},
        {0x0100003a, "F11"},
        {0x0100003b, "F12"},
        {0x0100003c, "F13"},
        {0x0100003d, "F14"},
        {0x0100003e, "F15"},
        {0x20, "Space Bar"},
        {0x21, "Exclam"},
        {0x22, "\""},
        {0x23, "#"},
        {0x24, "$"},
        {0x25, "%"},
        {0x26, "&"},
        {0x27, "'"},
        {0x28, "("},
        {0x29, ")"},
        {0x2a, "*"},
        {0x2b, "+"},
        {0x2c, ","},
        {0x2d, "-"},
        {0x2e, "."},
        {0x2f, "/"},
        {0x30, "0"},
        {0x31, "1"},
        {0x32, "2"},
        {0x33, "3"},
        {0x34, "4"},
        {0x35, "5"},
        {0x36, "6"},
        {0x37, "7"},
        {0x38, "8"},
        {0x39, "9"},
        {0x3a, ":"},
        {0x3b, ";"},
        {0x3c, "<"},
        {0x3d, "="},
        {0x3e, ">"},
        {0x3f, "?"},
        {0x40, "@"},
        {0x41, "A"},
        {0x42, "B"},
        {0x43, "C"},
        {0x44, "D"},
        {0x45, "E"},
        {0x46, "F"},
        {0x47, "G"},
        {0x48, "H"},
        {0x49, "I"},
        {0x4a, "J"},
        {0x4b, "K"},
        {0x4c, "L"},
        {0x4d, "M"},
        {0x4e, "N"},
        {0x4f, "O"},
        {0x50, "P"},
        {0x51, "Q"},
        {0x52, "R"},
        {0x53, "S"},
        {0x54, "T"},
        {0x55, "U"},
        {0x56, "V"},
        {0x57, "W"},
        {0x58, "X"},
        {0x59, "Y"},
        {0x5a, "Z"},
        {0x5b, "["},
        {0x5c, "\\"},
        {0x5d, "]"},
        {0x5f, "_"},
        {0x60, "`"},
        {0x7b, "{"},
        //{0x7c
        {0x7d, "}"},
        {0x7e, "~"},
        {93, "numpad 0"},
        {96, "numpad 1"},
        {97, "numpad 2"},
        {98, "numpad 3"},
        {99, "numpad 4"},
        {100, "numpad 5"},
        {101, "numpad 6"},
        {102, "numpad 7"},
        {103, "numpad 8"},
        {104, "numpad 9"},
        {105, "numpad multiply"},
        {106, "numpad add"},
        {107, "numpad subtract"},
        {109, "numpad decimal point"},
        {110, "numpad divide"}};

} // namespace ui
} // namespace xstudio