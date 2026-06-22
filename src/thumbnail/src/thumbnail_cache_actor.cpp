// SPDX-License-Identifier: Apache-2.0
#include <functional>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/thumbnail_cache_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::thumbnail;
using namespace xstudio::utility;
using namespace caf;

ThumbnailCacheActor::ThumbnailCacheActor(
    caf::actor_config &cfg, const size_t max_size, const size_t max_count)
    : caf::event_based_actor(cfg), update_pending_(false) {
    print_on_exit(this, "ThumbnailCacheActor");

    cache_.set_max_size(max_size);
    cache_.set_max_count(max_count);
    cache_.bind_change_callback([this](auto &&PH1, auto &&PH2) {
        update_changes(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    });

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](utility::clear_atom) -> bool {
            cache_.clear();
            return true;
        },

        [=](media_cache::count_atom) -> size_t { return cache_.count(); },

        [=](media_cache::count_atom, const size_t max_count) {
            cache_.set_max_count(max_count);
        },

        [=](media_cache::erase_atom, const size_t &key) { cache_.erase(key); },

        [=](media_cache::erase_atom, const std::vector<size_t> &keys) -> std::vector<size_t> {
            return cache_.erase(keys);
        },

        [=](media_cache::keys_atom) -> std::vector<size_t> { return cache_.keys(); },

        [=](media_cache::keys_atom, bool) {
            if (not erased_keys_.empty() or not new_keys_.empty())
                mail(
                    utility::event_atom_v,
                    media_cache::keys_atom_v,
                    std::vector<size_t>(new_keys_.begin(), new_keys_.end()),
                    std::vector<size_t>(erased_keys_.begin(), erased_keys_.end()))
                    .send(event_group_);

            new_keys_.clear();
            erased_keys_.clear();
            update_pending_ = false;
        },

        [=](media_cache::preserve_atom, const size_t &key) -> bool {
            return cache_.preserve(key);
        },

        [=](media_cache::retrieve_atom, const size_t &key) -> ThumbnailBufferPtr {
            return cache_.retrieve(key);
        },

        [=](media_cache::size_atom) -> size_t { return cache_.size(); },
        [=](media_cache::size_atom, const size_t max_size) { cache_.set_max_size(max_size); },

        [=](media_cache::store_atom, const size_t &key, ThumbnailBufferPtr buf) {
            cache_.store(key, buf);
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}

void ThumbnailCacheActor::update_changes(
    const std::vector<size_t> &store, const std::vector<size_t> &erase) {
    for (const auto &i : store) {
        new_keys_.insert(i);
        erased_keys_.erase(i);
    }

    for (const auto &i : erase) {
        new_keys_.erase(i);
        erased_keys_.insert(i);
    }
    if (not update_pending_ and (not new_keys_.empty() or not erased_keys_.empty())) {
        update_pending_ = true;
        anon_mail(media_cache::keys_atom_v, true)
            .delay(std::chrono::milliseconds(250))
            .send(this);
    }
}

void ThumbnailCacheActor::on_exit() {}
