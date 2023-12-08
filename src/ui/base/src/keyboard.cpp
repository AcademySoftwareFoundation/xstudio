// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/keyboard.hpp"
#include "xstudio/atoms.hpp"

/* forward declaration of this function from tessellation_helpers.cpp */
using namespace xstudio::ui;

// This is a straight clone of the Qt::Key enums but instead we provide string
// names for each key. The reason is that the actual key press event comes from
// qt and we pass the qt key ID - here in xSTUDIO backend we don't want
// any qt dependency hence this map.
std::map<int, std::string> Hotkey::key_names = {
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
    {0x60, "§"},
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

Hotkey::Hotkey(
    const int k,
    const int mod,
    const std::string name,
    const std::string component,
    const std::string description,
    const std::string context,
    const bool auto_repeat,
    caf::actor_addr watcher)
    : key_(k),
      modifiers_(mod),
      uuid_(utility::Uuid::generate_from_name((name + component).c_str())),
      name_(name),
      component_(component),
      description_(description),
      context_(context),
      auto_repeat_(auto_repeat) {

    if (watcher)
        watchers_.emplace_back(watcher, context);
}

bool Hotkey::update(const Hotkey &o) {
    bool changed = o.key_ != key_ || o.modifiers_ != modifiers_;
    key_         = o.key_;
    modifiers_   = o.modifiers_;
    auto_repeat_ = o.auto_repeat_;
    for (const auto &p : o.watchers_) {
        bool match = false;
        for (auto &q : watchers_) {
            if (q.first == p.first) {
                q.second = p.second;
                match    = true;
                break;
            }
        }
        if (!match) {
            watchers_.push_back(p);
        }
    }
    return changed;
}

void Hotkey::update_state(
    const std::set<int> &current_keys, const std::string &context, const bool auto_repeat) {

    int mod = 0;
    bool kp = false;

    for (auto &k : current_keys) {
        if (k == key_) {
            kp = true;
        }
        auto r = key_to_modifier.find(k);
        if (r != key_to_modifier.end()) {
            mod |= r->second;
        }
    }

    if (mod == modifiers_ && kp == true) {
        if (!pressed_) {
            pressed_ = true;
            notifty_watchers(context);
        } else if (auto_repeat && auto_repeat_) {
            notifty_watchers(context);
        }
    } else if (pressed_) {
        pressed_ = false;
        notifty_watchers(context);
    }
}

void Hotkey::notifty_watchers(const std::string &context) {
    auto p = watchers_.begin();
    while (p != watchers_.end()) {
        auto a = caf::actor_cast<caf::actor>((*p).first);
        if (!a) {
            p = watchers_.erase(p); // the 'watcher' must have closed down - let's remove it
        } else if (context == (*p).second || (*p).second == "any" || (*p).second.empty()) {
            anon_send(a, keypress_monitor::hotkey_event_atom_v, uuid_, pressed_, context);
            p++;
        } else {
            p++;
        }
    }
}

const std::string Hotkey::key() const {
    std::array<char, 2> k{(char)key_, 0};
    return std::string(k.data());
}


void Hotkey::add_watcher(caf::actor_addr watcher) { watchers_.emplace_back(watcher, context_); }


std::string Hotkey::hotkey_sequence() const {

    std::string r;
    const auto p = key_names.find(key_);
    if (p != key_names.cend()) {
        r = p->second;
    } else {
        const auto p2 = key_names.find(key_ & 127);
        if (p2 != key_names.cend()) {
            r = p2->second;
        } else {
            std::array<char, 2> b{(char)(key_ & 127), 0};
            r = b.data();
        }
    }

    if ((modifiers_ & ShiftModifier) == ShiftModifier) {
        r = "Shift+" + r;
    }
    if ((modifiers_ & MetaModifier) == MetaModifier) {
        r = "Meta+" + r;
    }
    if ((modifiers_ & AltModifier) == AltModifier) {
        r = "Alt+" + r;
    }
    if ((modifiers_ & ControlModifier) == ControlModifier) {
        r = "Ctrl+" + r;
    }
    return r;
}
