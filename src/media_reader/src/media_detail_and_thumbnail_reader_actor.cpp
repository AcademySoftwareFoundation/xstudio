// SPDX-License-Identifier: Apache-2.0
#include <caf/sec.hpp>
#include <caf/policy/select_all.hpp>
#include <limits>


#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/caf_media_error.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/media_reader/media_detail_and_thumbnail_reader_actor.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace std::chrono_literals;

using namespace caf;

using namespace xstudio;
using namespace xstudio::media;
using namespace xstudio::global_store;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media_reader;
using namespace xstudio::media_cache;

MediaDetailAndThumbnailReaderActor::MediaDetailAndThumbnailReaderActor(
    caf::actor_config &cfg, const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), uuid_(uuid) {
    print_on_exit(this, NAME);
    spdlog::debug("Created {}.", NAME);

    if (uuid_.is_null())
        uuid_.generate_in_place();

    {
        // spawn an instance of each media reader plugin that is available
        auto pm = system().registry().get<caf::actor>(plugin_manager_registry);
        scoped_actor sys{system()};
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_READER));

        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        prefs.get_group(js);

        for (const auto &i : details) {
            if (i.enabled_) {
                auto actor = request_receive<caf::actor>(
                    *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_, js);
                link_to(actor);
                plugins_.push_back(actor);
                plugins_map_[i.uuid_] = actor;
            }
        }
    }

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_media_detail_atom) {
            // here we process a thumbnail request once for every 4 'get_media_detail' requests
            // - this is to balance thumbnail delivery to the UI (which is nice but not
            // essential) against building media sources (which is essential)
            if (num_detail_requests_since_thumbnail_request_ > 4 &&
                !thumbnail_request_queue_.empty()) {
                process_get_thumbnail_queue();
            } else if (not media_detail_request_queue_.empty()) {
                process_get_media_detail_queue();
            } else {
                process_get_thumbnail_queue();
            }
        },

        [=](get_media_detail_atom _get_media_detail_atom,
            const caf::uri &_uri,
            const caf::actor_addr &key) -> result<MediaDetail> {
            // clear old entries.
            auto prune_age = utility::clock::now() - std::chrono::seconds(120);
            for (auto i = media_detail_cache_age_.begin();
                 i != media_detail_cache_age_.end();) {
                if (i->second < prune_age) {
                    media_detail_cache_.erase(i->first);
                    i = media_detail_cache_age_.erase(i);
                } else {
                    i++;
                }
            }

            if (media_detail_cache_.count(_uri)) {
                media_detail_cache_age_[_uri] = utility::clock::now();
                return media_detail_cache_[_uri];
            }

            // Add the request to a queue - if the queues aren't empty at this
            // point the loop to process the queues is already underway so we
            // don't want to send ourselves another message to start the
            // processing or we will start making read requests to the plugins
            // while other read requests are not complete
            bool start = queues_empty();
            auto rp    = make_response_promise<MediaDetail>();
            media_detail_request_queue_.emplace(_uri, key, rp);
            if (start)
                anon_send(
                    caf::actor_cast<caf::actor>(this),
                    get_media_detail_atom_v); // starts loop to chew through the request queue
            return rp;
        },

        [=](get_thumbnail_atom,
            const media::AVFrameID &mptr,
            const size_t size) -> result<thumbnail::ThumbnailBufferPtr> {
            bool start = queues_empty();
            auto rp    = make_response_promise<thumbnail::ThumbnailBufferPtr>();
            thumbnail_request_queue_.emplace(mptr, size, rp);
            if (start)
                anon_send(
                    caf::actor_cast<caf::actor>(this),
                    get_media_detail_atom_v); // starts loop to chew through the request queue
            return rp;
        },

        [=](utility::uuid_atom) -> Uuid { return uuid_; });
}

void MediaDetailAndThumbnailReaderActor::process_get_thumbnail_queue() {

    if (!thumbnail_request_queue_.size())
        return;

    num_detail_requests_since_thumbnail_request_ = 0;

    const auto thumbnail_request = thumbnail_request_queue_.front();
    thumbnail_request_queue_.pop();

    media::AVFrameID mptr = thumbnail_request.media_pointer_;
    const size_t size     = thumbnail_request.size_;
    caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp = thumbnail_request.rp_;

    try {
        fan_out_request<policy::select_all>(
            plugins_,
            infinite,
            media_reader::supported_atom_v,
            mptr.uri_,
            utility::get_signature(mptr.uri_))
            .then(
                [=](std::vector<std::pair<xstudio::utility::Uuid, MRCertainty>> supt) mutable {
                    // find the best media reader plugin to read this file ...
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
                        rp.deliver(make_error(media_error::unsupported, "Unsupported format"));
                        continue_processing_queue();
                    } else {
                        get_thumbnail_from_reader_plugin(
                            plugins_map_[best_reader_plugin_uuid], mptr, size, rp);
                    }
                },
                [=](const caf::error &err) mutable {
                    spdlog::warn("{} {}", err.category(), to_string(err));
                    rp.deliver(err);
                    continue_processing_queue();
                });
    } catch (std::exception &e) {
        rp.deliver(make_error(media_error::unsupported, e.what()));
        // spdlog::info("{} {}", __PRETTY_FUNCTION__, e.what());
        continue_processing_queue();
    }
}

void MediaDetailAndThumbnailReaderActor::get_thumbnail_from_reader_plugin(
    caf::actor &reader_plugin,
    const media::AVFrameID mptr,
    const size_t size,
    caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp) {

    auto colour_pipe_manager = system().registry().get<caf::actor>(colour_pipeline_registry);
    request(reader_plugin, infinite, get_thumbnail_atom_v, mptr, size)
        .then(
            [=](const thumbnail::ThumbnailBufferPtr &buf) mutable {
                if (buf && buf->format() == thumbnail::THUMBNAIL_FORMAT::TF_RGB24)
                    rp.deliver(buf);
                else if (buf) {
                    // send to colour pipeline..
                    rp.delegate(colour_pipe_manager, process_thumbnail_atom_v, mptr, buf);
                } else {
                    if (mptr.actor_addr_) {
                        auto dest = caf::actor_cast<caf::actor>(mptr.actor_addr_);
                        if (dest)
                            anon_send(dest, media_status_atom_v, MediaStatus::MS_UNSUPPORTED);
                    }
                    rp.deliver(make_error(
                        media_error::corrupt, "thumbnail loaded returned empty buffer."));
                }
                continue_processing_queue();
            },
            [=](const caf::error &err) mutable {
                spdlog::error("{} {}", err.category(), to_string(err));
                rp.deliver(err);
                continue_processing_queue();
            });
}

void MediaDetailAndThumbnailReaderActor::process_get_media_detail_queue() {

    if (media_detail_request_queue_.empty())
        return;

    num_detail_requests_since_thumbnail_request_++;
    const auto media_detail_request = media_detail_request_queue_.front();
    media_detail_request_queue_.pop();

    const caf::uri _uri                         = media_detail_request.uri_;
    const caf::actor_addr key                   = media_detail_request.key_;
    caf::typed_response_promise<MediaDetail> rp = media_detail_request.rp_;

    try {
        fan_out_request<policy::select_all>(
            plugins_,
            infinite,
            media_reader::supported_atom_v,
            _uri,
            utility::get_signature(_uri))
            .then(
                [=](std::vector<std::pair<xstudio::utility::Uuid, MRCertainty>> supt) mutable {
                    // find the best media reader plugin to read this file ...
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
                        rp.deliver(make_error(media_error::unsupported, "Unsupported format"));
                        continue_processing_queue();
                    } else {
                        request(
                            plugins_map_[best_reader_plugin_uuid],
                            infinite,
                            get_media_detail_atom_v,
                            _uri)
                            .then(
                                [=](const MediaDetail &md) mutable {
                                    media_detail_cache_age_[_uri] = utility::clock::now();
                                    media_detail_cache_[_uri]     = md;
                                    rp.deliver(md);
                                    continue_processing_queue();
                                },
                                [=](const caf::error &err) mutable {
                                    rp.deliver(err);
                                    send_error_to_source(key, err);
                                    continue_processing_queue();
                                });
                    }
                },
                [=](const caf::error &err) mutable {
                    rp.deliver(err);
                    send_error_to_source(key, err);
                    continue_processing_queue();
                });
    } catch (std::exception &e) {
        auto err = make_error(xstudio_error::error, e.what());
        send_error_to_source(key, err);
        continue_processing_queue();
        rp.deliver(err);
    }
}

void MediaDetailAndThumbnailReaderActor::send_error_to_source(
    const caf::actor_addr &addr, const caf::error &err) {
    if (addr) {
        auto dest = caf::actor_cast<caf::actor>(addr);
        if (dest and err.category() == caf::type_id_v<media::media_error>) {
            media_error me;
            from_integer(err.code(), me);
            switch (me) {
            case media_error::corrupt:
                anon_send(dest, media_status_atom_v, MediaStatus::MS_CORRUPT);
                break;
            case media_error::unsupported:
                anon_send(dest, media_status_atom_v, MediaStatus::MS_UNSUPPORTED);
                break;
            case media_error::unreadable:
                anon_send(dest, media_status_atom_v, MediaStatus::MS_UNREADABLE);
                break;
            case media_error::missing:
                anon_send(dest, media_status_atom_v, MediaStatus::MS_MISSING);
                break;
            }
        }
    }
}

void MediaDetailAndThumbnailReaderActor::continue_processing_queue() {

    if (!queues_empty()) {
        anon_send(caf::actor_cast<caf::actor>(this), get_media_detail_atom_v);
    }
}
