// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/cacheing_media_reader_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/time_cache.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio::media_reader;
using namespace xstudio::media;
using namespace xstudio::json_store;
using namespace xstudio::utility;
using namespace xstudio::global_store;


CachingMediaReaderActor::CachingMediaReaderActor(
    caf::actor_config &cfg,
    const utility::Uuid &media_reader_plugin_uuid,
    caf::actor image_cache,
    caf::actor audio_cache)
    : caf::event_based_actor(cfg),
      image_cache_(std::move(image_cache)),
      audio_cache_(std::move(audio_cache)) {

    print_on_exit(this, "CachingMediaReaderActor");
    spdlog::debug("Created CachingMediaReaderActor.");

    // create plugins..
    {
        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        prefs.get_group(js);

        auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
        scoped_actor sys{system()};

        precache_worker_ = request_receive<caf::actor>(
            *sys, pm, plugin_manager::spawn_plugin_atom_v, media_reader_plugin_uuid, js);
        link_to(precache_worker_);
        urgent_worker_ = request_receive<caf::actor>(
            *sys, pm, plugin_manager::spawn_plugin_atom_v, media_reader_plugin_uuid, js);
        link_to(urgent_worker_);
        audio_worker_ = request_receive<caf::actor>(
            *sys, pm, plugin_manager::spawn_plugin_atom_v, media_reader_plugin_uuid, js);
        link_to(audio_worker_);
    }

    if (not image_cache_)
        image_cache_ = system().registry().template get<caf::actor>(image_cache_registry);
    if (not audio_cache_)
        audio_cache_ = system().registry().template get<caf::actor>(audio_cache_registry);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_image_atom) {
            // get the next pending urgent image request
            if (!urgent_worker_busy_ && pending_get_image_requests_.size()) {
                do_urgent_get_image();
            }
        },

        [=](get_image_atom,
            const media::AVFrameID &mptr,
            bool pin,
            const utility::Uuid &playhead_uuid,
            const timebase::flicks playhead_position) -> result<ImageBufPtr> {
            auto rp = make_response_promise<media_reader::ImageBufPtr>();
            // first, check if the image we want is cached
            request(image_cache_, infinite, media_cache::retrieve_atom_v, mptr.key())
                .then(
                    [=](media_reader::ImageBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            request(urgent_worker_, infinite, get_image_atom_v, mptr)
                                .then(
                                    [=](media_reader::ImageBufPtr buf) mutable {
                                        rp.deliver(buf);
                                        // store the image in our cache
                                        anon_send<message_priority::high>(
                                            image_cache_,
                                            media_cache::store_atom_v,
                                            mptr.key(),
                                            buf,
                                            utility::clock::now() +
                                                (pin ? std::chrono::minutes(10)
                                                     : std::chrono::minutes(0)),
                                            playhead_uuid);
                                    },
                                    [=](const caf::error &err) mutable {
                                        // make an empty image buffer that holds the error
                                        // message
                                        std::stringstream err_msg;
                                        std::string caf_error_string = to_string(err);
                                        // strip the caf error formatting
                                        if (caf_error_string.find("error(\"") !=
                                            std::string::npos) {
                                            caf_error_string = std::string(caf_error_string, 7);
                                            // strip off the ") at the end too
                                            caf_error_string = std::string(
                                                caf_error_string,
                                                0,
                                                caf_error_string.length() - 2);
                                        }

                                        err_msg << "Error loading file \""
                                                << to_string(mptr.uri())
                                                << "\": " << caf_error_string;

                                        media_reader::ImageBufPtr buf(
                                            new media_reader::ImageBuffer(err_msg.str()));
                                        rp.deliver(buf);
                                    });
                        }
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_audio_atom,
            const media::AVFrameID mptr,
            bool pin,
            const utility::Uuid playhead_uuid) -> result<AudioBufPtr> {
            auto rp = make_response_promise<media_reader::AudioBufPtr>();
            // first, check if the image we want is cached
            request(audio_cache_, infinite, media_cache::retrieve_atom_v, mptr.key())
                .then(
                    [=](media_reader::AudioBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            request(urgent_worker_, infinite, get_audio_atom_v, mptr)
                                .then(
                                    [=](media_reader::AudioBufPtr buf) mutable {
                                        rp.deliver(buf);
                                        // store the image in our cache
                                        anon_send(
                                            audio_cache_,
                                            media_cache::store_atom_v,
                                            mptr.key(),
                                            buf,
                                            utility::clock::now() +
                                                (pin ? std::chrono::minutes(10)
                                                     : std::chrono::minutes(0)),
                                            playhead_uuid);
                                    },
                                    [=](const caf::error &) mutable {
                                        // deliver an empty buffer
                                        rp.deliver(buf);
                                    });
                        }
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_image_atom,
            const media::AVFrameID &mptr,
            caf::actor playhead,
            const utility::Uuid playhead_uuid,
            const time_point &tp,
            const timebase::flicks playhead_position) {
            receive_image_buffer_request(mptr, playhead, playhead_uuid, tp, playhead_position);
        },

        [=](read_precache_image_atom, const media::AVFrameID &mptr) -> result<ImageBufPtr> {
            // note the caller (GlobalMediaReaderActor) handles the cacheing
            // of this image buffer
            auto rp = make_response_promise<media_reader::ImageBufPtr>();
            request(precache_worker_, infinite, get_image_atom_v, mptr)
                .then(
                    [=](media_reader::ImageBufPtr buf) mutable { rp.deliver(buf); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](read_precache_audio_atom, const media::AVFrameID &mptr) -> result<AudioBufPtr> {
            // note the caller (GlobalMediaReaderActor) handles the cacheing
            // of this image buffer
            auto rp = make_response_promise<media_reader::AudioBufPtr>();
            request(precache_worker_, infinite, get_audio_atom_v, mptr)
                .then(
                    [=](media_reader::AudioBufPtr buf) mutable { rp.deliver(buf); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_detail_atom atom, const caf::uri &_uri) {
            delegate(urgent_worker_, atom, _uri);
        }

    );
}

void CachingMediaReaderActor::do_urgent_get_image() {

    auto p                      = pending_get_image_requests_.begin();
    const media::AVFrameID mptr = p->second.mptr_;
    caf::actor playhead         = p->second.playhead_;
    auto tp                     = p->second.time_point_;
    auto playhead_uuid          = p->first;
    auto tps                    = p->second.playhead_position_;
    pending_get_image_requests_.erase(p);

    urgent_worker_busy_ = true;
    request(urgent_worker_, infinite, get_image_atom_v, mptr)
        .then(
            [=](media_reader::ImageBufPtr buf) mutable {
                // send the image back to the playhead that requested it
                send(playhead, push_image_atom_v, buf, mptr, tp, tps);

                // store the image in our cache
                anon_send<message_priority::high>(
                    image_cache_,
                    media_cache::store_atom_v,
                    mptr.key(),
                    buf,
                    utility::clock::now(),
                    playhead_uuid);

                // perhaps more urgent requests are now pending
                urgent_worker_busy_ = false;
                anon_send(this, get_image_atom_v);
            },
            [=](const caf::error &err) mutable {
                std::stringstream err_msg;
                std::string caf_error_string = to_string(err);
                // strip the caf error formatting
                if (caf_error_string.find("error(\"") != std::string::npos) {
                    caf_error_string = std::string(caf_error_string, 7);
                    // strip off the ") at the end too
                    caf_error_string =
                        std::string(caf_error_string, 0, caf_error_string.length() - 2);
                }

                err_msg << "Error loading file \"" << to_string(mptr.uri())
                        << "\": " << caf_error_string;

                // make an empty image buffer that holds the error message
                media_reader::ImageBufPtr buf(new media_reader::ImageBuffer(err_msg.str()));

                // send the image back to the playhead that requested it
                send(playhead, push_image_atom_v, buf, mptr, tp, tps);

                urgent_worker_busy_ = false;
                anon_send(this, get_image_atom_v);
            });
}

void CachingMediaReaderActor::receive_image_buffer_request(
    const media::AVFrameID &mptr,
    caf::actor playhead,
    const utility::Uuid playhead_uuid,
    const time_point &tp,
    const timebase::flicks playhead_position) {

    // first, check if the image we want is cached
    request(image_cache_, infinite, media_cache::retrieve_atom_v, mptr.key())
        .then(
            [=](media_reader::ImageBufPtr buf) mutable {
                if (buf) {
                    std::string path = uri_to_posix_path(mptr.uri());
                    // send the image back to the playhead that requested it
                    send(playhead, push_image_atom_v, buf, mptr, tp, playhead_position);
                } else {
                    // image is not cached. Update the request to load the image
                    pending_get_image_requests_[playhead_uuid] =
                        ImmediateImageReqest(mptr, playhead, tp, playhead_position);
                    send(this, get_image_atom_v);
                }
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("Failed cache retrieve buffer {} {}", mptr.key(), to_string(err));
            });
}