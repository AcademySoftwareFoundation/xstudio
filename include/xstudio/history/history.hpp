// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <utility>
#include <deque>
#include <limits>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/undo_redo.hpp"

namespace xstudio {
namespace history {

    template <typename V> class History : public utility::Container {
      public:
        History(const std::string &name = "History");
        History(const utility::JsonStore &jsn);
        ~History() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] auto count() const { return undo_redo_.count(); }

        void set_max_count(const size_t value) { undo_redo_.set_max_count(value); };

        void push(const V &value) { undo_redo_.push(value); }
        std::optional<V> undo() { return undo_redo_.undo(); }
        std::optional<V> redo() { return undo_redo_.redo(); }
        void clear() { undo_redo_.clear(); }
        [[nodiscard]] bool empty() const { return undo_redo_.empty(); }

        [[nodiscard]] bool enabled() const { return enabled_; }
        void set_enabled(const bool value) {
            enabled_ = value;
            if (not enabled_)
                undo_redo_.clear();
        }

      private:
        utility::UndoRedo<V> undo_redo_;
        bool enabled_{true};
    };

    template <typename V>
    History<V>::History(const std::string &name) : Container(name, "History") {}


    template <typename V>
    History<V>::History(const utility::JsonStore &jsn)
        : Container(static_cast<utility::JsonStore>(jsn["container"])),
          undo_redo_(jsn["undo_redo"]) {}

    template <typename V> utility::JsonStore History<V>::serialise() const {
        utility::JsonStore jsn;

        jsn["container"] = Container::serialise();
        jsn["undo_redo"] = undo_redo_.serialise();

        return jsn;
    }

    template <typename K, typename V> class HistoryMap : public utility::Container {
      public:
        HistoryMap(const std::string &name = "HistoryMap");
        HistoryMap(const utility::JsonStore &jsn);
        ~HistoryMap() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] auto count() const { return undo_redo_.count(); }

        void set_max_count(const size_t value) { undo_redo_.set_max_count(value); };

        void push(const K &key, const V &value) { undo_redo_.push(key, value); }

        std::optional<V> undo() { return undo_redo_.undo(); }
        std::optional<V> redo() { return undo_redo_.redo(); }
        std::optional<K> peek_undo() { return undo_redo_.peek_undo(); }
        std::optional<K> peek_redo() { return undo_redo_.peek_redo(); }
        std::optional<V> undo(const K &key) { return undo_redo_.undo(key); }
        std::optional<V> redo(const K &key) { return undo_redo_.redo(key); }
        void clear() { undo_redo_.clear(); }
        [[nodiscard]] bool empty() const { return undo_redo_.empty(); }

        [[nodiscard]] bool enabled() const { return enabled_; }
        void set_enabled(const bool value) {
            enabled_ = value;
            if (not enabled_)
                undo_redo_.clear();
        }

      private:
        utility::UndoRedoMap<K, V> undo_redo_;
        bool enabled_{true};
    };

    template <typename K, typename V>
    HistoryMap<K, V>::HistoryMap(const std::string &name) : Container(name, "HistoryMap") {}


    template <typename K, typename V>
    HistoryMap<K, V>::HistoryMap(const utility::JsonStore &jsn)
        : Container(static_cast<utility::JsonStore>(jsn["container"])),
          undo_redo_(jsn["undo_redo"]) {}

    template <typename K, typename V> utility::JsonStore HistoryMap<K, V>::serialise() const {
        utility::JsonStore jsn;

        jsn["container"] = Container::serialise();
        jsn["undo_redo"] = undo_redo_.serialise();

        return jsn;
    }
} // namespace history
} // namespace xstudio
