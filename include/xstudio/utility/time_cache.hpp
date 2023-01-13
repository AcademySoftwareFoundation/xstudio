// SPDX-License-Identifier: Apache-2.0
// NOTES
// properties of the cache
// we store mixed frames as shared_ptrs (we only give out const to clients)
// we should limit on memory not the number of frames cached ?
// we either return a frame or request a frame from the media reader.
// we don't want to release frames that have been eager loaded (we'd end up reading the twice)
// allow cached frames to be manually expired. (watchout for shared frames).
// reuse buffers if they are a close match on size. NONONONONONO they are SHARED!!
// check ref COUNT ? IS THAT SAFE ? == 1 then no one is referencing it so we can reuse it..
// who knows how big the buffer should be ? the reader ?
// who handles eager loading ? the reader ?
// sort cache on expected presentation timestamp ?
// if the expired buffer is still in use remove ref and deduct from our budget.
// make a distinction from request and readahead hint.
// can we generalise the cache ?
// make the media cache a global actor
// but allow local cache actors.
// the same way we use the json_store/global jsonstore
// not sure the request to read a frame should go to the cache..
// seems like the reader should be the client...
#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace utility {
    template <typename K, typename V> class TimeCache {
      public:
        using cache_type     = std::map<K, V>;
        using uuid_type      = std::map<K, std::set<utility::Uuid>>;
        using timepoint_type = std::map<K, std::set<time_point>>;

        TimeCache(
            const size_t max_size  = std::numeric_limits<size_t>::max(),
            const size_t max_count = std::numeric_limits<size_t>::max());
        virtual ~TimeCache();

        bool store(
            const K &key,
            V value,
            const time_point &time    = utility::clock::now(),
            const bool force_eviction = false,
            const utility::Uuid &uuid = utility::Uuid());

        bool store(
            const K &key,
            V value,
            const time_point &time,
            const utility::Uuid &uuid,
            const time_point &out_of_date_time);

        bool store_check(
            const K &key,
            size_t size               = 0,
            const time_point &time    = utility::clock::now(),
            const bool force_eviction = false,
            const utility::Uuid &uuid = utility::Uuid());

        bool store_check(
            const K &key,
            size_t size,
            const utility::Uuid &uuid,
            const time_point &out_of_date_time);

        V retrieve(
            const K &key,
            const time_point &time    = utility::clock::now(),
            const utility::Uuid &uuid = utility::Uuid());

        V release(
            const time_point &ntp     = utility::clock::now(),
            const time_point &newtp   = utility::clock::now(),
            const bool force_eviction = false);

        V release_out_of_date(const time_point &out_of_date_time = utility::clock::now());

        V reuse(
            const size_t min_size,
            const time_point &ntp     = utility::clock::now(),
            const time_point &newtp   = utility::clock::now(),
            const bool force_eviction = false);

        bool preserve(
            const K &key,
            const time_point &time    = utility::clock::now(),
            const utility::Uuid &uuid = utility::Uuid());

        void make_entries_hot(const std::vector<std::pair<media::MediaKey, utility::time_point>>
                                  &keys_and_timepoints);

        void unpreserve(const utility::Uuid &uuid);

        void clear();
        bool erase(const K &key);
        bool erase(const utility::Uuid &uuid);
        bool erase(const K &key, const utility::Uuid &uuid);
        std::vector<K> erase(const std::vector<K> &key);

        typename cache_type::iterator erase(const typename cache_type::iterator &it);

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
        bool shrink(
            const size_t required_size,
            const size_t required_count,
            const time_point &time    = utility::clock::now(),
            const bool force_eviction = true);

        bool shrink_using_out_of_date(
            const size_t required_size,
            const size_t required_count,
            const time_point &out_of_date_time);

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

        void clean_timepoints(const K &key);
        void add_timepoint_reference(
            const K &key, const time_point &time, const utility::Uuid &uuid);
        void add_cache_entry(
            const K &key,
            V value,
            const time_point &time,
            const utility::Uuid &uuid,
            const size_t size);

        cache_type cache_;
        uuid_type uuid_cache_;
        timepoint_type timepoint_cache_;
        size_t max_size_;
        size_t max_count_;
        size_t size_{0};
        size_t count_{0};
    };

    template <typename K, typename V>
    TimeCache<K, V>::TimeCache(const size_t max_size, const size_t max_count)
        : max_size_(max_size), max_count_(max_count) {}

    template <typename K, typename V> TimeCache<K, V>::~TimeCache() = default;

    template <typename K, typename V>
    void TimeCache<K, V>::add_timepoint_reference(
        const K &key, const time_point &time, const utility::Uuid &uuid) {
        uuid_cache_[key].insert(uuid);
        if (timepoint_cache_.count(key))
            timepoint_cache_[key].insert(time);
        else
            timepoint_cache_[key] = std::set<time_point>{time};
    }

    template <typename K, typename V>
    void TimeCache<K, V>::add_cache_entry(
        const K &key,
        V value,
        const time_point &time,
        const utility::Uuid &uuid,
        const size_t size) {
        count_++;
        size_ += size;
        cache_[key] = value;
        call_change_callback({key}, {});

        uuid_cache_[key] = std::set<utility::Uuid>{};
        add_timepoint_reference(key, time, uuid);
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::store_check(
        const K &key,
        size_t size,
        const time_point &time,
        const bool force_eviction,
        const utility::Uuid &uuid) {

        // true if already cached or there is space
        if (not(force_eviction or (count_ < max_count_ and (size_ + size) < max_size_) or
                cache_.count(key))) {
            // can we make space.
            while (count_ > max_count_ - 1) {
                if (not release(utility::clock::now(), time, force_eviction))
                    return false;
            }

            while (size_ > max_size_ - size) {
                if (not release(utility::clock::now(), time, force_eviction))
                    return false;
            }
        }

        return true;
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::store_check(
        const K &key,
        size_t size,
        const utility::Uuid &uuid,
        const time_point &out_of_date_time) {

        // true if already cached or there is space
        if (not((count_ < max_count_ and (size_ + size) < max_size_) or cache_.count(key))) {
            // can we make space.
            while (count_ > max_count_ - 1) {
                if (not release_out_of_date(out_of_date_time))
                    return false;
            }

            while (size_ > max_size_ - size) {
                if (not release_out_of_date(out_of_date_time))
                    return false;
            }
        }

        return true;
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::store(
        const K &key,
        V value,
        const time_point &time,
        const bool force_eviction,
        const utility::Uuid &uuid) {
        // already got it..
        if (cache_.count(key)) {
            add_timepoint_reference(key, time, uuid);
            clean_timepoints(key);
        } else {
            size_t _size = (value ? value->size() : 0);

            if (not shrink(
                    (max_size_ > _size ? max_size_ - _size : 0),
                    max_count_ - 1,
                    time,
                    force_eviction))
                return false;

            add_cache_entry(key, value, time, uuid, _size);
        }
        return true;
    }


    template <typename K, typename V>
    bool TimeCache<K, V>::store(
        const K &key,
        V value,
        const time_point &time,
        const utility::Uuid &uuid,
        const time_point &out_of_date_time) {
        // already got it..
        if (cache_.count(key)) {
            add_timepoint_reference(key, time, uuid);
            clean_timepoints(key);
        } else {
            size_t _size = (value ? value->size() : 0);

            if (not shrink_using_out_of_date(
                    (max_size_ > _size ? max_size_ - _size : 0),
                    max_count_ - 1,
                    out_of_date_time))
                return false;

            add_cache_entry(key, value, time, uuid, _size);
        }
        return true;
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::shrink(
        const size_t required_size,
        const size_t required_count,
        const time_point &time,
        const bool force_eviction) {
        while (count_ > required_count) {
            if (not release(utility::clock::now(), time, force_eviction))
                return false;
        }

        while (size_ > required_size) {
            if (not release(utility::clock::now(), time, force_eviction))
                return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::shrink_using_out_of_date(
        const size_t required_size,
        const size_t required_count,
        const time_point &out_of_date_time) {
        while (count_ > required_count) {
            if (not release_out_of_date(out_of_date_time))
                return false;
        }

        while (size_ > required_size) {
            if (not release_out_of_date(out_of_date_time))
                return false;
        }
        return true;
    }

    template <typename K, typename V> void TimeCache<K, V>::clear() {
        call_change_callback({}, keys());

        cache_.clear();
        uuid_cache_.clear();
        timepoint_cache_.clear();
        count_ = 0;
        size_  = 0;
    }

    template <typename K, typename V> bool TimeCache<K, V>::erase(const K &key) {
        const auto it = cache_.find(key);
        if (it != std::end(cache_)) {
            erase(it);
            return true;
        }
        return false;
    }

    template <typename K, typename V>
    std::vector<K> TimeCache<K, V>::erase(const std::vector<K> &keys) {
        std::vector<K> result;
        for (const auto &i : keys) {
            if (erase(i))
                result.push_back(i);
        }
        return result;
    }

    template <typename K, typename V> bool TimeCache<K, V>::erase(const utility::Uuid &uuid) {
        auto it     = std::begin(cache_);
        bool result = false;
        while (it != std::end(cache_)) {
            if (uuid_cache_[it->first].count(uuid)) {
                uuid_cache_[it->first].erase(uuid);
                result = true;
                if (uuid_cache_[it->first].empty()) {
                    it = erase(it);
                    continue;
                }
            }
            ++it;
        }
        return result;
    }

    template <typename K, typename V>
    typename TimeCache<K, V>::cache_type::iterator
    TimeCache<K, V>::erase(const typename cache_type::iterator &it) {
        typename cache_type::iterator nit;
        if (it->second)
            size_ -= it->second->size();
        count_--;
        uuid_cache_.erase(uuid_cache_.find(it->first));
        timepoint_cache_.erase(timepoint_cache_.find(it->first));
        call_change_callback({}, {it->first});
        nit = cache_.erase(it);
        return nit;
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::erase(const K &key, const utility::Uuid &uuid) {
        const auto it = uuid_cache_.find(key);
        bool result   = false;
        if (it != std::end(uuid_cache_)) {
            // remove from set..
            it->second.erase(uuid);
            result = true;
            if (it->second.empty())
                erase(cache_.find(key));
        }
        return result;
    }

    // we must always release..
    // unless the size is set..
    // return empty buffer on fail / empty cache.
    template <typename K, typename V>
    V TimeCache<K, V>::release(
        const time_point &ntp, const time_point &newtp, const bool force_eviction) {

        V ptr;
        if (cache_.empty())
            return ptr;

        // release in special time order..
        long int max_offset(0);
        K key;

        // set to our proposed time, if we're bigger than every chache entry we fail.
        if (not force_eviction)
            max_offset = std::abs(
                std::chrono::duration_cast<std::chrono::microseconds>(ntp - newtp).count());

        for (const auto &i : timepoint_cache_) {
            long int min_offset(std::numeric_limits<long int>::max());

            // calculate offset in time from now,
            for (const auto &tp : i.second) {
                long int offset = std::abs(
                    std::chrono::duration_cast<std::chrono::microseconds>(ntp - tp).count());
                if (min_offset > offset)
                    min_offset = offset;
            }

            if (min_offset >= max_offset) {
                max_offset = min_offset;
                key        = i.first;
            }
        }
        // valid key ?
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            ptr                 = it->second;
            const auto &old_tps = timepoint_cache_[key];
            auto ms             = std::chrono::duration_cast<std::chrono::milliseconds>(
                          utility::clock::now() - *(old_tps.begin()))
                          .count();
            erase(it);
            // spdlog::debug("Release buffer {} which has timestamp {} milliseconds from
            // now", key, ms);
        }
        return ptr;
    }

    // release the oldest item that is older than out_of_date_time
    template <typename K, typename V>
    V TimeCache<K, V>::release_out_of_date(const time_point &out_of_date_time) {

        V ptr;
        if (cache_.empty())
            return ptr;

        K key;

        long int maxx = 0;
        std::vector<time_point> bb;
        for (const auto &i : timepoint_cache_) {

            // calculate offset in time from now,
            long int ctt = std::numeric_limits<long int>::max();
            std::vector<time_point> cc;
            for (const auto &tp : i.second) {
                ctt = std::min(
                    ctt,
                    static_cast<long int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              out_of_date_time - tp)
                                              .count()));
                cc.push_back(tp);
            }
            if (ctt > maxx) {
                maxx = ctt;
                key  = i.first;
                bb   = cc;
            }
        }
        // valid key ?
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            ptr = it->second;
            erase(it);
        }
        return ptr;
    }

    template <typename K, typename V>
    V TimeCache<K, V>::reuse(
        const size_t min_size,
        const time_point &ntp,
        const time_point &newtp,
        const bool force_eviction) {
        while (not cache_.empty() and
               ((min_size + size_) > max_size_ or (count_ + 1) > max_count_)) {
            V candidate = release(ntp, newtp, force_eviction);
            if (not candidate)
                break;
            if (candidate->size() >= min_size and candidate.use_count() == 1)
                return candidate;
        }
        return V();
    }

    template <typename K, typename V>
    V TimeCache<K, V>::retrieve(
        const K &key, const time_point &time, const utility::Uuid &uuid) {
        auto it = cache_.find(key);
        if (it == std::end(cache_))
            return V();
        // found entry, add timestamp (bit like lru ?)
        timepoint_cache_[key].insert(time);
        if (not uuid.is_null())
            uuid_cache_[key].insert(uuid);
        clean_timepoints(key);
        return it->second;
    }

    template <typename K, typename V> std::vector<K> TimeCache<K, V>::keys() const {
        std::vector<K> _keys;
        for (const auto &i : cache_)
            _keys.push_back(i.first);
        return _keys;
    }

    template <typename K, typename V>
    bool
    TimeCache<K, V>::preserve(const K &key, const time_point &time, const utility::Uuid &uuid) {
        if (retrieve(key, time, uuid))
            return true;

        return false;
    }

    template <typename K, typename V>
    void TimeCache<K, V>::make_entries_hot(
        const std::vector<std::pair<media::MediaKey, utility::time_point>>
            &keys_and_timepoints) {

        // expecting keys_and_timepoints to be ordered, earliest first. We take the
        // first entry and work out the time delta needed to shift its time point
        // to 'now' and we apply the same delta across the whole set, thus making
        // all these entries in the cache 'hot' ... i.e. we need them pretty soon
        // and they shouldn't be purged when new cache entries are inserted

        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            utility::clock::now() - keys_and_timepoints.front().second);

        for (const auto &key : keys_and_timepoints) {
            auto it = cache_.find(key.first);
            if (it != std::end(cache_)) {
                timepoint_cache_[key.first] = std::set<time_point>({key.second + delta});
            }
        }
    }

    template <typename K, typename V>
    void TimeCache<K, V>::unpreserve(const utility::Uuid &uuid) {
        // set the 'required by' time point on all cache entries that mathch uuid
        // backwards by one hour so that it can be dropped from the cache in
        // favour of new incoming data,
        auto it = std::begin(cache_);
        while (it != std::end(cache_)) {
            if (uuid_cache_[it->first].count(uuid) && timepoint_cache_.count(it->first)) {
                std::set<time_point> ntp;
                for (const auto &tp : timepoint_cache_[it->first]) {
                    ntp.insert(tp - std::chrono::hours(1));
                }
                timepoint_cache_[it->first] = ntp;
            }
            it++;
        }
    }
    // if they never expire the timepoint list can grow indefinitely..
    // remove timepoints older than now, but keep the most recent of these.
    template <typename K, typename V> void TimeCache<K, V>::clean_timepoints(const K &key) {
        const time_point now = utility::clock::now();
        time_point newest;

        for (auto i = std::begin(timepoint_cache_[key]);
             i != std::end(timepoint_cache_[key]);) {
            if (*i < now) {
                if (*i > newest)
                    newest = *i;
                i = timepoint_cache_[key].erase(i);
            } else
                ++i;
        }
        if (newest != time_point())
            timepoint_cache_[key].insert(newest);
    }
} // namespace utility
} // namespace xstudio
