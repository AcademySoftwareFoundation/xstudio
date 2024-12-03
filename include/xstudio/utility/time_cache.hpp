// SPDX-License-Identifier: Apache-2.0
// NOTES

// TimeCache class is used to manage storage of ImageBuffers, AudioBuffers and
// ColourPipelineData - the MediaCacheActor owns the Class and provides access
// to the cache.
// When the cache is full (the size of the buffers exceeds the size set on
// the cache), buffers (values) are discarded to make space for new buffers.
// We do this by checking timestamps that store when a buffer will be needed
// in the near future. If the timestamps are in the past the buffer can be 
// discarded.
// The playheads will continually notify the cache which buffers that are going
// to need in the coming few seconds for playback, and the cache updates the
// timepoints accordingly.
// 
// Note that it's possible for a buffer to be needed 0.1s and again 2.0s in the
// future, say. Once we have advanced > 0.1s, then the other timepoint at 2.0s 
// becomes relevant in terms of retaining the buffer when we need to make more
// space.
//
// Sometimes, if we have a buffer stored that's needed in 10.0s and we need to
// store a new buffer needed in 1.0s, then we might need to delete the buffer
// stored for 10s in favour of the buffer needed more imminently. This is the
// reason for storing a set of timepoints telling us when buffers are needed
// rather than a single timepoitn.
// 
// We also store Uuids against the cache entries. These uuids tell us which 
// object is 'insterested' in that cache entry. For example, a SubPlayhead X154
// might be requesting reads for a particular media item and then retrieveing the
// buffers from the cache during display. When the user switches to view another
// piece of media it means the SubPlayhead X154 will be destroyed. It tells the 
// MediaReader, which then checks the cache - if the only Uuid stored with the
// buffers is X154 then the buffers are considered safe for removal as they will
// no longer be needed for display.
//
// Final note: We use std::map, not std::unsorted_map. Although advice is that
// std::map should not be used for high performance applications, we found that
// lookups and insertions are much quicker for std::map. It might be becuase
// our cache is usually in the order of a few hundred or at most a few thousand
// entries and binary lookup of std::map is superiour at these small sizes.

// The goal of the cache is to keep values that we need and 
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
#include "xstudio/media/media.hpp"

namespace xstudio {
namespace utility {
    template <typename K, typename V> class TimeCache {
      public:

        struct CacheEntry {
            CacheEntry() = default;
            CacheEntry(const V _v) : value(_v) {}
            V value;
            std::unordered_set<utility::Uuid> uuids;
            std::set<time_point> timepoints;
        };
        typedef std::shared_ptr<CacheEntry> CacheEntryPtr;

        using cache_type     = std::map<K, CacheEntryPtr>;
        cache_type cache_;

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
        [[nodiscard]] bool empty() const { return count_ == 0; }


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

        /*bool noisy = {false};
        std::vector<int> retrieve_times;
        size_t avg_sum{0};
        size_t avg_ct{0};*/

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
        auto it = cache_.find(key);
        it->second->uuids.insert(uuid);
        it->second->timepoints.insert(time);
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

        cache_[key] = std::make_shared<CacheEntry>(value);

        call_change_callback({key}, {});

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
        result.reserve(keys.size());
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
            if (it->second->uuids.count(uuid)) {
                it->second->uuids.erase(uuid);
                result = true;
                if (it->second->uuids.empty()) {
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
            size_ -= it->second->value->size();
        count_--;
        call_change_callback({}, {it->first});
        nit = cache_.erase(it);
        return nit;
    }

    template <typename K, typename V>
    bool TimeCache<K, V>::erase(const K &key, const utility::Uuid &uuid) {
        const auto it = cache_.find(key);
        bool result   = false;
        if (it != std::end(cache_)) {
            // remove from set..
            if (it->second->uuids.count(uuid)) {
                it->second->uuids.erase(uuid);
                result = true;
                if (it->second->uuids.empty()) {
                    erase(it);
                }
            }
        }
        return result;
    }

    // The goal here is to find the cache entry that has the oldest timepoints
    // in the whole set, and remove it. Remember, timepoint is the estimated
    // time when the given data entry will be needed. If the timepoints tell us
    // it was needed in the past, then we can safely remove. If it's in the 
    // future, and inside 'newtp' then we can't get rid of it as we will need 
    // it soon so we have a release failure.
    // return empty buffer on fail / empty cache.
    template <typename K, typename V>
    V TimeCache<K, V>::release(
        const time_point &ntp, const time_point &newtp, const bool force_eviction) {

        V ptr;
        if (cache_.empty())
            return ptr;

        // release in special time order..
        long int max_offset(0);

        typename cache_type::iterator it = cache_.end();
        typename cache_type::iterator i = cache_.begin();

        // set to our proposed time, if we're bigger than every chache entry we fail.
        if (not force_eviction)
            max_offset = std::abs(
                std::chrono::duration_cast<std::chrono::microseconds>(ntp - newtp).count());

        // loop over every cache entry
        while (i != cache_.end()) {

            const std::set<time_point> & timepoints = i->second->timepoints;

            // timepoints is an ordered set, so the last entry is the timepoint
            // furthest forward in time. How does this timepoint (which represents
            // the furthest point in the future that we would need this Value)
            // compare to 'max_offset' ?
            if (timepoints.size()) {
                long int offset = std::abs(
                    std::chrono::duration_cast<std::chrono::microseconds>(ntp - *(timepoints.rbegin())).count());
                if (offset >= max_offset) {
                    max_offset = offset;
                    it        = i;
                }
            }
            i++;

        }
        if (it != cache_.end()) {
            ptr = it->second->value;
            erase(it);
        }
        return ptr; 

    }

    // release the oldest item that is older than out_of_date_time
    template <typename K, typename V>
    V TimeCache<K, V>::release_out_of_date(const time_point &out_of_date_time) {

        V ptr;
        if (cache_.empty())
            return ptr;

        typename cache_type::iterator it = cache_.end();
        typename cache_type::iterator i = cache_.begin();

        long int maxx = 0;

        // loop over every cache entry
        while (i != cache_.end()) {

            const std::set<time_point> & timepoints = i->second->timepoints;

            // timepoints is an ordered set, so the last entry is the timepoint
            // furthest forward in time. How does this timepoint (which represents
            // the furthest point in the future that we would need this Value)
            // compare to 'max_offset' ?
            if (timepoints.size()) {
                long int offset = std::abs(
                    std::chrono::duration_cast<std::chrono::microseconds>(out_of_date_time - *(timepoints.rbegin())).count());
                if (offset >= maxx) {
                    maxx = offset;
                    it = i;
                }
            }
            ++i;

        }
        // valid key ?
        if (it != cache_.end()) {
            ptr = it->second->value;
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
        auto tp = utility::clock::now();
        auto it = cache_.find(key);
        if (it == std::end(cache_))
            return V();
        // found entry, add timestamp (bit like lru ?)
        it->second->timepoints.insert(time);
        if (not uuid.is_null())
            it->second->uuids.insert(uuid);
        clean_timepoints(key);

        /*if (noisy){
            retrieve_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(utility::clock::now()-tp).count());
            if (retrieve_times.size() == 1024) {
                int _max = 0;
                int _min = 100000000;
                int sum = 0;
                for (auto &t: retrieve_times) {
                    sum += t;
                    _max=std::max(t, _max);
                    _min=std::min(t, _min);
                }
                std::sort(retrieve_times.begin(), retrieve_times.end());
                int last_ten = 0;
                for (int i = 924; i < 1024; ++i) {
                    last_ten += retrieve_times[i];
                }
                avg_sum += sum;
                avg_ct += 1024;
                std::cerr << this << " Cache Size: " << cache_.size() << "  Min retrieve: " << _min << ", Max retrieve: " << _max << ", Average: " << sum/1024 << " (mean), " << retrieve_times[512] << " (median), " << " worst 100: " << last_ten/100 << "   Running average " << avg_sum/avg_ct << "\n";
                retrieve_times.clear();
            }
        }*/

        return it->second->value;
    }

    template <typename K, typename V> std::vector<K> TimeCache<K, V>::keys() const {
        std::vector<K> _keys;
        _keys.reserve(cache_.size());
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
                it->second->timepoints = std::set<time_point>({key.second + delta});
            }
        }
    }

    template <typename K, typename V>
    void TimeCache<K, V>::unpreserve(const utility::Uuid &uuid) {
        // set the 'required by' time point on all cache entries that mathch uuid
        // backwards by one hour so that it can be dropped from the cache in
        // favour of new incoming data,
        typename cache_type::iterator it = cache_.begin();
        while (it != cache_.end()) {
            if (it->second->uuids.count(uuid)) {
                std::set<time_point> new_timepoints;
                std::set<time_point> &timepoints = it->second->timepoints;
                for (const auto &tp : timepoints) {
                    new_timepoints.emplace(tp - std::chrono::hours(1));
                }
                timepoints = new_timepoints;
            }
            it++;
        }
    }
    // if they never expire the timepoint list can grow indefinitely..
    // remove timepoints older than now, but keep the most recent of these.
    template <typename K, typename V> void TimeCache<K, V>::clean_timepoints(const K &key) {

        auto it = cache_.find(key);
        if (it == cache_.end()) return;

        std::set<time_point> & timepoints = it->second->timepoints;

        if (timepoints.empty()) return;

        const time_point now = utility::clock::now();

        // get the most recent timepoint
        time_point newest = *(timepoints.rbegin());

        // erasing all entries older than 'now'
        while ((*timepoints.begin()) < now) {
            timepoints.erase(timepoints.begin());
            if (timepoints.empty()) break;
        }

        if (timepoints.empty())
            timepoints.emplace(newest);

    }
} // namespace utility
} // namespace xstudio
