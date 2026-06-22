// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <deque>
#include <map>
#include <optional>

#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace utility {

    template <typename V> class UndoRedo {
      public:
        UndoRedo() = default;
        UndoRedo(const utility::JsonStore &jsn);
        ~UndoRedo() = default;

        [[nodiscard]] utility::JsonStore serialise() const;
        [[nodiscard]] auto count() const { return undo_.size(); }
        [[nodiscard]] bool empty() const { return undo_.empty(); }

        void set_max_count(const size_t value);
        void push(const V &value);
        std::optional<V> undo();
        std::optional<V> redo();
        void clear();

      private:
        std::deque<V> undo_;
        std::deque<V> redo_;

        size_t max_count_{std::numeric_limits<size_t>::max()};
    };

    template <typename V> UndoRedo<V>::UndoRedo(const utility::JsonStore &jsn) {}

    template <typename V> utility::JsonStore UndoRedo<V>::serialise() const {
        utility::JsonStore jsn;


        return jsn;
    }

    template <typename V> void UndoRedo<V>::set_max_count(const size_t value) {
        max_count_ = value;
        while (undo_.size() > max_count_) {
            undo_.pop_back();
        }
    }

    template <typename V> void UndoRedo<V>::push(const V &value) {
        undo_.emplace_front(value);
        redo_.clear();
    }

    template <typename V> std::optional<V> UndoRedo<V>::undo() {
        if (undo_.empty())
            return {};

        auto v = undo_.front();
        undo_.pop_front();

        redo_.push_front(v);

        return v;
    }

    template <typename V> std::optional<V> UndoRedo<V>::redo() {
        if (redo_.empty())
            return {};

        auto v = redo_.front();
        redo_.pop_front();
        undo_.push_front(v);

        return v;
    }

    template <typename V> void UndoRedo<V>::clear() {
        undo_.clear();
        redo_.clear();
    }

    template <typename K, typename V> class UndoRedoMap {
      public:
        UndoRedoMap() = default;
        UndoRedoMap(const utility::JsonStore &jsn);
        ~UndoRedoMap() = default;

        [[nodiscard]] utility::JsonStore serialise() const;
        [[nodiscard]] auto count() const { return undo_.size(); }
        [[nodiscard]] bool empty() const { return undo_.empty(); }

        void set_max_count(const size_t value);
        void push(const K &key, const V &value);

        std::optional<V> undo();
        std::optional<V> redo();
        std::optional<V> undo(const K &key);
        std::optional<V> redo(const K &key);

        std::optional<K> peek_undo();
        std::optional<K> peek_redo();

        void clear();

      private:
        std::multimap<K, V> undo_;
        std::multimap<K, V> redo_;

        size_t max_count_{std::numeric_limits<size_t>::max()};
    };

    template <typename K, typename V>
    UndoRedoMap<K, V>::UndoRedoMap(const utility::JsonStore &jsn) {}

    template <typename K, typename V> utility::JsonStore UndoRedoMap<K, V>::serialise() const {
        utility::JsonStore jsn;
        return jsn;
    }

    template <typename K, typename V>
    void UndoRedoMap<K, V>::set_max_count(const size_t value) {
        max_count_ = value;
        while (undo_.size() > max_count_) {
            undo_.erase(undo_.begin());
        }
    }

    template <typename K, typename V>
    void UndoRedoMap<K, V>::push(const K &key, const V &value) {
        undo_.emplace(key, value);
        redo_.clear();
    }

    template <typename K, typename V> std::optional<K> UndoRedoMap<K, V>::peek_undo() {
        if (undo_.empty())
            return {};

        auto it = undo_.rbegin();
        return it->first;
    }

    template <typename K, typename V> std::optional<K> UndoRedoMap<K, V>::peek_redo() {
        if (redo_.empty())
            return {};

        auto it = redo_.begin();
        return it->first;
    }

    template <typename K, typename V> std::optional<V> UndoRedoMap<K, V>::undo() {
        if (undo_.empty())
            return {};

        auto it = undo_.rbegin();
        auto kv = *it;

        undo_.erase(std::next(it).base());
        redo_.insert(kv);

        return kv.second;
    }

    template <typename K, typename V> std::optional<V> UndoRedoMap<K, V>::redo() {
        if (redo_.empty())
            return {};

        auto it = redo_.begin();
        auto kv = *it;
        redo_.erase(it->first);

        undo_.insert(kv);

        return kv.second;
    }

    template <typename K, typename V> std::optional<V> UndoRedoMap<K, V>::undo(const K &max) {
        if (undo_.empty())
            return {};

        auto it = undo_.rbegin();

        if (max >= it->first)
            return {};

        auto kv = *it;

        undo_.erase(std::next(it).base());
        redo_.insert(kv);

        return kv.second;
    }

    template <typename K, typename V> std::optional<V> UndoRedoMap<K, V>::redo(const K &min) {
        if (redo_.empty())
            return {};

        // special handing required as duplicate key ordering is backwards..
        // the oldest is inserted first which means we'll get the wrong one back

        auto it = redo_.begin();

        if (min > it->first)
            return {};

        // if first entry has multiple identical keys
        // select the last one.
        auto key_count = redo_.count(it->first);
        if (key_count > 1)
            std::advance(it, key_count - 1);

        auto kv = *it;
        redo_.erase(it);

        undo_.insert(kv);

        return kv.second;
    }

    template <typename K, typename V> void UndoRedoMap<K, V>::clear() {
        undo_.clear();
        redo_.clear();
    }

} // namespace utility
} // namespace xstudio
