// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

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

    // a re-useable blank image to store against failed image reads
    blank_image_ = make_blank_image();

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_image_atom, const ImageBufPtr &blank) {
            // not a get_image request, but setting the blank_image
            blank_image_ = blank;
        },

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
            mail(media_cache::retrieve_atom_v, mptr.key())
                .request(image_cache_, infinite)
                .then(
                    [=](media_reader::ImageBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            mail(get_image_atom_v, mptr)
                                .request(urgent_worker_, infinite)
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
            // first, check if the image we want is cached
            mail(media_cache::retrieve_atom_v, mptr.key())
                .request(audio_cache_, infinite)
                .then(
                    [=](media_reader::AudioBufPtr buf) mutable {
                        if (buf) {
                            rp.deliver(buf);
                        } else {
                            mail(get_audio_atom_v, mptr)
                                .request(urgent_worker_, infinite)
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
            mail(get_image_atom_v, mptr)
                .request(precache_worker_, infinite)
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
            mail(get_audio_atom_v, mptr)
                .request(precache_worker_, infinite)
                .then(
                    [=](media_reader::AudioBufPtr buf) mutable { rp.deliver(buf); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_detail_atom atom, const caf::uri &_uri) {
            return mail(atom, _uri).delegate(urgent_worker_);
        }

    );
}

void CachingMediaReaderActor::do_urgent_get_image() {

    auto p                      = pending_get_image_requests_.begin();
    const media::AVFrameID mptr = p->second.mptr_;
    auto rp                     = p->second.response_promise_;
    auto playhead_uuid          = p->first;
    pending_get_image_requests_.erase(p);

    urgent_worker_busy_ = true;
    mail(get_image_atom_v, mptr)
        .request(urgent_worker_, infinite)
        .then(
            [=](media_reader::ImageBufPtr buf) mutable {
                // send the image back to the playhead that requested it
                rp.deliver(buf);

                // store the image in our cache
                anon_mail(
                    media_cache::store_atom_v,
                    mptr.key(),
                    buf,
                    utility::clock::now(),
                    playhead_uuid)
                    .urgent()
                    .send(image_cache_);

                // perhaps more urgent requests are now pending
                urgent_worker_busy_ = false;
                anon_mail(get_image_atom_v).send(this);
            },
            [=](const caf::error &err) mutable {
                // make an empty image buffer that holds the error message
                media_reader::ImageBufPtr buf = make_error_buffer(err, mptr);
                rp.deliver(buf);

                // store the failed image in our cache so we don't
                // keep trying to load it
                anon_mail(
                    media_cache::store_atom_v,
                    mptr.key(),
                    buf,
                    utility::clock::now(),
                    playhead_uuid)
                    .urgent()
                    .send(image_cache_);

                urgent_worker_busy_ = false;
                anon_mail(get_image_atom_v).send(this);
            });
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
                    // image is not cached. Update the request to load the image
                    pending_get_image_requests_[playhead_uuid] = ImmediateImageReqest(mptr, rt);
                    mail(get_image_atom_v).send(this);
                }
            },
            [=](const caf::error &err) mutable {
                rt.deliver(err);
                spdlog::warn("Failed cache retrieve buffer {} {}", mptr.key(), to_string(err));
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