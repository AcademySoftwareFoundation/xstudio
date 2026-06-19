// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/keyboard.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/string_helpers.hpp"

/* forward declaration of this function from tessellation_helpers.cpp */
using namespace xstudio::ui;

Hotkey::Hotkey(
    const int k,
    const int mod,
    const std::string name,
    const std::string component,
    const std::string description,
    const std::string window_name,
    const bool auto_repeat,
    caf::actor_addr watcher,
    const utility::Uuid uuid)
    : default_key_(k),
      default_modifiers_(mod),
      key_(k),
      modifiers_(mod),
      uuid_(
          uuid.is_null() ? utility::Uuid::generate_from_name((name + component).c_str())
                         : uuid),
      name_(name),
      component_(component),
      description_(description),
      auto_repeat_(auto_repeat) {

    if (watcher)
        watchers_.emplace_back(watcher);
}

bool Hotkey::update(const Hotkey &o, const bool update_sequence) {
    bool changed = o.key_ != key_ || o.modifiers_ != modifiers_;
    if (changed && update_sequence) {
        key_         = o.key_;
        modifiers_   = o.modifiers_;
        auto_repeat_ = o.auto_repeat_;
    }
    for (const auto &p : o.watchers_) {
        bool match = false;
        for (auto &q : watchers_) {
            if (q == p) {
                match = true;
                break;
            }
        }
        if (!match) {
            watchers_.push_back(p);
        }
    }
    return changed;
}

bool Hotkey::update(const int new_key, const int new_modifiers) {
    bool changed = new_key != key_ || new_modifiers != modifiers_;
    if (changed) {
        key_       = new_key;
        modifiers_ = new_modifiers;
    }
    return changed;
}

void Hotkey::update_state(
    const std::set<int> &current_keys,
    const std::string &context,
    const std::string &window,
    const bool auto_repeat,
    caf::actor keypress_monitor,
    const bool due_to_focus_change) {

    // context tells us where the hotkey interaction happened (e.g. 'main_viewport')
    // If this hotkey has a context specifier that doesn't match then we don't
    // update the hotkey state.

    int mod = 0;
    bool kp = false;

    for (auto &k : current_keys) {
        if (k == key_) {
            kp = true;
        }
        auto r = Hotkey::key_to_modifier.find(k);
        if (r != Hotkey::key_to_modifier.end()) {
            mod |= r->second;
        }
    }

    if (mod == modifiers_ && kp == true) {

        if (!pressed_) {

            pressed_ = true;
            notify(context, window, keypress_monitor, due_to_focus_change);

        } else if (auto_repeat && auto_repeat_) {
            notify(context, window, keypress_monitor, due_to_focus_change);
        }
    } else if (pressed_) {
        pressed_ = false;
        notify(context, window, keypress_monitor, due_to_focus_change);
    }
}

void Hotkey::notify(
    const std::string &context,
    const std::string &window,
    caf::actor keypress_monitor,
    const bool due_to_focus_change) {


    auto p = exclusive_watchers_.begin();
    while (p != exclusive_watchers_.end()) {
        auto exclusive = caf::actor_cast<caf::actor>(*p);
        if (exclusive) {
            // we've found a valid 'exclusive' hotkey watcher
            anon_mail(
                keypress_monitor::hotkey_event_atom_v,
                uuid_,
                pressed_,
                context,
                window,
                due_to_focus_change)
                .send(exclusive);
            return;
        } else {
            // the exclusive watcher must have exited without telling us!
            p = exclusive_watchers_.erase(p);
        }
    }

    // no exclusive watchers, broadcast hotkey event to everyone!
    notify_watchers(context, window, due_to_focus_change);
    anon_mail(
        keypress_monitor::hotkey_event_atom_v,
        uuid_,
        pressed_,
        context,
        window,
        due_to_focus_change)
        .send(keypress_monitor);
}

void Hotkey::exclusive_watcher(caf::actor_addr w, bool watch) {

    auto p = exclusive_watchers_.begin();
    while (p != exclusive_watchers_.end()) {
        if (*p == w) {
            p = exclusive_watchers_.erase(p);
        } else {
            p++;
        }
    }

    if (watch) {
        exclusive_watchers_.insert(exclusive_watchers_.begin(), w);
    }
}

void Hotkey::watcher_died(caf::actor_addr &watcher) {
    auto p = watchers_.begin();
    while (p != watchers_.end()) {
        if (*p == watcher) {
            p = watchers_.erase(p);
        } else {
            p++;
        }
    }
}

void Hotkey::add_watcher(caf::actor_addr w) {

    auto p = watchers_.begin();
    while (p != watchers_.end()) {
        if (*p == w) {
            return;
        }
        p++;
    }
    watchers_.push_back(w);
}


void Hotkey::notify_watchers(
    const std::string &context, const std::string &window, const bool due_to_focus_change) {
    auto p = watchers_.begin();
    while (p != watchers_.end()) {
        auto a = caf::actor_cast<caf::actor>(*p);
        if (!a) {
            p = watchers_.erase(p); // the 'watcher' must have closed down - let's remove it
        } else {
            anon_mail(
                keypress_monitor::hotkey_event_atom_v,
                uuid_,
                pressed_,
                context,
                window,
                due_to_focus_change)
                .send(a);
            p++;
        }
    }
}

const std::string Hotkey::key() const {
    if (key_ == -1) {
        return "";
    }
    std::array<char, 2> k{(char)key_, 0};
    return std::string(k.data());
}

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
#ifdef __apple__
        r = "Ctrl+" + r;
#else
        r = "Meta+" + r;
#endif
    }
    if ((modifiers_ & AltModifier) == AltModifier) {
#ifdef __apple__
        r = "Opt+" + r;
#else
        r = "Alt+" + r;
#endif
    }
    if ((modifiers_ & ControlModifier) == ControlModifier) {
#ifdef __apple__
        r = "Cmd+" + r;
#else
        r = "Ctrl+" + r;
#endif
    }
    return r;
}

void Hotkey::sequence_to_key_and_modifier(
    const std::string &sequence, int &keycode, int &modifier) {

    std::vector<std::string> seq = utility::split(sequence, '+');
    modifier                     = 0;
    keycode                      = -1;
    for (const auto &p : seq) {
        const std::string D = utility::replace_all(p, " ", "");
        const std::string d = utility::to_lower(D);
        if (d == "shift") {
            modifier |= ShiftModifier;
        } else if (d == "meta") {
            modifier |= MetaModifier;
        } else if (d == "alt") {
            modifier |= AltModifier;
        } else if (d == "ctrl" || d == "control") {
            modifier |= ControlModifier;
        } else {
            for (const auto &q : ui::Hotkey::key_names) {
                if (q.second == D) {
                    keycode = q.first;
                    break;
                }
            }
        }
    }
    if (keycode == -1 and not modifier) {
        throw std::runtime_error(
            fmt::format("Unable to identify key name in hotkey sequence '{}'", sequence)
                .c_str());
    }
}

void Hotkey::sequence_to_key_and_modifier(
    const std::vector<std::string> &seq, int &keycode, int &modifier) {

    modifier = 0;
    keycode  = -1;

    if (seq.size() == 1 && seq[0] == "") {
        // 'unbound' hotkeyt case
        return;
    }

    for (const auto &p : seq) {
        const std::string d = utility::to_lower(p);
        if (d == "shift") {
            modifier |= ShiftModifier;
        } else if (d == "meta") {
            modifier |= MetaModifier;
        } else if (d == "alt") {
            modifier |= AltModifier;
        } else if (d == "ctrl" || d == "control") {
            modifier |= ControlModifier;
        } else {
            for (const auto &q : ui::Hotkey::key_names) {
                if (q.second == p) {
                    keycode = q.first;
                    break;
                }
            }
        }
    }
    if (keycode == -1 and not modifier) {
        std::string ss;
        for (const auto &s : seq) {
            if (!ss.empty()) {
                ss += "+";
            }
            ss += s;
        }
        throw std::runtime_error(
            fmt::format("Unable to identify key name in hotkey sequence '{}'", ss).c_str());
    }
}