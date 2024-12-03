// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <functional>
#include <malloc.h>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_cache/media_cache_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/time_cache.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::media_cache;
using namespace xstudio::global_store;
using namespace xstudio::utility;
using namespace caf;

class TrimActor : public caf::event_based_actor {
  public:
    TrimActor(caf::actor_config &cfg);
    ~TrimActor() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "TrimActor";
    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
};

TrimActor::TrimActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    // trim memory usage, as when idle it'll slowly creep up until we run out of memory.
    delayed_anon_send(this, std::chrono::minutes(10), unpreserve_atom_v);

    behavior_.assign(
        [=](unpreserve_atom, const size_t count) {
    // spdlog::stopwatch sw;
#ifdef _WIN32
            _heapmin();
#else
            malloc_trim(64);
#endif
            // spdlog::warn("Release {:.3f}", sw);
        },
        [=](unpreserve_atom) {
#ifdef _WIN32
            _heapmin();
#else
            malloc_trim(64);
#endif
            delayed_anon_send(this, std::chrono::minutes(10), unpreserve_atom_v);
        }

    );
}

GlobalImageCacheActor::GlobalImageCacheActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg), update_pending_(false) {
    print_on_exit(this, "GlobalImageCacheActor");

    system().registry().put(image_cache_registry, this);
    size_t max_size  = std::numeric_limits<size_t>::max();
    size_t max_count = std::numeric_limits<size_t>::max();

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        max_count   = preference_value<size_t>(j, "/core/image_cache/max_count");
        max_size    = preference_value<size_t>(j, "/core/image_cache/max_size") * 1024 * 1024;
        reset_idle_ = std::chrono::minutes(
            preference_value<size_t>(j, "/core/image_cache/release_on_idle"));
    } catch (...) {
    }

    cache_.set_max_size(max_size);
    cache_.set_max_count(max_count);
    cache_.bind_change_callback([this](auto &&PH1, auto &&PH2) {
        update_changes(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    });

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    auto trim = spawn<TrimActor>();
    link_to(trim);

    delayed_anon_send(this, std::chrono::minutes(1), clear_atom_v, true);

    // For cache benchmarking
    // cache_.noisy = true;

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](clear_atom, const bool idle_check) {
            if (reset_idle_.count() and not cache_.empty() and
                utility::clock::now() - last_activity_ > reset_idle_) {
                anon_send(this, clear_atom_v);
            }
            delayed_anon_send(this, std::chrono::minutes(1), clear_atom_v, true);
        },
        [=](clear_atom) -> bool {
            cache_.clear();
            anon_send(trim, unpreserve_atom_v, static_cast<size_t>(0));
            return true;
        },

        [=](count_atom) -> size_t { return cache_.count(); },

        [=](erase_atom, const media::MediaKey &key) { cache_.erase(key); },

        [=](erase_atom, const media::MediaKey &key, const utility::Uuid &uuid) {
            cache_.erase(key, uuid);
        },

        [=](erase_atom, const media::MediaKeyVector &keys) -> media::MediaKeyVector {
            return cache_.erase(keys);
        },

        [=](erase_atom, const utility::Uuid &uuid) -> bool {
            cache_.erase(uuid);
            return true;
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &js) {
            try {
                reset_idle_ = std::chrono::minutes(
                    preference_value<size_t>(js, "/core/image_cache/release_on_idle"));
                auto new_count = preference_value<size_t>(js, "/core/image_cache/max_count");
                auto new_size =
                    preference_value<size_t>(js, "/core/image_cache/max_size") * 1024 * 1024;
                if (cache_.max_size() != new_size)
                    cache_.set_max_size(new_size);
                if (cache_.max_count() != new_count)
                    cache_.set_max_count(new_count);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](keys_atom) -> media::MediaKeyVector { return cache_.keys(); },

        [=](keys_atom, bool) {
            if (not erased_keys_.empty() or not new_keys_.empty()) {
                // force purge of memory..
                if (not erased_keys_.empty())
                    anon_send(trim, unpreserve_atom_v, erased_keys_.size());

                send(
                    event_group_,
                    utility::event_atom_v,
                    media_cache::keys_atom_v,
                    media::MediaKeyVector(new_keys_.begin(), new_keys_.end()),
                    media::MediaKeyVector(erased_keys_.begin(), erased_keys_.end()));
            }

            new_keys_.clear();
            erased_keys_.clear();
            update_pending_ = false;
        },

        [=](unpreserve_atom, const utility::Uuid &uuid) -> bool {
            cache_.unpreserve(uuid);
            return true;
        },

        [=](preserve_atom, const media::MediaKey &key) -> bool { return cache_.preserve(key); },

        [=](preserve_atom, const media::MediaKey &key, const time_point &time) -> bool {
            return cache_.preserve(key, time);
        },

        [=](preserve_atom, const media::MediaKey &key, const time_point &time, const Uuid &uuid)
            -> bool { return cache_.preserve(key, time, uuid); },

        // given a list of frame pointers, check which frames are in the cache
        // and return a list of those that *aren't* in the cache
        [=](preserve_atom,
            const media::AVFrameIDsAndTimePoints &mpts,
            const Uuid &uuid) -> media::AVFrameIDsAndTimePoints {
            media::AVFrameIDsAndTimePoints result;
            result.reserve(mpts.size());
            for (const auto &p : mpts) {
                if (!cache_.preserve(p.second->key(), p.first, uuid)) {
                    result.push_back(p);
                }
            }
            return result;
        },

        [=](preserve_atom,
            const std::vector<std::pair<media::MediaKey, utility::time_point>>
                &keys_and_timepoints) -> bool {
            cache_.make_entries_hot(keys_and_timepoints);
            return true;
        },

        [=](retrieve_atom, const media::MediaKey &key) -> media_reader::ImageBufPtr {
            last_activity_ = utility::clock::now();
            return cache_.retrieve(key);
        },

        [=](retrieve_atom, const media::AVFrameIDsAndTimePoints &mptr_and_timepoints)
            -> std::vector<media_reader::ImageBufPtr> {
            std::vector<media_reader::ImageBufPtr> result;
            result.reserve(mptr_and_timepoints.size());

            last_activity_ = utility::clock::now();

            for (const auto &p : mptr_and_timepoints) {
                result.emplace_back(cache_.retrieve(p.second->key(), p.first));
                result.back().when_to_display_ = p.first;
            }
            return result;
        },

        [=](retrieve_atom,
            const media::MediaKey &key,
            const time_point &time) -> media_reader::ImageBufPtr {
            last_activity_ = utility::clock::now();
            return cache_.retrieve(key, time);
        },

        [=](retrieve_atom, const media::MediaKey &key, const time_point &time, const Uuid &uuid)
            -> media_reader::ImageBufPtr {
            last_activity_ = utility::clock::now();
            return cache_.retrieve(key, time, uuid);
        },

        [=](size_atom) -> size_t { return cache_.size(); },

        [=](store_atom, const media::MediaKey &key, media_reader::ImageBufPtr buf) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf);
        },

        [=](store_atom,
            const media::MediaKey &key,
            const media_reader::ImageBufPtr &buf,
            const time_point &when) -> bool {
            last_activity_ = utility::clock::now();

            return cache_.store(key, buf, when);
        },

        [=](store_atom,
            const media::MediaKey &key,
            const media_reader::ImageBufPtr &buf,
            const time_point &when,
            const utility::Uuid &uuid) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf, when, false, uuid);
        },

        [=](store_atom,
            const media::MediaKey &key,
            const media_reader::ImageBufPtr &buf,
            const time_point &when,
            const utility::Uuid &uuid) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf, when, false, uuid);
        },
        [=](store_atom,
            const media::MediaKey &key,
            const media_reader::ImageBufPtr &buf,
            const time_point &when,
            const utility::Uuid &uuid,
            const time_point &cache_out_date_tp) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf, when, uuid, cache_out_date_tp);
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}

void GlobalImageCacheActor::update_changes(
    const media::MediaKeyVector &store, const media::MediaKeyVector &erase) {

    new_keys_.insert(store.begin(), store.end());
    for (const auto &i : store)
        erased_keys_.erase(i);

    erased_keys_.insert(erase.begin(), erase.end());
    for (const auto &i : erase)
        new_keys_.erase(i);

    if (not update_pending_ and (not new_keys_.empty() or not erased_keys_.empty())) {
        update_pending_ = true;
        delayed_anon_send(this, std::chrono::milliseconds(250), keys_atom_v, true);
    }
}

void GlobalImageCacheActor::on_exit() { system().registry().erase(image_cache_registry); }


GlobalAudioCacheActor::GlobalAudioCacheActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg), update_pending_(false) {
    print_on_exit(this, "GlobalAudioCacheActor");

    system().registry().put(audio_cache_registry, this);
    size_t max_size  = std::numeric_limits<size_t>::max();
    size_t max_count = std::numeric_limits<size_t>::max();

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        max_count   = preference_value<size_t>(j, "/core/audio_cache/max_count");
        max_size    = preference_value<size_t>(j, "/core/audio_cache/max_size") * 1024 * 1024;
        reset_idle_ = std::chrono::minutes(
            preference_value<size_t>(j, "/core/audio_cache/release_on_idle"));
    } catch (...) {
    }

    cache_.set_max_size(max_size);
    cache_.set_max_count(max_count);
    cache_.bind_change_callback([this](auto &&PH1, auto &&PH2) {
        update_changes(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    });
    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    delayed_anon_send(this, std::chrono::minutes(1), clear_atom_v, true);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](clear_atom, const bool idle_check) {
            if (reset_idle_.count() and not cache_.empty() and
                utility::clock::now() - last_activity_ > reset_idle_) {
                anon_send(this, clear_atom_v);
            }
            delayed_anon_send(this, std::chrono::minutes(1), clear_atom_v, true);
        },

        [=](clear_atom) -> bool {
            cache_.clear();
            return true;
        },

        [=](count_atom) -> size_t { return cache_.count(); },

        [=](erase_atom, const media::MediaKey &key) { cache_.erase(key); },

        [=](erase_atom, const media::MediaKey &key, const utility::Uuid &uuid) {
            cache_.erase(key, uuid);
        },

        [=](erase_atom, const media::MediaKeyVector &keys) -> media::MediaKeyVector {
            return cache_.erase(keys);
        },

        [=](erase_atom, const utility::Uuid &uuid) { cache_.erase(uuid); },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &js) {
            try {
                reset_idle_ = std::chrono::minutes(
                    preference_value<size_t>(js, "/core/audio_cache/release_on_idle"));

                auto new_count = preference_value<size_t>(js, "/core/audio_cache/max_count");
                auto new_size =
                    preference_value<size_t>(js, "/core/audio_cache/max_size") * 1024 * 1024;
                if (cache_.max_size() != new_size) {
                    cache_.set_max_size(new_size);
                }
                if (cache_.max_count() != new_count)
                    cache_.set_max_count(new_count);
            } catch (...) {
            }
        },

        [=](keys_atom) -> media::MediaKeyVector { return cache_.keys(); },

        [=](keys_atom, bool) {
            if (not erased_keys_.empty() or not new_keys_.empty())
                send(
                    event_group_,
                    utility::event_atom_v,
                    media_cache::keys_atom_v,
                    media::MediaKeyVector(new_keys_.begin(), new_keys_.end()),
                    media::MediaKeyVector(erased_keys_.begin(), erased_keys_.end()));

            new_keys_.clear();
            erased_keys_.clear();
            update_pending_ = false;
        },

        [=](preserve_atom, const media::MediaKey &key) -> bool { return cache_.preserve(key); },

        [=](preserve_atom, const media::MediaKey &key, const time_point &time) -> bool {
            return cache_.preserve(key, time);
        },

        // given a list of frame pointers, check which frames are in the cache
        // and return a list of those that *aren't* in the cache

        [=](preserve_atom,
            const media::AVFrameIDsAndTimePoints &mpts,
            const Uuid &uuid) -> media::AVFrameIDsAndTimePoints {
            media::AVFrameIDsAndTimePoints result;
            result.reserve(mpts.size());
            for (const auto &p : mpts) {
                if (!cache_.preserve(p.second->key(), p.first, uuid)) {
                    result.push_back(p);
                }
            }
            return result;
        },

        [=](preserve_atom, const media::MediaKey &key, const time_point &time, const Uuid &uuid)
            -> bool { return cache_.preserve(key, time, uuid); },

        [=](retrieve_atom, const media::AVFrameIDsAndTimePoints &mptr_and_timepoints)
            -> std::vector<media_reader::AudioBufPtr> {
            std::vector<media_reader::AudioBufPtr> result;
            last_activity_ = utility::clock::now();

            result.reserve(mptr_and_timepoints.size());

            for (const auto &p : mptr_and_timepoints) {
                result.emplace_back(cache_.retrieve(p.second->key(), p.first));
                result.back().when_to_display_ = p.first;
            }

            return result;
        },

        [=](retrieve_atom, const media::MediaKey &key) -> media_reader::AudioBufPtr {
            last_activity_ = utility::clock::now();
            return cache_.retrieve(key);
        },

        [=](retrieve_atom,
            const media::MediaKey &key,
            const time_point &time) -> media_reader::AudioBufPtr {
            last_activity_ = utility::clock::now();

            return cache_.retrieve(key, time);
        },

        [=](retrieve_atom, const media::MediaKey &key, const time_point &time, const Uuid &uuid)
            -> media_reader::AudioBufPtr {
            last_activity_ = utility::clock::now();
            return cache_.retrieve(key, time, uuid);
        },

        [=](size_atom) -> size_t { return cache_.size(); },

        [=](store_atom, const media::MediaKey &key, media_reader::AudioBufPtr buf) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf);
        },

        [=](store_atom,
            const media::MediaKey &key,
            media_reader::AudioBufPtr buf,
            const time_point &when) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf, when);
        },

        [=](store_atom,
            const media::MediaKey &key,
            media_reader::AudioBufPtr buf,
            const time_point &when,
            const utility::Uuid &uuid) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf, when, false, uuid);
        },

        [=](store_atom,
            const media::MediaKey &key,
            media_reader::AudioBufPtr buf,
            const time_point &when,
            const utility::Uuid &uuid,
            const time_point &cache_out_date_tp) -> bool {
            last_activity_ = utility::clock::now();
            return cache_.store(key, buf, when, uuid, cache_out_date_tp);
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}

void GlobalAudioCacheActor::on_exit() { system().registry().erase(audio_cache_registry); }


void GlobalAudioCacheActor::update_changes(
    const media::MediaKeyVector &store, const media::MediaKeyVector &erase) {

    new_keys_.insert(store.begin(), store.end());
    for (const auto &i : store)
        erased_keys_.erase(i);

    erased_keys_.insert(erase.begin(), erase.end());
    for (const auto &i : erase)
        new_keys_.erase(i);

    if (not update_pending_ and (not new_keys_.empty() or not erased_keys_.empty())) {
        update_pending_ = true;
        delayed_anon_send(this, std::chrono::milliseconds(250), keys_atom_v, true);
    }
}
