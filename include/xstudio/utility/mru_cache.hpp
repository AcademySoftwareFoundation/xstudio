// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

namespace xstudio {
namespace utility {
    template <typename K, typename V> class MRUCache {
      public:
        using cache_type = std::map<K, std::shared_ptr<V>>;
        using list_type  = std::list<typename cache_type::iterator>;

        MRUCache(
            const size_t max_size  = std::numeric_limits<size_t>::max(),
            const size_t max_count = std::numeric_limits<size_t>::max());
        virtual ~MRUCache();

        bool store(const K &key, std::shared_ptr<V> value);

        std::shared_ptr<V> retrieve(const K &key);

        std::shared_ptr<V> release();

        bool preserve(const K &key);
        void preserve(const typename cache_type::iterator &it);

        void clear();
        bool erase(const K &key);
        std::vector<K> erase(const std::vector<K> &key);

        typename cache_type::iterator erase(const typename cache_type::iterator &it);

        [[nodiscard]] bool empty() const { return count_ == 0; }
        [[nodiscard]] size_t size() const { return size_; }
        [[nodiscard]] size_t count() const { return count_; }

        [[nodiscard]] size_t max_size() const { return max_size_; }
        [[nodiscard]] size_t max_count() const { return max_count_; }

        void set_max_size(const size_t max_size) {
            max_size_ = max_size;
            shrink(max_size_, max_count_);
        }
        void set_max_count(const size_t max_count) {
            max_count_ = max_count;
            shrink(max_size_, max_count_);
        }
        bool shrink(const size_t required_size, const size_t required_count);

        [[nodiscard]] std::vector<K> keys() const;

        void bind_change_callback(
            std::function<void(const std::vector<K> &store, const std::vector<K> &erase)> fn) {
            change_callback_ = [fn](auto &&PH1, auto &&PH2) {
                return fn(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
            };
        }

        void call_change_callback(const std::vector<K> &store, const std::vector<K> &erase) {
            if (change_callback_)
                change_callback_(store, erase);
        }

      private:
        std::function<void(const std::vector<K> &store, const std::vector<K> &erase)>
            change_callback_;

        cache_type cache_;
        list_type mru_;
        size_t max_size_;
        size_t max_count_;
        size_t size_{0};
        size_t count_{0};
    };

    template <typename K, typename V>
    MRUCache<K, V>::MRUCache(const size_t max_size, const size_t max_count)
        : max_size_(max_size), max_count_(max_count) {}

    template <typename K, typename V> MRUCache<K, V>::~MRUCache() = default;

    template <typename K, typename V>
    bool MRUCache<K, V>::store(const K &key, std::shared_ptr<V> value) {
        // already got it..
        if (cache_.count(key)) {
            // bump to top of list.. ?
            preserve(key);
        } else {
            size_t _size = 0;
            if (value)
                _size = value->size();

            auto shrunk = shrink((max_size_ > _size ? max_size_ - _size : 0), max_count_ - 1);

            if (not shrunk)
                return false;

            count_++;
            size_ += _size;
            auto result = cache_.insert(std::make_pair(key, value));
            mru_.emplace_front(result.first);
            call_change_callback({key}, {});
        }
        return true;
    }

    template <typename K, typename V>
    bool MRUCache<K, V>::shrink(const size_t required_size, const size_t required_count) {
        while (count_ > required_count) {
            release();
        }

        while (size_ > required_size) {
            release();
        }
        return true;
    }

    template <typename K, typename V> void MRUCache<K, V>::clear() {
        call_change_callback({}, keys());

        cache_.clear();
        mru_.clear();
        count_ = 0;
        size_  = 0;
    }

    template <typename K, typename V> bool MRUCache<K, V>::erase(const K &key) {
        const auto it = cache_.find(key);
        if (it != std::end(cache_)) {
            erase(it);
            return true;
        }
        return false;
    }

    template <typename K, typename V>
    std::vector<K> MRUCache<K, V>::erase(const std::vector<K> &keys) {
        std::vector<K> result;
        for (const auto &i : keys) {
            if (erase(i))
                result.push_back(i);
        }
        return result;
    }

    template <typename K, typename V>
    typename MRUCache<K, V>::cache_type::iterator
    MRUCache<K, V>::erase(const typename cache_type::iterator &it) {
        typename cache_type::iterator nit;
        if (it->second)
            size_ -= it->second->size();
        count_--;
        call_change_callback({}, {it->first});
        // find in mru_
        mru_.remove(it); // is iterator equivalent ?
        nit = cache_.erase(it);

        assert(mru_.size() == cache_.size());
        return nit;
    }

    // we must always release..
    // unless the size is set..
    // return empty buffer on fail / empty cache.
    template <typename K, typename V> std::shared_ptr<V> MRUCache<K, V>::release() {
        std::shared_ptr<V> ptr;
        if (cache_.empty())
            return ptr;

        auto it = mru_.back();
        ptr     = it->second;
        erase(it);
        return ptr;
    }

    template <typename K, typename V>
    std::shared_ptr<V> MRUCache<K, V>::retrieve(const K &key) {
        auto it = cache_.find(key);
        if (it == std::end(cache_))
            return std::shared_ptr<V>();

        preserve(it);

        return it->second;
    }

    template <typename K, typename V> std::vector<K> MRUCache<K, V>::keys() const {
        std::vector<K> _keys;
        for (const auto &i : cache_)
            _keys.push_back(i.first);
        return _keys;
    }

    template <typename K, typename V>
    void MRUCache<K, V>::preserve(const typename cache_type::iterator &it) {
        mru_.remove(it);
        mru_.push_front(it);
    }


    template <typename K, typename V> bool MRUCache<K, V>::preserve(const K &key) {
        auto it = cache_.find(key);

        if (it == std::end(cache_))
            return false;

        preserve(it);

        return true;
    }

} // namespace utility
} // namespace xstudio
