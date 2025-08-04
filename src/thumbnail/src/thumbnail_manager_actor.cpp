// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

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
        anon_mail(json_store::update_atom_v, j).send(this);
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

        [=](media_reader::get_thumbnail_atom) { process_queue(); },

        [=](media_reader::get_thumbnail_atom atom, const media::AVFrameID &mptr) {
            return mail(atom, mptr, size_t(thumb_size_), size_t(0), true)
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](media_reader::get_thumbnail_atom atom, const media::AVFrameID &mptr) {
            return mail(atom, mptr, size_t(thumb_size_), size_t(0), true)
                .delegate(caf::actor_cast<caf::actor>(this));
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

            mail(atom)
                .request(target, infinite)
                .then(
                    [=](const bool) mutable {
                        if (mem_cache and disk_cache) {
                            mail(atom)
                                .request(dsk_cache_, infinite)
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
                return mail(atom).delegate(mem_cache_);
            else
                return mail(atom).delegate(dsk_cache_);
        },

        [=](media_cache::size_atom atom, const bool memory_cache) {
            if (memory_cache)
                return mail(atom).delegate(mem_cache_);
            else
                return mail(atom).delegate(dsk_cache_);
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
            anon_mail(atom, ThumbnailKey(key, thumb_size).hash(), buf).send(mem_cache_);
        },

        [=](media_reader::get_thumbnail_atom,
            const media::AVFrameID &mptr,
            const size_t thumb_size,
            const size_t hash,
            const bool cache_to_disk) -> result<ThumbnailBufferPtr> {
            // try cache..
            // cache key will differ from media keys.
            auto rp = make_response_promise<ThumbnailBufferPtr>();

            request_buffer(rp, mptr, thumb_size, hash, cache_to_disk);

            return rp;
        },

        [=](media_reader::get_thumbnail_atom,
            const media::AVFrameID &mptr,
            const size_t thumb_size,
            const size_t hash,
            const bool cache_to_disk) -> result<ThumbnailBufferPtr> {
            // try cache..
            // cache key will differ from media keys.
            auto rp = make_response_promise<ThumbnailBufferPtr>();

            request_buffer(rp, mptr, thumb_size, hash, cache_to_disk);

            return rp;
        },

        // convert to jpg
        [=](media_reader::get_thumbnail_atom,
            const ThumbnailBufferPtr &buffer,
            const int quality) {
            return mail(media_reader::get_thumbnail_atom_v, buffer, quality)
                .delegate(dsk_cache_);
        },

        // convert to jpg
        [=](media_reader::get_thumbnail_atom, const ThumbnailBufferPtr &buffer) {
            return mail(media_reader::get_thumbnail_atom_v, buffer).delegate(dsk_cache_);
        },

        // convert from jpg
        [=](media_reader::get_thumbnail_atom, const std::vector<std::byte> &buffer) {
            return mail(media_reader::get_thumbnail_atom_v, buffer).delegate(dsk_cache_);
        },

        [=](media_reader::get_thumbnail_atom atom,
            const media::AVFrameID &mptr,
            const size_t hash,
            const bool cache_to_disk) {
            return mail(atom, mptr, thumb_size_, hash, cache_to_disk)
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            return mail(json_store::update_atom_v, full).delegate(actor_cast<caf::actor>(this));
        },

        [=](json_store::update_atom, const JsonStore &js) mutable {
            try {
                auto new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/memory_cache/max_count");
                if (mem_max_count != new_size_t) {
                    mem_max_count = new_size_t;
                    anon_mail(media_cache::count_atom_v, mem_max_count).send(mem_cache_);
                    spdlog::debug(
                        "set mem_max_count {} {}", __PRETTY_FUNCTION__, mem_max_count);
                }

                new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/memory_cache/max_size") *
                    1024 * 1024;
                if (mem_max_size != new_size_t) {
                    mem_max_size = new_size_t;
                    anon_mail(media_cache::size_atom_v, mem_max_size).send(mem_cache_);
                    spdlog::debug("set mem_max_size {} {}", __PRETTY_FUNCTION__, mem_max_size);
                }


                new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/disk_cache/max_count");
                if (dsk_max_count != new_size_t) {
                    dsk_max_count = new_size_t;
                    anon_mail(media_cache::count_atom_v, dsk_max_count).send(dsk_cache_);
                    spdlog::debug(
                        "set dsk_max_count {} {}", __PRETTY_FUNCTION__, dsk_max_count);
                }

                new_size_t =
                    preference_value<size_t>(js, "/core/thumbnail/disk_cache/max_size") * 1024 *
                    1024;
                if (dsk_max_size != new_size_t) {
                    dsk_max_size = new_size_t;
                    anon_mail(media_cache::size_atom_v, dsk_max_size).send(dsk_cache_);
                    spdlog::debug("set dsk_max_size {} {}", __PRETTY_FUNCTION__, dsk_max_size);
                }

                auto new_string =
                    preference_value<std::string>(js, "/core/thumbnail/disk_cache/path");
                new_string = expand_envvars(new_string);

                if (cache_path != new_string) {
                    cache_path = new_string;
                    anon_mail(thumbnail::cache_path_atom_v, posix_path_to_uri(cache_path))
                        .send(dsk_cache_);
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
    auto err = make_error(xstudio_error::error, "System shutting down");
    for (auto &p : request_queue_) {
        p->deliver(err);
    }

    system().registry().erase(thumbnail_manager_registry);
}

void ThumbnailManagerActor::request_buffer(
    caf::typed_response_promise<ThumbnailBufferPtr> rp,
    const media::AVFrameID &mptr,
    const size_t thumb_size,
    const size_t hash,
    const bool cache_to_disk) {

    auto key = ThumbnailKey(mptr, hash, thumb_size).hash();

    mail(media_cache::retrieve_atom_v, key)
        .request(mem_cache_, infinite)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                if (buf) {
                    rp.deliver(buf);
                } else {
                    // add to queue, if to many requests or already inflight.
                    bool start_loop = request_queue_.empty();
                    queue_thumbnail_request(rp, mptr, thumb_size, hash, cache_to_disk);
                    if (start_loop)
                        anon_mail(media_reader::get_thumbnail_atom_v)
                            .send(caf::actor_cast<caf::actor>(this));
                }
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void ThumbnailManagerActor::queue_thumbnail_request(
    caf::typed_response_promise<ThumbnailBufferPtr> rp,
    const media::AVFrameID &mptr,
    const size_t thumb_size,
    const size_t hash,
    const bool cache_to_disk) {

    for (auto p = request_queue_.begin(); p != request_queue_.end(); ++p) {
        if ((*p)->mptr == mptr && (*p)->size == thumb_size && (*p)->hash == hash &&
            (*p)->cache_to_disk == cache_to_disk) {
            // we put it at the end of the queue, so it gets processed next
            auto tn_request = *p;
            tn_request->response_promises.push_back(rp);
            request_queue_.erase(p);
            request_queue_.push_back(tn_request);
            return;
        }
    }

    auto tn_request           = std::make_shared<ThumbnailRequest>();
    tn_request->mptr          = mptr;
    tn_request->size          = thumb_size;
    tn_request->hash          = hash;
    tn_request->cache_to_disk = cache_to_disk;
    tn_request->response_promises.push_back(rp);
    request_queue_.push_back(tn_request);
}

void ThumbnailManagerActor::request_buffer(
    caf::typed_response_promise<ThumbnailBufferPtr> rp,
    const std::string &key,
    const size_t thumb_size) {

    auto tkey = ThumbnailKey(key, thumb_size).hash();

    mail(media_cache::retrieve_atom_v, tkey)
        .request(mem_cache_, infinite)
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

    ThumbnailRequestPtr tn_request;

    // N.b.We process the requests most recent first. This is because the most
    // recent requests are made by the UI to draw thumbnails in the media list.
    // If the user scrolls through the media list we get a lot of thumbnail
    // requests, but we want to process the most recent first so the thumbnails
    // needed to go onscreen are provided immediately and the ones that were
    // scrolled past earlier are lower priority.

    // get the last request that isn't already in flight
    for (auto p = request_queue_.rbegin(); p != request_queue_.rend(); p++) {
        if (!(*p)->in_flight) {
            tn_request = (*p);
            break;
        }
    }

    if (!tn_request)
        return;

    tn_request->in_flight = true;

    auto key = ThumbnailKey(tn_request->mptr, tn_request->hash, tn_request->size).hash();

    auto continue_func = [=]() {
        // delayed send prevents 'tight loop' in the message queue,
        // so incoming messages such as cancelled thumbnail requests
        // are able to be adressed between processing thumbnails
        for (auto p = request_queue_.begin(); p != request_queue_.end(); p++) {
            if ((*p) == tn_request) {
                request_queue_.erase(p);
                break;
            }
        }
        if (request_queue_.size()) {
            anon_mail(media_reader::get_thumbnail_atom_v)
                .delay(std::chrono::milliseconds(5))
                .send(caf::actor_cast<caf::actor>(this));
        }
    };

    mail(media_cache::retrieve_atom_v, key)
        .request(mem_cache_, infinite)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                if (buf) {
                    tn_request->deliver(buf);
                    continue_func();
                } else {
                    mail(
                        media_reader::get_thumbnail_atom_v,
                        tn_request->mptr,
                        tn_request->size,
                        tn_request->hash,
                        tn_request->cache_to_disk)
                        .request(dsk_cache_, infinite)
                        .then(
                            [=](ThumbnailBufferPtr &buf) mutable {
                                // if valid add to memory cache
                                if (buf)
                                    anon_mail(
                                        media_cache::store_atom_v,
                                        ThumbnailKey(
                                            tn_request->mptr,
                                            tn_request->hash,
                                            tn_request->size)
                                            .hash(),
                                        buf)
                                        .send(mem_cache_);

                                // deliver buffer.
                                tn_request->deliver(buf);
                                continue_func();
                            },
                            [=](const caf::error &err) mutable {
                                tn_request->deliver(err);
                                continue_func();
                            });
                }
            },
            [=](const caf::error &err) mutable {
                tn_request->deliver(err);
                continue_func();
            });
}
