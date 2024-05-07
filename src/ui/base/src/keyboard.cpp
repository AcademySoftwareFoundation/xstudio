// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/keyboard.hpp"
#include "xstudio/atoms.hpp"

/* forward declaration of this function from tessellation_helpers.cpp */
using namespace xstudio::ui;

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
