// SPDX-License-Identifier: Apache-2.0
#include <caf/sec.hpp>
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include <limits>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/caf_media_error.hpp"
#include "xstudio/media_reader/cacheing_media_reader_actor.hpp"
#include "xstudio/media_reader/media_detail_and_thumbnail_reader_actor.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/time_cache.hpp"

using namespace std::chrono_literals;

using namespace caf;

using namespace xstudio;
using namespace xstudio::media;
using namespace xstudio::global_store;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media_reader;
using namespace xstudio::media_cache;

namespace {
using map_addr_timepoint = std::map<std::string, utility::time_point>;
using workers_t          = std::list<std::shared_ptr<std::pair<caf::actor, int>>>;
} // namespace


class ReaderHelper : public caf::event_based_actor {
  public:
    ReaderHelper(
        caf::actor_config &cfg,
        std::vector<caf::actor> plugins,
        std::map<std::string, utility::Uuid> plugin_map);
    ~ReaderHelper() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ReaderHelper";
    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
    std::vector<caf::actor> plugins_;
    std::map<std::string, utility::Uuid> plugin_map_;
};

ReaderHelper::ReaderHelper(
    caf::actor_config &cfg,
    std::vector<caf::actor> plugins,
    std::map<std::string, utility::Uuid> plugin_map)
    : caf::event_based_actor(cfg),
      plugins_(std::move(plugins)),
      plugin_map_(std::move(plugin_map)) {

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](get_reader_atom,
            const caf::uri &_uri,
            const std::string &hint) -> result<caf::actor> {
            // spdlog::warn("get_reader_atom {} {}", to_string(_uri),hint);
            auto tp = utility::clock::now();

            // use hint
            if (not hint.empty() and plugin_map_.count(hint)) {
                auto uuid = plugin_map_[hint];
                try {
                    return spawn<CachingMediaReaderActor>(
                        uuid,
                        system().registry().template get<caf::actor>(image_cache_registry),
                        system().registry().template get<caf::actor>(audio_cache_registry));
                } catch (...) {
                    // shutting down
                    return make_error(sec::runtime_error, "Shutting down");
                }
            }

            auto rp = make_response_promise<caf::actor>();
            try {
                fan_out_request<policy::select_all>(
                    plugins_,
                    infinite,
                    media_reader::supported_atom_v,
                    _uri,
                    utility::get_signature(_uri))
                    .then(
                        [=](std::vector<std::pair<xstudio::utility::Uuid, MRCertainty>>
                                supt) mutable {
                            // find best match or fail..
                            xstudio::utility::Uuid best_reader_plugin_uuid;
                            MRCertainty best_match = MRC_NO;

                            for (const auto &i : supt) {
                                if (i.second > best_match) {
                                    best_match              = i.second;
                                    best_reader_plugin_uuid = i.first;
                                }
                            }

                            if (best_match == MRC_NO) {
                                spdlog::warn("{} Unsupported format.", __PRETTY_FUNCTION__);
                                rp.deliver(
                                    make_error(media_error::unsupported, "Unsupported format"));
                            } else {
                                try {
                                    rp.deliver(spawn<CachingMediaReaderActor>(
                                        best_reader_plugin_uuid,
                                        system().registry().template get<caf::actor>(
                                            image_cache_registry),
                                        system().registry().template get<caf::actor>(
                                            audio_cache_registry)));
                                } catch (...) {
                                    // shutting down
                                    return rp.deliver(
                                        make_error(sec::runtime_error, "Shutting down"));
                                }
                            }
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } catch (const std::exception &err) {
                rp.deliver(make_error(media_error::unreadable, err.what()));
            }

            return rp;
        });
}

GlobalMediaReaderActor::GlobalMediaReaderActor(
    caf::actor_config &cfg, const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), uuid_(uuid), max_source_count_(256), max_source_age_(600) {
    print_on_exit(this, "GlobalMediaReaderActor");
    spdlog::debug("Created GlobalMediaReaderActor.");
    if (uuid_.is_null())
        uuid_.generate_in_place();

    // get plugins
    {
        JsonStore js;
        try {
            auto prefs = GlobalStoreHelper(system());
            join_broadcast(this, prefs.get_group(js));
            max_source_count_ =
                preference_value<size_t>(js, "/core/media_reader/max_source_count");
            max_source_age_ = preference_value<size_t>(js, "/core/media_reader/max_source_age");
        } catch (...) {
        }

        auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
        scoped_actor sys{system()};
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_READER));

        for (const auto &i : details) {
            if (i.enabled_) {
                auto actor = request_receive<caf::actor>(
                    *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_, js);
                link_to(actor);
                plugins_.push_back(actor);
                plugins_map_[i.name_] = i.uuid_;
            }
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    pool_ = caf::actor_pool::make(
        system(),
        5,
        [&] { return system().spawn<ReaderHelper>(plugins_, plugins_map_); },
        caf::actor_pool::round_robin());
    link_to(pool_);

    system().registry().put(media_reader_registry, this);

    anon_mail(retire_readers_atom_v).delay(std::chrono::seconds(max_source_age_)).send(this);

    image_cache_ = system().registry().template get<caf::actor>(image_cache_registry);
    audio_cache_ = system().registry().template get<caf::actor>(audio_cache_registry);

    auto media_detail_reader_pool = caf::actor_pool::make(
        system(),
        4, // hardcoding to 4 media detail fetchers
        [&] { return system().spawn<MediaDetailAndThumbnailReaderActor>(); },
        caf::actor_pool::round_robin());
    link_to(media_detail_reader_pool);

    auto thumbnail_reader_pool = caf::actor_pool::make(
        system(),
        1, // hardcoding to 1 thumbnail reader. The reader doesn't actually do
        // the work, it just delegates to the global media reader but via a
        // queue of requests
        [&] { return system().spawn<MediaDetailAndThumbnailReaderActor>(); },
        caf::actor_pool::round_robin());
    link_to(thumbnail_reader_pool);

#pragma GCC diagnostic pop

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](clear_precache_queue_atom, const Uuid &playhead_uuid) -> bool {
            playback_precache_request_queue_.clear_pending_requests(playhead_uuid);
            background_precache_request_queue_.clear_pending_requests(playhead_uuid);

            // this marks all cache entries for this playhead as 'stale' by
            // moving their timestamps to 1 hour in the past - hence they
            // will be dropped if the cache fills up
            anon_mail(media_cache::unpreserve_atom_v, playhead_uuid).send(image_cache_);

            /*anon_mail(media_cache::unpreserve_atom_v,
                playhead_uuid).send(audio_cache_);*/

            return true;
        },

        [=](clear_precache_queue_atom, const std::vector<Uuid> &playhead_uuids) -> bool {
            for (const auto &playhead_uuid : playhead_uuids) {

                playback_precache_request_queue_.clear_pending_requests(playhead_uuid);
                background_precache_request_queue_.clear_pending_requests(playhead_uuid);
                // this marks all cache entries for this playhead as 'stale' by
                // moving their timestamps to 1 hour in the past - hence they
                // will be dropped if the cache fills up

                anon_mail(media_cache::unpreserve_atom_v, playhead_uuid).send(image_cache_);
            }
            return true;
        },

        [=](retire_readers_atom, const media::AVFrameID &mptr) -> bool {
            return prune_reader(reader_key(mptr.uri(), mptr.media_source_addr()));
        },

        [=](get_image_atom,
            const media::AVFrameID &mptr,
            const bool
                pin, // stamp the frame 10 minutes in the future so it sticks in the cache
            const utility::Uuid &playhead_uuid,
            const timebase::flicks plahead_position) -> result<ImageBufPtr> {
            auto rp = make_response_promise<media_reader::ImageBufPtr>();
            mail(media_cache::retrieve_atom_v, mptr.key())
                .request(image_cache_, infinite)
                .then(
                    [=](media_reader::ImageBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            // check for existing reader.
                            auto reader = check_cached_reader(
                                reader_key(mptr.uri(), mptr.media_source_addr()));

                            if (reader) {
                                // was using await, not sure why, but I've changed it to then
                                mail(
                                    get_image_atom_v,
                                    mptr,
                                    pin,
                                    playhead_uuid,
                                    plahead_position)
                                    .request(*reader, infinite)
                                    .then(
                                        [=](media_reader::ImageBufPtr buf) mutable {
                                            rp.deliver(buf);
                                        },
                                        [=](const caf::error &err) mutable {
                                            // make an empty image buffer that holds the error
                                            // message
                                            media_reader::ImageBufPtr buf(
                                                new media_reader::ImageBuffer(to_string(err)));
                                            rp.deliver(err);
                                        });
                            } else {
                                // request new reader instance.
                                auto tp = utility::clock::now();
                                mail(get_reader_atom_v, mptr.uri(), mptr.reader())
                                    .request(pool_, infinite)
                                    .then(
                                        [=](caf::actor &new_reader) mutable {
                                            new_reader = add_reader(
                                                new_reader,
                                                reader_key(
                                                    mptr.uri(), mptr.media_source_addr()));

                                            // was using await, not sure why, but I've changed
                                            // it to then
                                            mail(
                                                get_image_atom_v,
                                                mptr,
                                                pin,
                                                playhead_uuid,
                                                plahead_position)
                                                .request(new_reader, infinite)
                                                .then(
                                                    [=](media_reader::ImageBufPtr buf) mutable {
                                                        rp.deliver(buf);
                                                    },
                                                    [=](const caf::error &err) mutable {
                                                        // make an empty image buffer that holds
                                                        // the error message
                                                        media_reader::ImageBufPtr buf(
                                                            new media_reader::ImageBuffer(
                                                                to_string(err)));
                                                        rp.deliver(err);
                                                    });
                                        },
                                        [=](const caf::error &err) mutable {
                                            send_error_to_source(mptr.media_source_addr(), err);

                                            media_reader::ImageBufPtr buf(
                                                new media_reader::ImageBuffer(to_string(err)));
                                            rp.deliver(err);
                                        });
                            }
                        }
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](get_future_frames_atom,
            const media::AVFrameIDsAndTimePoints &mptr_and_timepoints,
            const utility::Uuid & /*playhead_uuid*/
        ) {
            // 'future frames' are requested by the playhead during playback, these
            // are used to prime the viewport textures so we can start uploading
            // pixels to the GPU before we actually need them for display. As
            // such, and because we already have the precache requests being
            // made via another channel, we don't bother asking readers for
            // these images we just get the cache to return whatever it has (and
            // blanks if the images aren't cached)
            return mail(media_cache::retrieve_atom_v, mptr_and_timepoints)
                .delegate(image_cache_);
        },

        [=](get_audio_atom,
            const media::AVFrameIDsAndTimePoints &mptr_and_timepoints,
            const utility::Uuid & /*playhead_uuid*/
        ) {
            // similar to images,during playback we need to fetch several audio frames
            // ahead of time due to soundcard latency. We assume that the 'precache'
            // mechanism has already decoded and stored those frames.
            return mail(media_cache::retrieve_atom_v, mptr_and_timepoints)
                .delegate(audio_cache_);
        },

        [=](get_image_atom,
            const media::AVFrameID &mptr,
            caf::actor playhead,
            const utility::Uuid playhead_uuid,
            const utility::time_point &tp,
            const timebase::flicks playhead_position) {
            mail(media_cache::retrieve_atom_v, mptr.key())
                .request(image_cache_, infinite)
                .then(
                    [=](const media_reader::ImageBufPtr &buf) mutable {
                        if (buf) {
                            mail(push_image_atom_v, buf, mptr, tp, playhead_position)
                                .send(playhead);
                        } else {
                            auto reader = check_cached_reader(
                                reader_key(mptr.uri(), mptr.media_source_addr()));
                            if (reader) {
                                mail(get_image_atom_v, mptr, playhead_uuid)
                                    .request(*reader, infinite)
                                    .then(
                                        [=](const media_reader::ImageBufPtr &buf) mutable {
                                            mail(
                                                push_image_atom_v,
                                                buf,
                                                mptr,
                                                tp,
                                                playhead_position)
                                                .send(playhead);
                                        },
                                        [=](caf::error &err) {});
                            } else {

                                // This prevents a load of requests for new readers to pile up
                                // on pool_ when the same image is requested multiple times
                                auto request_details =
                                    std::make_shared<ImmediateFrameRequest>();
                                auto rkey = reader_key(mptr.uri(), mptr.media_source_addr());
                                request_details->mptr              = mptr;
                                request_details->playhead          = playhead;
                                request_details->playhead_uuid     = playhead_uuid;
                                request_details->tp                = tp;
                                request_details->playhead_position = playhead_position;
                                immediate_frame_requests_[rkey].push_back(request_details);

                                if (immediate_frame_requests_[rkey].size() > 1)
                                    return;

                                // get reader..
                                auto tp2 = utility::clock::now();
                                mail(get_reader_atom_v, mptr.uri(), mptr.reader())
                                    .request(pool_, infinite)
                                    .then(
                                        [=](caf::actor &new_reader) mutable {
                                            new_reader = add_reader(new_reader, rkey);

                                            auto p = immediate_frame_requests_.find(rkey);
                                            if (p == immediate_frame_requests_.end())
                                                return;
                                            auto rr = p->second;
                                            immediate_frame_requests_.erase(p);

                                            for (size_t i = 0; i < rr.size(); ++i) {

                                                std::shared_ptr<ImmediateFrameRequest> r =
                                                    rr[i];

                                                mail(
                                                    get_image_atom_v, r->mptr, r->playhead_uuid)
                                                    .request(new_reader, infinite)
                                                    .then(
                                                        [=](const media_reader::ImageBufPtr
                                                                &buf) mutable {
                                                            mail(
                                                                push_image_atom_v,
                                                                buf,
                                                                r->mptr,
                                                                r->tp,
                                                                r->playhead_position)
                                                                .send(r->playhead);
                                                        },
                                                        [=](caf::error &err) {});
                                            }
                                        },
                                        [=](const caf::error &err) mutable {
                                            send_error_to_source(mptr.media_source_addr(), err);

                                            media_reader::ImageBufPtr buf(
                                                new media_reader::ImageBuffer(to_string(err)));
                                            mail(
                                                push_image_atom_v,
                                                buf,
                                                mptr,
                                                tp,
                                                playhead_position)
                                                .send(playhead);
                                        });
                            }
                        }
                    },
                    [=](const caf::error &err) mutable {
                        spdlog::warn(
                            "Failed cache retrieve buffer {} {}", mptr.key(), to_string(err));
                    });
        },

        [=](get_media_detail_atom _get_media_detail_atom,
            const caf::uri &_uri,
            const caf::actor_addr &key) {
            return mail(_get_media_detail_atom, _uri, key).delegate(media_detail_reader_pool);
        },

        [=](get_thumbnail_atom atom, const media::AVFrameID &mptr, const size_t size) {
            return mail(atom, mptr, size).delegate(thumbnail_reader_pool);
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            return mail(json_store::update_atom_v, full).delegate(actor_cast<caf::actor>(this));
        },

        [&](json_store::update_atom, const JsonStore &json) {
            max_source_count_ =
                preference_value<size_t>(json, "/core/media_reader/max_source_count");
            max_source_age_ =
                preference_value<size_t>(json, "/core/media_reader/max_source_age");
            // mmm_->update_preferences(json);
            prune_readers();
        },

        [=](playback_precache_atom,
            const media::AVFrameID &mptr,
            const utility::time_point &time,
            const Uuid &playhead_uuid) -> result<bool> {
            playback_precache_request_queue_.add_frame_request(mptr, time, playhead_uuid);
            return true;
        },

        [=](playback_precache_atom,
            const media::AVFrameIDsAndTimePoints media_ptrs,
            const Uuid &playhead_uuid,
            const media::MediaType mt) -> result<bool> {
            // we've received fresh lookahead read requests from the playhead
            // during playback. We want to ask the cache actors if they already
            // have those frames, and if not we need to queue read requests to
            // start reading/decoding those frames
            auto rp = make_response_promise<bool>();
            if (mt == MT_IMAGE) {
                mail(media_cache::preserve_atom_v, media_ptrs, playhead_uuid)
                    .request(image_cache_, std::chrono::seconds(1))
                    .await(
                        [=](const media::AVFrameIDsAndTimePoints
                                media_ptrs_not_in_image_cache) mutable {
                            if (media_ptrs_not_in_image_cache.size()) {

                                // clear all pending requests
                                playback_precache_request_queue_.clear_pending_requests(
                                    playhead_uuid);
                                background_precache_request_queue_.clear_pending_requests(
                                    playhead_uuid);

                                playback_precache_request_queue_.add_frame_requests(
                                    media_ptrs_not_in_image_cache, playhead_uuid);

                                if (media_ptrs.size())
                                    background_cached_ref_timepoint_[playhead_uuid] =
                                        media_ptrs.front().first;
                                continue_precacheing();
                            }
                            rp.deliver(true);
                        },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            } else {
                mail(media_cache::preserve_atom_v, media_ptrs, playhead_uuid)
                    .request(audio_cache_, std::chrono::seconds(1))
                    .await(
                        [=](const media::AVFrameIDsAndTimePoints
                                &media_ptrs_not_in_audio_cache) mutable {
                            if (media_ptrs_not_in_audio_cache.size()) {

                                // clear all pending requests
                                playback_precache_request_queue_.clear_pending_requests(
                                    playhead_uuid);
                                background_precache_request_queue_.clear_pending_requests(
                                    playhead_uuid);

                                playback_precache_request_queue_.add_frame_requests(
                                    media_ptrs_not_in_audio_cache, playhead_uuid);

                                if (media_ptrs.size())
                                    background_cached_ref_timepoint_[playhead_uuid] =
                                        media_ptrs.front().first;
                                continue_precacheing();
                            }
                            rp.deliver(true);
                        },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            }
            return rp;
        },

        [=](static_precache_atom,
            const media::AVFrameIDsAndTimePoints mptrs,
            const Uuid playhead_uuid) -> result<bool> {
            auto rp = make_response_promise<bool>();
            auto tt = utility::clock::now();

            // we've been told to start background cacheing - this should only happen
            // when playback has halted and the user isn't actively scrubbing the
            // playhead, so we assume playback read-ahead can be cancelled.
            // We keep a note of the timepoint of the first frame in the request
            // queue - anything with an older time in the cache can be discarded
            // when the cache is full
            if (mptrs.size()) {
                mail(media_cache::unpreserve_atom_v, playhead_uuid)
                    .request(image_cache_, infinite)
                    .then(
                        [=](bool) mutable {
                            background_precache_request_queue_.clear_pending_requests(
                                playhead_uuid);
                            playback_precache_request_queue_.clear_pending_requests(
                                playhead_uuid);
                            background_precache_request_queue_.add_frame_requests(
                                mptrs, playhead_uuid);
                            background_cached_ref_timepoint_[playhead_uuid] =
                                mptrs.front().first;
                            continue_precacheing();
                            rp.deliver(true);
                        },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            } else {
                background_precache_request_queue_.clear_pending_requests(playhead_uuid);
                rp.deliver(false);
            }
            return rp;
        },

        [=](retire_readers_atom) {
            prune_readers();
            anon_mail(retire_readers_atom_v)
                .delay(std::chrono::seconds(max_source_age_))
                .send(this);
        },

        [=](do_precache_work_atom) { do_precache(); },

        [=](utility::uuid_atom) -> Uuid { return uuid_; });
}

std::optional<caf::actor>
GlobalMediaReaderActor::check_cached_reader(const std::string &key, const bool preserve) {
    if (readers_.count(key)) {
        reader_access_[key] = clock::now() + std::chrono::seconds(preserve ? 10 : 0);
        return readers_[key];
    }

    return {};
}

caf::actor GlobalMediaReaderActor::add_reader(
    const caf::actor &reader, const std::string &key, const bool preserve) {
    reader_access_[key] = clock::now() + std::chrono::seconds(preserve ? 10 : 0);

    if (readers_.count(key)) {
        // spdlog::warn("DUPLICATE READER {} {}", key, readers_.count(key));
        // duplicate reader abort..
        send_exit(reader, caf::exit_reason::user_shutdown);
        return readers_[key];
    }

    link_to(reader);
    readers_[key] = reader;
    prune_readers();

    return reader;
}

caf::actor GlobalMediaReaderActor::get_reader(
    const caf::uri &_uri, const caf::actor_addr &_key, const std::string &hint) {
    auto key           = reader_key(_uri, _key);
    auto cached_reader = check_cached_reader(key);

    if (cached_reader)
        return *cached_reader;

    return caf::actor();
}

std::string
GlobalMediaReaderActor::reader_key(const caf::uri &_uri, const caf::actor_addr &_key) const {

    // if _uri is for a 'blank' frame, we don't want to use the media source
    // actor address as a key to cache the reader. The reason is that we might
    // get blank frames for a media source that is still building itself and
    // later we get non-blank frames from it. In other words, if uri is for
    // a blank frame, the uri for the given _key may change later.
    static const auto blank_uri = *caf::make_uri("xstudio://blank/?colour=gray");
    if (_uri == blank_uri)
        return to_string(_uri);

    if (_key) {
        return to_string(_key);
    }
    return to_string(_uri);
}


bool GlobalMediaReaderActor::prune_reader(const std::string &key) {
    auto result = false;
    auto it     = readers_.find(key);

    if (it != std::end(readers_)) {
        result = true;
        unlink_from(it->second);
        send_exit(it->second, caf::exit_reason::user_shutdown);
        reader_access_.erase(it->first);
        readers_.erase(it->first);
    }

    return result;
}

void GlobalMediaReaderActor::prune_readers() {
    utility::time_point now = clock::now();
    bool reaped             = true;

    while (reaped) {
        reaped = false;
        for (auto it = std::begin(reader_access_); it != std::end(reader_access_); ++it) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second) >
                std::chrono::seconds(max_source_age_)) {
                auto a = readers_[it->first];
                unlink_from(a);
                send_exit(a, caf::exit_reason::user_shutdown);
                readers_.erase(it->first);
                spdlog::debug("{}", readers_.count(it->first));
                reaped = true;
                reader_access_.erase(it);
                break;
            }
        }
    }

    while (readers_.size() > max_source_count_) {
        auto rit       = reader_access_.end();
        time_point old = clock::now();
        for (auto it = std::begin(reader_access_); it != std::end(reader_access_); ++it) {
            if (it->second < old) {
                old = it->second;
                rit = it;
            }
        }
        // got oldest..
        if (rit != reader_access_.end()) {
            auto a = readers_[rit->first];
            unlink_from(a);
            send_exit(a, caf::exit_reason::user_shutdown);
            readers_.erase(rit->first);
            reader_access_.erase(rit);
        } else {
            break;
        }
    }
}

void GlobalMediaReaderActor::on_exit() { system().registry().erase(media_reader_registry); }

void GlobalMediaReaderActor::do_precache() {

    // We won't process a new request if there are already precache requests in
    // flight for a given playhead. The reason is the async nature of CAF ...
    // we could send 100s of requests to precache frames (sending messages is
    // fast) before frames can actually be read, decoded and cached (because
    // reading frames is slow) - we would then be in a situation where the CAF
    // mailbox is full of requests to precache frames
    std::optional<FrameRequest> fr = playback_precache_request_queue_.pop_request(
        playheads_with_precache_requests_in_flight_);

    // when putting new images in the cache, images older than this timepoint can
    // be discarded
    bool is_background_cache = false;
    if (not fr) {
        fr = background_precache_request_queue_.pop_request(
            playheads_with_precache_requests_in_flight_);


        if (not fr) {
            return; // global reader is saying pre-cache queue for this reader is empty
        }
        is_background_cache = true;
    }

    const std::shared_ptr<const media::AVFrameID> mptr = fr->requested_frame_;

    const time_point &predicted_time   = fr->required_by_;
    const utility::Uuid &playhead_uuid = fr->requesting_playhead_uuid_;
    time_point cache_out_of_date_threshold =
        utility::clock::now() - std::chrono::milliseconds(10);

    if (background_cached_ref_timepoint_.find(playhead_uuid) !=
        background_cached_ref_timepoint_.end()) {
        cache_out_of_date_threshold =
            background_cached_ref_timepoint_[playhead_uuid] - std::chrono::seconds(1);
    }

    // mark that the playhead is waiting for something. This prevents queuing multiple requests,
    // we only want to send a new request when we've received the previous one.


    // this flag prevents us from making pre-cache read requests while other
    // requests haven't yet been responded to, without needing to use 'await'
    // which would otherwise block this crucial actor

    caf::actor cache_actor =
        mptr->media_type() == media::MediaType::MT_IMAGE ? image_cache_ : audio_cache_;
    mark_playhead_waiting_for_precache_result(playhead_uuid);

    mail(media_cache::preserve_atom_v, mptr->key(), predicted_time, playhead_uuid)
        .request(cache_actor, std::chrono::milliseconds(500))
        .then(

            [=](const bool exists) mutable {
                if (exists) {
                    // already have in the cache, but might still have work to do
                    mark_playhead_received_precache_result(playhead_uuid);
                    // if (is_background_cache) {
                    // keep_cache_hot(mptr.key(), predicted_time, playhead_uuid);
                    // }
                    continue_precacheing();
                } else {
                    try {
                        auto reader =
                            get_reader(mptr->uri(), mptr->media_source_addr(), mptr->reader());
                        if (not reader) {
                            // we need a new reader
                            mail(get_reader_atom_v, mptr->uri(), mptr->reader())
                                .request(pool_, infinite)
                                .then(
                                    [=](caf::actor new_reader) mutable {
                                        auto key =
                                            reader_key(mptr->uri(), mptr->media_source_addr());
                                        new_reader = add_reader(new_reader, key);
                                        if (cache_actor == image_cache_) {
                                            read_and_cache_image(
                                                new_reader,
                                                *fr,
                                                cache_out_of_date_threshold,
                                                is_background_cache);
                                        } else {
                                            read_and_cache_audio(
                                                new_reader,
                                                *fr,
                                                cache_out_of_date_threshold,
                                                is_background_cache);
                                        }
                                    },
                                    [=](caf::error &err) {
                                        mark_playhead_received_precache_result(playhead_uuid);
                                        continue_precacheing();
                                    });
                        } else {
                            if (cache_actor == image_cache_) {
                                read_and_cache_image(
                                    reader,
                                    *fr,
                                    cache_out_of_date_threshold,
                                    is_background_cache);
                            } else {
                                read_and_cache_audio(
                                    reader,
                                    *fr,
                                    cache_out_of_date_threshold,
                                    is_background_cache);
                            }
                        }
                    } catch (std::exception &) {
                        // we have been unable to create a reader - the file is
                        // unreadable for some reason. We do not want to report an
                        // error because we are currently pre-cacheing. The error
                        // *will* get reported when we actaully want to show the
                        // image as an immediate frame request wlil be made as the
                        // image isn't in the cache, and at that point error message
                        // propagation will give the user feedback about the frame
                        // being unreadable

                        // shouldn't it continue... ?
                        // mark_playhead_received_precache_result(playhead_uuid);
                        // continue_precacheing();
                    }
                }
            },
            [=](const caf::error &err) {
                mark_playhead_received_precache_result(playhead_uuid);
                spdlog::warn("Failed preserve buffer {} {}", mptr->key(), to_string(err));
            });
}

void GlobalMediaReaderActor::keep_cache_hot(
    const media::MediaKey &new_entry,
    const utility::time_point &tp,
    const utility::Uuid &playhead_uuid) {

    background_cached_frames_[playhead_uuid].push_back(std::make_pair(new_entry, tp));
    /*anon_mail(media_cache::preserve_atom_v,
        background_cached_frames_[playhead_uuid]).send(image_cache_);*/
}

void GlobalMediaReaderActor::read_and_cache_image(
    caf::actor reader,
    const FrameRequest fr,
    const utility::time_point cache_out_of_date_threshold,
    const bool is_background_cache) {

    const std::shared_ptr<const media::AVFrameID> mptr = fr.requested_frame_;
    const time_point predicted_time                    = fr.required_by_;
    const utility::Uuid playhead_uuid                  = fr.requesting_playhead_uuid_;

    mail(read_precache_image_atom_v, *mptr)
        .request(reader, std::chrono::seconds(60))
        .then(
            [=](media_reader::ImageBufPtr buf) mutable {
                // store the image in our cache. We use a different store message
                // if background cacheing
                if (is_background_cache) {
                    mail(
                        media_cache::store_atom_v,
                        mptr->key(),
                        buf,
                        predicted_time,
                        playhead_uuid,
                        cache_out_of_date_threshold)
                        .request(image_cache_, std::chrono::milliseconds(500))
                        .then(
                            [=](const bool stored) {
                                mark_playhead_received_precache_result(playhead_uuid);

                                if (!stored) {
                                    // cache is full ... stop background cacheing
                                    background_precache_request_queue_.clear_pending_requests(
                                        playhead_uuid);
                                } else {
                                    // still might have work to do
                                    continue_precacheing();
                                }
                            },
                            [=](const caf::error &err) mutable {
                                mark_playhead_received_precache_result(playhead_uuid);
                                spdlog::warn("Cache store error {}", to_string(err));
                            });
                } else {
                    mail(
                        media_cache::store_atom_v,
                        mptr->key(),
                        buf,
                        predicted_time,
                        playhead_uuid)
                        .request(image_cache_, std::chrono::milliseconds(500))
                        .then(
                            [=](const bool stored) {
                                if (!stored) {
                                    // woops, cache is full. Stop pre-reading.
                                    playback_precache_request_queue_.clear_pending_requests(
                                        playhead_uuid);
                                    background_precache_request_queue_.clear_pending_requests(
                                        playhead_uuid);
                                }
                                mark_playhead_received_precache_result(playhead_uuid);

                                // still might have work to do
                                continue_precacheing();
                            },
                            [=](const caf::error &err) mutable {
                                mark_playhead_received_precache_result(playhead_uuid);
                                spdlog::warn("Cache store error {}", to_string(err));
                            });
                }
            },
            [=](const caf::error &err) mutable {
                mark_playhead_received_precache_result(playhead_uuid);
                send_error_to_source(mptr->media_source_addr(), err);
                // we might still have more work to do so keep going
                continue_precacheing();
            });
}

void GlobalMediaReaderActor::read_and_cache_audio(
    caf::actor reader,
    const FrameRequest fr,
    const utility::time_point cache_out_of_date_threshold,
    const bool is_background_cache) {

    const std::shared_ptr<const media::AVFrameID> mptr = fr.requested_frame_;
    const time_point predicted_time                    = fr.required_by_;
    const utility::Uuid playhead_uuid                  = fr.requesting_playhead_uuid_;

    mail(read_precache_audio_atom_v, *mptr)
        .request(reader, std::chrono::seconds(60))
        .then(
            [=](media_reader::AudioBufPtr buf) mutable {
                // store the image in our cache
                mail(
                    media_cache::store_atom_v,
                    mptr->key(),
                    buf,
                    predicted_time,
                    playhead_uuid,
                    cache_out_of_date_threshold)
                    .request(audio_cache_, std::chrono::milliseconds(500))
                    .then(
                        [=](const bool stored) {
                            mark_playhead_received_precache_result(playhead_uuid);

                            if (!stored && is_background_cache) {
                                // cache is full ... stop background cacheing
                                background_precache_request_queue_.clear_pending_requests(
                                    playhead_uuid);
                            } else {
                                continue_precacheing();
                                if (is_background_cache) {
                                    // keep_cache_hot(mptr.key(), predicted_time,
                                    // playhead_uuid);
                                }
                            }
                        },
                        [=](const caf::error &err) mutable {
                            mark_playhead_received_precache_result(playhead_uuid);
                            spdlog::warn("Audio cache store error {}", to_string(err));
                        });
            },
            [=](const caf::error &err) mutable {
                mark_playhead_received_precache_result(playhead_uuid);
                send_error_to_source(mptr->media_source_addr(), err);
                // we might still have more work to do so keep going
                continue_precacheing();
            });
}

void GlobalMediaReaderActor::continue_precacheing() {

    // precache requests are queued up, and when a precache request is processed
    // this function is called to then process the next request, so it's a loop
    // that is enacted by a recursive message send. If the read step is very fast
    // (e.g. for audio decode) the loop becomes 'tight' and other messages
    // coming into this actor can get blocked. This is why a slight delay is
    // employed here - to ensure new incoming messages can get handled and not
    // left in the queue
    anon_mail(do_precache_work_atom_v).send(caf::actor_cast<caf::actor>(this));
}

void GlobalMediaReaderActor::mark_playhead_waiting_for_precache_result(
    const utility::Uuid &playhead_uuid) {

    auto p = playheads_with_precache_requests_in_flight_.find(playhead_uuid);
    if (p == playheads_with_precache_requests_in_flight_.end()) {
        playheads_with_precache_requests_in_flight_[playhead_uuid] = 1;
    } else {
        p->second++;
    }
}

void GlobalMediaReaderActor::mark_playhead_received_precache_result(
    const utility::Uuid &playhead_uuid) {
    auto p = playheads_with_precache_requests_in_flight_.find(playhead_uuid);
    if (p != playheads_with_precache_requests_in_flight_.end()) {
        if (p->second <= 1) {
            playheads_with_precache_requests_in_flight_.erase(p);
        } else {
            p->second--;
        }
    }
}

void GlobalMediaReaderActor::send_error_to_source(
    const caf::actor_addr &addr, const caf::error &err) {
    if (addr) {

        auto dest = caf::actor_cast<caf::actor>(addr);
        if (dest and err.category() == caf::type_id_v<media::media_error>) {
            media_error me;
            from_integer(err.code(), me);
            switch (me) {
            case media_error::corrupt:
                anon_mail(media_status_atom_v, MediaStatus::MS_CORRUPT).send(dest);
                break;
            case media_error::unsupported:
                anon_mail(media_status_atom_v, MediaStatus::MS_UNSUPPORTED).send(dest);
                break;
            case media_error::unreadable:
                anon_mail(media_status_atom_v, MediaStatus::MS_UNREADABLE).send(dest);
                break;
            case media_error::missing:
                anon_mail(media_status_atom_v, MediaStatus::MS_MISSING).send(dest);
                break;
            }
        }
    }
}
