// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <caf/actor_registry.hpp>
#include <thread>

#include "xstudio/media_reader/cacheing_media_reader_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/time_cache.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

using namespace xstudio::media_reader;
using namespace xstudio::media;
using namespace xstudio::json_store;
using namespace xstudio::utility;
using namespace xstudio::global_store;

namespace {

static Uuid blankshader_uuid{"d6c8722b-dc2a-42f9-981d-a2485c6ceea1"};

static std::string blankshader{R"(
#version 330 core
uniform int blank_width;
uniform int dummy;

// forward declaration
uvec4 get_image_data_4bytes(int byte_address);

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    int bytes_per_pixel = 4;
    int pixel_bytes_offset_in_texture_memory = (image_coord.x + image_coord.y*blank_width)*bytes_per_pixel;
    uvec4 c = get_image_data_4bytes(pixel_bytes_offset_in_texture_memory);
    return vec4(float(c.x)/255.0f,float(c.y)/255.0f,float(c.z)/255.0f,1.0f);
}
)"};

static xstudio::ui::viewport::GPUShaderPtr
    blank_shader(new xstudio::ui::opengl::OpenGLShader(blankshader_uuid, blankshader));

ImageBufPtr make_blank_image() {

    ImageBufPtr buf;
    int width             = 192;
    int height            = 108;
    size_t size           = width * height;
    int bytes_per_channel = 1;

    // we are totally free to choose the pixel layout, but we need to unpack
    // in the shader. RGBA 4 bytes matches underlying texture format, so most
    // simple option.
    int bytes_per_pixel = 4 * bytes_per_channel;

    JsonStore jsn;
    jsn["blank_width"] = width;

    buf.reset(new ImageBuffer(blankshader_uuid, jsn));
    buf->allocate(size * bytes_per_pixel);
    buf->set_shader(blank_shader);
    buf->set_image_dimensions(Imath::V2i(width, height));

    std::array<uint8_t, 4> c = {16, 16, 16};
    int i                    = 0;
    uint8_t *b               = (uint8_t *)buf->buffer();
    while (i < size) {
        if (((i / 16) & 1) == (i / (192 * 16) & 1)) {
            b[0] = c[0];
            b[1] = c[1];
            b[2] = c[2];
        } else {
            b[0] = 0;
            b[1] = 0;
            b[2] = 0;
        }
        b += 4; // buf is implicitly rgba
        ++i;
    }
    return buf;
}

} // namespace

CachingMediaReaderActor::WorkerPreferences
CachingMediaReaderActor::worker_preferences(const utility::JsonStore &js) const {
    WorkerPreferences prefs;

    const auto cpu_count = std::max(1U, std::thread::hardware_concurrency());
    prefs.urgent_worker_count_   = std::max<size_t>(1, std::min<size_t>(cpu_count, 4));
    prefs.precache_worker_count_ =
        std::max<size_t>(1, std::min<size_t>(std::max(1U, cpu_count / 2), 2));
    prefs.audio_worker_count_ = 1;

    try {
        prefs.urgent_worker_count_ = std::clamp<size_t>(
            preference_value<size_t>(js, "/core/media_reader/max_worker_count"), 1, 16);
    } catch (...) {
    }

    try {
        prefs.precache_worker_count_ = std::clamp<size_t>(
            preference_value<size_t>(js, "/core/media_reader/precache_worker_count"), 1, 16);
    } catch (...) {
    }

    try {
        prefs.audio_worker_count_ = std::clamp<size_t>(
            preference_value<size_t>(js, "/core/media_reader/audio_worker_count"), 1, 8);
    } catch (...) {
    }

    return prefs;
}

void CachingMediaReaderActor::spawn_worker_pool(
    WorkerPoolState &pool,
    const utility::Uuid &media_reader_plugin_uuid,
    const utility::JsonStore &js,
    size_t worker_count) {
    const auto worker_total =
        std::max<size_t>(1, worker_count);

    auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
    scoped_actor sys{system()};

    pool.workers_.reserve(worker_total);
    pool.busy_.assign(worker_total, false);

    for (size_t worker_index = 0; worker_index < worker_total; ++worker_index) {
        auto worker = request_receive<caf::actor>(
            *sys, pm, plugin_manager::spawn_plugin_atom_v, media_reader_plugin_uuid, js);
        link_to(worker);
        pool.workers_.emplace_back(std::move(worker));
    }
}

caf::actor CachingMediaReaderActor::next_worker(WorkerPoolState &pool) {
    if (pool.workers_.empty())
        return caf::actor{};

    const auto worker_index = pool.next_round_robin_index_ % pool.workers_.size();
    pool.next_round_robin_index_ = (worker_index + 1) % pool.workers_.size();
    return pool.workers_[worker_index];
}

std::optional<size_t> CachingMediaReaderActor::acquire_idle_worker(WorkerPoolState &pool) {
    if (pool.workers_.empty())
        return {};

    for (size_t offset = 0; offset < pool.busy_.size(); ++offset) {
        const auto worker_index = (pool.next_dispatch_index_ + offset) % pool.busy_.size();
        if (!pool.busy_[worker_index]) {
            pool.busy_[worker_index] = true;
            pool.next_dispatch_index_ = (worker_index + 1) % pool.busy_.size();
            return worker_index;
        }
    }

    return {};
}

void CachingMediaReaderActor::release_worker(WorkerPoolState &pool, size_t worker_index) {
    if (worker_index < pool.busy_.size())
        pool.busy_[worker_index] = false;
}

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

    auto prefs = GlobalStoreHelper(system());
    JsonStore js;
    prefs.get_group(js);

    const auto worker_counts = worker_preferences(js);
    spawn_worker_pool(urgent_workers_, media_reader_plugin_uuid, js, worker_counts.urgent_worker_count_);
    spawn_worker_pool(
        precache_workers_, media_reader_plugin_uuid, js, worker_counts.precache_worker_count_);
    spawn_worker_pool(audio_workers_, media_reader_plugin_uuid, js, worker_counts.audio_worker_count_);

    if (not image_cache_)
        image_cache_ = system().registry().template get<caf::actor>(image_cache_registry);
    if (not audio_cache_)
        audio_cache_ = system().registry().template get<caf::actor>(audio_cache_registry);

    // a re-useable blank image to store against failed image reads
    blank_image_ = make_blank_image();

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_image_atom, const ImageBufPtr &blank) {
            // not a get_image request, but setting the blank_image
            blank_image_ = blank;
        },

        [=](get_image_atom) { dispatch_pending_urgent_image_requests(); },

        [=](get_image_atom,
            const media::AVFrameID &mptr,
            bool pin,
            const utility::Uuid &playhead_uuid,
            const timebase::flicks playhead_position) -> result<ImageBufPtr> {
            (void)playhead_position;
            auto rp = make_response_promise<media_reader::ImageBufPtr>();
            auto worker = next_worker(urgent_workers_);
            if (!worker) {
                rp.deliver(make_error(sec::runtime_error, "No urgent media reader workers"));
                return rp;
            }

            // first, check if the image we want is cached
            mail(media_cache::retrieve_atom_v, mptr.key())
                .request(image_cache_, infinite)
                .then(
                    [=](media_reader::ImageBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            mail(get_image_atom_v, mptr)
                                .request(worker, infinite)
                                .then(
                                    [=](media_reader::ImageBufPtr buf) mutable {
                                        rp.deliver(buf);
                                        // store the image in our cache
                                        anon_mail(
                                            media_cache::store_atom_v,
                                            mptr.key(),
                                            buf,
                                            utility::clock::now() +
                                                (pin ? std::chrono::minutes(10)
                                                     : std::chrono::minutes(0)),
                                            playhead_uuid)
                                            .urgent()
                                            .send(image_cache_);
                                    },
                                    [=](const caf::error &err) mutable {
                                        // make an empty image buffer that holds the error
                                        // message
                                        media_reader::ImageBufPtr buf =
                                            make_error_buffer(err, mptr);

                                        // store the failed image in our cache so we don't
                                        // keep trying to load it
                                        anon_mail(
                                            media_cache::store_atom_v,
                                            mptr.key(),
                                            buf,
                                            utility::clock::now() +
                                                (pin ? std::chrono::minutes(10)
                                                     : std::chrono::minutes(0)),
                                            playhead_uuid)
                                            .urgent()
                                            .send(image_cache_);

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
            auto worker = next_worker(audio_workers_);
            if (!worker) {
                rp.deliver(make_error(sec::runtime_error, "No audio media reader workers"));
                return rp;
            }

            // first, check if the image we want is cached
            mail(media_cache::retrieve_atom_v, mptr.key())
                .request(audio_cache_, infinite)
                .then(
                    [=](media_reader::AudioBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            mail(get_audio_atom_v, mptr)
                                .request(worker, infinite)
                                .then(
                                    [=](media_reader::AudioBufPtr buf) mutable {
                                        rp.deliver(buf);
                                        // store the image in our cache
                                        anon_mail(
                                            media_cache::store_atom_v,
                                            mptr.key(),
                                            buf,
                                            utility::clock::now() +
                                                (pin ? std::chrono::minutes(10)
                                                     : std::chrono::minutes(0)),
                                            playhead_uuid)
                                            .send(audio_cache_);
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
            const utility::Uuid playhead_uuid) -> result<ImageBufPtr> {
            return receive_image_buffer_request(mptr, playhead_uuid);
        },

        [=](read_precache_image_atom, const media::AVFrameID &mptr) -> result<ImageBufPtr> {
            // note the caller (GlobalMediaReaderActor) handles the cacheing
            // of this image buffer
            auto rp = make_response_promise<media_reader::ImageBufPtr>();
            auto worker = next_worker(precache_workers_);
            if (!worker) {
                rp.deliver(make_error(sec::runtime_error, "No precache media reader workers"));
                return rp;
            }
            mail(get_image_atom_v, mptr)
                .request(worker, infinite)
                .then(
                    [=](media_reader::ImageBufPtr buf) mutable { rp.deliver(buf); },
                    [=](const caf::error &err) mutable {
                        rp.deliver(make_error_buffer(err, mptr));
                    });

            return rp;
        },

        [=](read_precache_audio_atom, const media::AVFrameID &mptr) -> result<AudioBufPtr> {
            // note the caller (GlobalMediaReaderActor) handles the cacheing
            // of this image buffer
            auto rp = make_response_promise<media_reader::AudioBufPtr>();
            auto worker = next_worker(precache_workers_);
            if (!worker) {
                rp.deliver(make_error(sec::runtime_error, "No precache audio reader workers"));
                return rp;
            }
            mail(get_audio_atom_v, mptr)
                .request(worker, infinite)
                .then(
                    [=](media_reader::AudioBufPtr buf) mutable { rp.deliver(buf); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_detail_atom atom, const caf::uri &_uri) -> result<media::MediaDetail> {
            auto worker = next_worker(urgent_workers_);
            if (!worker)
                return make_error(sec::runtime_error, "No urgent media reader workers");
            return mail(atom, _uri).delegate(worker);
        }

    );
}

void CachingMediaReaderActor::cancel_superseded_request(const utility::Uuid &playhead_uuid) {
    const auto key_it = playhead_pending_image_requests_.find(playhead_uuid);
    if (key_it == playhead_pending_image_requests_.end())
        return;

    const auto pending_it = pending_get_image_requests_.find(key_it->second);
    playhead_pending_image_requests_.erase(key_it);
    if (pending_it == pending_get_image_requests_.end())
        return;

    auto &pending_request = pending_it->second;
    pending_request.waiters_.erase(
        std::remove_if(
            pending_request.waiters_.begin(),
            pending_request.waiters_.end(),
            [&playhead_uuid](const ImmediateImageWaiter &waiter) {
                return waiter.playhead_uuid_ == playhead_uuid;
            }),
        pending_request.waiters_.end());
    if (pending_request.waiters_.empty() && !pending_request.active_) {
        pending_get_image_order_.erase(
            std::remove(
                pending_get_image_order_.begin(),
                pending_get_image_order_.end(),
                pending_it->first),
            pending_get_image_order_.end());
        pending_get_image_requests_.erase(pending_it);
    }
}

void CachingMediaReaderActor::enqueue_urgent_image_request(
    const media::AVFrameID &mptr,
    const utility::Uuid &playhead_uuid,
    caf::typed_response_promise<ImageBufPtr> &rp) {
    cancel_superseded_request(playhead_uuid);

    auto [pending_it, inserted] =
        pending_get_image_requests_.try_emplace(mptr.key(), PendingImmediateImageRequest(mptr));
    auto &pending_request = pending_it->second;
    if (inserted) {
        pending_request.mptr_ = mptr;
    }

    pending_request.waiters_.emplace_back(playhead_uuid, rp);
    playhead_pending_image_requests_[playhead_uuid] = mptr.key();

    if (!pending_request.queued_ && !pending_request.active_) {
        pending_request.queued_ = true;
        pending_get_image_order_.push_back(mptr.key());
    }

    dispatch_pending_urgent_image_requests();
}

void CachingMediaReaderActor::dispatch_pending_urgent_image_requests() {
    while (true) {
        const auto worker_index = acquire_idle_worker(urgent_workers_);
        if (!worker_index.has_value())
            return;

        bool dispatched_request = false;
        while (!pending_get_image_order_.empty()) {
            const auto key = pending_get_image_order_.front();
            pending_get_image_order_.pop_front();

            auto pending_it = pending_get_image_requests_.find(key);
            if (pending_it == pending_get_image_requests_.end())
                continue;

            auto &pending_request = pending_it->second;
            pending_request.queued_ = false;
            if (pending_request.active_ || pending_request.waiters_.empty())
                continue;

            pending_request.active_ = true;
            dispatch_urgent_image_request(key, pending_request, *worker_index);
            dispatched_request = true;
            break;
        }

        if (!dispatched_request) {
            release_worker(urgent_workers_, *worker_index);
            return;
        }
    }
}

void CachingMediaReaderActor::dispatch_urgent_image_request(
    const media::MediaKey &key,
    PendingImmediateImageRequest &request,
    size_t worker_index) {
    mail(get_image_atom_v, request.mptr_)
        .request(urgent_workers_.workers_[worker_index], infinite)
        .then(
            [=](media_reader::ImageBufPtr buf) mutable {
                finish_urgent_image_request(key, worker_index, buf);
            },
            [=](const caf::error &err) mutable {
                finish_urgent_image_request(key, worker_index, ImageBufPtr(), &err);
            });
}

void CachingMediaReaderActor::finish_urgent_image_request(
    const media::MediaKey &key,
    size_t worker_index,
    const ImageBufPtr &buf,
    const caf::error *err) {
    auto pending_it = pending_get_image_requests_.find(key);
    release_worker(urgent_workers_, worker_index);
    if (pending_it == pending_get_image_requests_.end()) {
        dispatch_pending_urgent_image_requests();
        return;
    }

    auto pending_request = std::move(pending_it->second);
    pending_get_image_requests_.erase(pending_it);

    ImageBufPtr result = buf;
    if (err) {
        result = make_error_buffer(*err, pending_request.mptr_);
    }

    std::optional<utility::Uuid> cache_owner;
    for (auto &waiter : pending_request.waiters_) {
        auto key_it = playhead_pending_image_requests_.find(waiter.playhead_uuid_);
        if (key_it != playhead_pending_image_requests_.end() && key_it->second == key) {
            if (!cache_owner.has_value())
                cache_owner = waiter.playhead_uuid_;
            playhead_pending_image_requests_.erase(key_it);
            waiter.response_promise_.deliver(result);
        }
    }

    if (cache_owner.has_value()) {
        anon_mail(
            media_cache::store_atom_v,
            key,
            result,
            utility::clock::now(),
            *cache_owner)
            .urgent()
            .send(image_cache_);
    }

    dispatch_pending_urgent_image_requests();
}

caf::typed_response_promise<ImageBufPtr> CachingMediaReaderActor::receive_image_buffer_request(
    const media::AVFrameID &mptr, const utility::Uuid playhead_uuid) {

    auto rt = make_response_promise<ImageBufPtr>();
    // first, check if the image we want is cached
    mail(media_cache::retrieve_atom_v, mptr.key())
        .request(image_cache_, infinite)
        .then(
            [=](media_reader::ImageBufPtr buf) mutable {
                if (buf) {
                    // send the image back to the playhead that requested it
                    rt.deliver(buf);
                } else {
                    enqueue_urgent_image_request(mptr, playhead_uuid, rt);
                }
            },
            [=](const caf::error &err) mutable {
                rt.deliver(err);
                spdlog::warn(
                    "Failed cache retrieve buffer {} {}",
                    to_string(mptr.key()),
                    to_string(err));
            });
    return rt;
}

ImageBufPtr CachingMediaReaderActor::make_error_buffer(
    const caf::error &err, const media::AVFrameID &mptr) {


    // make an empty image buffer that holds the error
    // message
    std::stringstream err_msg;
    std::string caf_error_string = to_string(err);
    // strip the caf error formatting
    if (caf_error_string.find("error(\"") != std::string::npos) {
        caf_error_string = std::string(caf_error_string, 7);
        // strip off the ") at the end too
        caf_error_string = std::string(caf_error_string, 0, caf_error_string.length() - 2);
    }

    ImageBufPtr err_buf = blank_image_;
    err_buf.set_frame_id(mptr);
    err_buf.set_error_details(caf_error_string);
    return err_buf;

    /*err_msg << "Error loading file \""
            << to_string(mptr.uri())
            << "\": " << caf_error_string;

    return media_reader::ImageBufPtr(new media_reader::ImageBuffer(err_msg.str()));*/
}
