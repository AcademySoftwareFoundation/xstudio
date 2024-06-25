// SPDX-License-Identifier: Apache-2.0
#include <functional>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/thumbnail/thumbnail_cache_actor.hpp"
#include "xstudio/thumbnail/thumbnail_disk_cache_actor.hpp"
#include "xstudio/thumbnail/thumbnail_manager_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace caf;
using namespace xstudio::global_store;
using namespace xstudio::thumbnail;
using namespace xstudio::utility;
using namespace xstudio;

ThumbnailManagerActor::ThumbnailManagerActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {
    print_on_exit(this, "ThumbnailManagerActor");

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    system().registry().put(thumbnail_manager_registry, this);

    size_t mem_max_size    = std::numeric_limits<size_t>::max();
    size_t mem_max_count   = std::numeric_limits<size_t>::max();
    size_t dsk_max_size    = std::numeric_limits<size_t>::max();
    size_t dsk_max_count   = std::numeric_limits<size_t>::max();
    std::string cache_path = "";
    size_t thumb_size_     = 256;

    // subscribe to prefs and push to self.
    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        anon_send(this, json_store::update_atom_v, j);
    } catch (...) {
    }

    mem_cache_ = spawn<ThumbnailCacheActor>();
    link_to(mem_cache_);
    dsk_cache_ = spawn<ThumbnailDiskCacheActor>();
    link_to(dsk_cache_);

    // watch for global status events.
    // join_event_group(this, system().registry().template get<caf::actor>(global_registry));

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](media_reader::cancel_thumbnail_request_atom atom, const utility::Uuid job_uuid) {
            for (auto i = request_queue_.begin(); i != request_queue_.end(); ++i) {
                auto rp                    = std::get<0>(*i);
                const utility::Uuid j_uuid = std::get<5>(*i);
                if (j_uuid == job_uuid) {
                    rp.deliver(make_error(xstudio_error::error, "Cancelled"));
                    request_queue_.erase(i);
                    break;
                }
            }
        },

        [=](media_reader::get_thumbnail_atom) { process_queue(); },

        [=](media_reader::get_thumbnail_atom atom, const media::AVFrameID &mptr) {
            delegate(
                caf::actor_cast<caf::actor>(this),
                atom,
                mptr,
                size_t(thumb_size_),
                size_t(0),
                true,
                utility::Uuid());
        },

        [=](media_reader::get_thumbnail_atom atom,
            const media::AVFrameID &mptr,
            const utility::Uuid &job_uuid) {
            delegate(
                caf::actor_cast<caf::actor>(this),
                atom,
                mptr,
                size_t(thumb_size_),
                size_t(0),
                true,
                job_uuid);
        },

        [=](utility::clear_atom atom,
            const bool mem_cache,
            const bool disk_cache) -> result<bool> {
            if (not mem_cache and not disk_cache)
                return true;

            auto target = caf::actor();
            auto rp     = make_response_promise<bool>();

            if (mem_cache)
                target = mem_cache_;
            else if (disk_cache)
                target = dsk_cache_;

            request(target, infinite, atom)
                .then(
                    [=](const bool) mutable {
                        if (mem_cache and disk_cache) {
                            request(dsk_cache_, infinite, atom)
                                .then(
                                    [=](const bool) mutable { rp.deliver(true); },
                                    [=](const caf::error &err) mutable { rp.deliver(err); });

                        } else {
                            rp.deliver(true);
                        }
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_cache::count_atom atom, const bool memory_cache) {
            if (memory_cache)
                delegate(mem_cache_, atom);
            else
                delegate(dsk_cache_, atom);
        },

        [=](media_cache::size_atom atom, const bool memory_cache) {
            if (memory_cache)
                delegate(mem_cache_, atom);
            else
                delegate(dsk_cache_, atom);
        },

        [=](media_reader::get_thumbnail_atom atom,
            const media::AVFrameID &mptr,
            const size_t thumb_size,
            const size_t hash,
            const bool cache_to_disk) {
            delegate(
                caf::actor_cast<caf::actor>(this),
                atom,
                mptr,
                thumb_size,
                hash,
                cache_to_disk,
                Uuid::generate());
        },

        // get thumb from memory cache.
        [=](media_reader::get_thumbnail_atom,
            const std::string &key,
            const size_t thumb_size) -> result<ThumbnailBufferPtr> {
            auto rp = make_response_promise<ThumbnailBufferPtr>();
            request_buffer(rp, key, thumb_size);
            return rp;
        },

        [=](media_cache::store_atom atom,
            const std::string &key,
            const size_t thumb_size,
            const ThumbnailBufferPtr &buf) {
            anon_send(mem_cache_, atom, ThumbnailKey(key, thumb_size).hash(), buf);
        },

        [=](media_reader::get_thumbnail_atom,
            const media::AVFrameID &mptr,
            const size_t thumb_size,
            const size_t hash,
            const bool cache_to_disk,
            const utility::Uuid &job_uuid) -> result<ThumbnailBufferPtr> {
            // try cache..
            // cache key will differ from media keys.
            auto rp = make_response_promise<ThumbnailBufferPtr>();

            request_buffer(rp, mptr, thumb_size, hash, cache_to_disk, job_uuid);

            return rp;
        },

        // convert to jpg
        [=](media_reader::get_thumbnail_atom, const ThumbnailBufferPtr &buffer) {
            delegate(dsk_cache_, media_reader::get_thumbnail_atom_v, buffer);
        },

        // convert from jpg
        [=](media_reader::get_thumbnail_atom, const std::vector<std::byte> &buffer) {
            delegate(dsk_cache_, media_reader::get_thumbnail_atom_v, buffer);
        },

        [=](media_reader::get_thumbnail_atom atom,
            const media::AVFrameID &mptr,
            const size_t hash,
            const bool cache_to_disk) {
            delegate(
                caf::actor_cast<caf::actor>(this),
                atom,
                mptr,
                thumb_size_,
                hash,
                cache_to_disk);
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &js) mutable {
            try {
                auto new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/memory_cache/max_count");
                if (mem_max_count != new_size_t) {
                    mem_max_count = new_size_t;
                    anon_send(mem_cache_, media_cache::count_atom_v, mem_max_count);
                    spdlog::debug(
                        "set mem_max_count {} {}", __PRETTY_FUNCTION__, mem_max_count);
                }

                new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/memory_cache/max_size") *
                    1024 * 1024;
                if (mem_max_size != new_size_t) {
                    mem_max_size = new_size_t;
                    anon_send(mem_cache_, media_cache::size_atom_v, mem_max_size);
                    spdlog::debug("set mem_max_size {} {}", __PRETTY_FUNCTION__, mem_max_size);
                }


                new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/disk_cache/max_count");
                if (dsk_max_count != new_size_t) {
                    dsk_max_count = new_size_t;
                    anon_send(dsk_cache_, media_cache::count_atom_v, dsk_max_count);
                    spdlog::debug(
                        "set dsk_max_count {} {}", __PRETTY_FUNCTION__, dsk_max_count);
                }

                new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/disk_cache/max_size") * 1024 *
                    1024;
                if (dsk_max_size != new_size_t) {
                    dsk_max_size = new_size_t;
                    anon_send(dsk_cache_, media_cache::size_atom_v, dsk_max_size);
                    spdlog::debug("set dsk_max_size {} {}", __PRETTY_FUNCTION__, dsk_max_size);
                }

                auto new_string =
                    preference_value<std::string>(js, "/core/thumbnail/disk_cache/path");
                new_string = expand_envvars(new_string);
                
                if (cache_path != new_string) {
                    cache_path = new_string;
                    anon_send(
                        dsk_cache_,
                        thumbnail::cache_path_atom_v,
                        posix_path_to_uri(cache_path));
                    spdlog::debug("set cache_path {} {}", __PRETTY_FUNCTION__, cache_path);
                }

                new_size_t = preference_value<size_t>(js, "/core/thumbnail/size");
                if (thumb_size_ != new_size_t) {
                    thumb_size_ = new_size_t;
                }
            } catch (...) {
            }
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}


void ThumbnailManagerActor::on_exit() {
    // fufil pending queries.
    while (not request_queue_.empty()) {
        auto [r, m, t, h, c, i] = request_queue_.front();
        r.deliver(make_error(xstudio_error::error, "System shutting down"));
        request_queue_.pop_front();
    }

    system().registry().erase(thumbnail_manager_registry);
}

void ThumbnailManagerActor::request_buffer(
    caf::typed_response_promise<ThumbnailBufferPtr> rp,
    const media::AVFrameID &mptr,
    const size_t thumb_size,
    const size_t hash,
    const bool cache_to_disk,
    const utility::Uuid &job_uuid) {

    auto key = ThumbnailKey(mptr, hash, thumb_size).hash();

    request(mem_cache_, infinite, media_cache::retrieve_atom_v, key)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                if (buf) {
                    rp.deliver(buf);
                } else {
                    // add to queue, if to many requests or already inflight.
                    bool start_loop = request_queue_.empty();
                    request_queue_.emplace_back(
                        std::make_tuple(rp, mptr, thumb_size, hash, cache_to_disk, job_uuid));
                    if (start_loop)
                        anon_send(
                            caf::actor_cast<caf::actor>(this),
                            media_reader::get_thumbnail_atom_v);
                }
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void ThumbnailManagerActor::request_buffer(
    caf::typed_response_promise<ThumbnailBufferPtr> rp,
    const std::string &key,
    const size_t thumb_size) {

    auto tkey = ThumbnailKey(key, thumb_size).hash();

    request(mem_cache_, infinite, media_cache::retrieve_atom_v, tkey)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                if (buf) {
                    rp.deliver(buf);
                } else {
                    rp.deliver(make_error(xstudio_error::error, "No cached image."));
                }
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}


void ThumbnailManagerActor::process_queue() {

    if (!request_queue_.size())
        return;

    auto r = request_queue_.front();

    // unpack this tuple!
    caf::typed_response_promise<ThumbnailBufferPtr> response_promise = std::get<0>(r);
    media::AVFrameID media_ptr                                       = std::get<1>(r);
    const size_t thumb_size                                          = std::get<2>(r);
    const size_t hash                                                = std::get<3>(r);
    const bool cache_to_disk                                         = std::get<4>(r);
    const utility::Uuid job_id                                       = std::get<5>(r);

    auto key = ThumbnailKey(media_ptr, hash, thumb_size).hash();

    auto continue_func = [=]() {
        for (auto i = request_queue_.begin(); i != request_queue_.end(); ++i) {
            const utility::Uuid j_uuid = std::get<5>(*i);
            if (j_uuid == job_id) {
                request_queue_.erase(i);
                break;
            }
        }
        // delayed send prevents 'tight loop' in the message queue,
        // so incoming messages such as cancelled thumbnail requests
        // are able to be adressed between processing thumbnails
        if (request_queue_.size()) {
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::milliseconds(5),
                media_reader::get_thumbnail_atom_v);
        }
    };

    request(mem_cache_, infinite, media_cache::retrieve_atom_v, key)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                if (buf) {
                    response_promise.deliver(buf);
                    continue_func();
                } else {
                    request(
                        dsk_cache_,
                        infinite,
                        media_reader::get_thumbnail_atom_v,
                        media_ptr,
                        thumb_size,
                        hash,
                        cache_to_disk)
                        .then(
                            [=](ThumbnailBufferPtr &buf) mutable {
                                // if valid add to memory cache
                                if (buf)
                                    anon_send(
                                        mem_cache_,
                                        media_cache::store_atom_v,
                                        ThumbnailKey(media_ptr, hash, thumb_size).hash(),
                                        buf);

                                // deliver buffer.
                                response_promise.deliver(buf);
                                continue_func();
                            },
                            [=](const caf::error &err) mutable {
                                response_promise.deliver(err);
                                continue_func();
                            });
                }
            },
            [=](const caf::error &err) mutable {
                request_queue_.pop_front();
                continue_func();
            });
}
