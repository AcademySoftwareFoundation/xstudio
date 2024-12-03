// SPDX-License-Identifier: Apache-2.0
#include <sstream>

#include <caf/policy/select_all.hpp>

#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/pixel_info.hpp"
#include "xstudio/media_reader/image_buffer.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio;

std::string LUTDescriptor::as_string() const {

    std::stringstream r;
    r << data_type_ << "_" << dimension_ << "_" << channels_ << "_" << interpolation_ << "_"
      << xsize_ << "_" << ysize_ << "_" << zsize_;
    return r.str();
}


ColourPipeline::ColourPipeline(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : StandardPlugin(cfg, "ColourPipeline", init_settings), uuid_(utility::Uuid::generate()) {

    cache_ = system().registry().template get<caf::actor>(colour_cache_registry);

    // this ensures colour OPs get loaded
    load_colour_op_plugins();
}

ColourPipeline::~ColourPipeline() {}

size_t ColourOperationData::size() const {
    size_t rt = 0;
    for (const auto &lut : luts_) {
        rt += lut->data_size();
    }
    return rt;
}

caf::message_handler ColourPipeline::message_handler_extensions() {

    return caf::message_handler(
        [=](colour_pipe_linearise_data_atom,
            const media::AVFrameID &media_ptr) -> result<ColourOperationDataPtr> {
            /* Call virtual method to make the linearise transform data object */
            auto rp = make_response_promise<ColourOperationDataPtr>();
            linearise_op_data(rp, media_ptr);
            return rp;
        },
        [=](colour_pipe_display_data_atom,
            const media::AVFrameID &media_ptr) -> result<ColourOperationDataPtr> {
            /* Call virtual method to make the display transform data object */
            auto rp = make_response_promise<ColourOperationDataPtr>();
            linear_to_display_op_data(rp, media_ptr);
            return rp;
        },

        // This call takes an image ptr, and returns a copy of the image ptr
        // but with all colour management data filled in.
        // Note that we do this in two steps. The ColourPipelineDataPtr carries
        // static colour pipe data, like LUTs and shader code. This data is 
        // generally expensive to generate and is pre-prepared and cached by
        // this actor before we need it a draw-time.
        // The colour_pipe_unirorms is dynamic colour management data - stuff like
        // the viewer exposure, gamma etc. This should be fast to compute and
        // is not cached but provided on-demand.
        [=](get_colour_pipe_data_atom,
            const media_reader::ImageBufPtr image) -> caf::result<media_reader::ImageBufPtr> {

            auto rp = make_response_promise<media_reader::ImageBufPtr>();

            request(self(), infinite, get_colour_pipe_data_atom_v, image.frame_id()).then(
                [=](const ColourPipelineDataPtr ptr) mutable {
                    media_reader::ImageBufPtr result = image;
                    result.colour_pipe_data_ = ptr;

                    request(self(), infinite, colour_operation_uniforms_atom_v, result).then(
                        [=](const utility::JsonStore & colour_pipe_uniforms) mutable {
                            result.colour_pipe_uniforms_ = colour_pipe_uniforms;
                            rp.deliver(result);
                        },
                        [=](caf::error &err) mutable {
                            rp.deliver(err);
                        });                    
                },
                [=](caf::error &err) mutable {
                    rp.deliver(err);
                });

            return rp;
        },

        [=](get_colour_pipe_data_atom,
            const media::AVFrameID &media_ptr) -> caf::result<ColourPipelineDataPtr> {
            auto rp = make_response_promise<ColourPipelineDataPtr>();

            // This message handler is crucial to xSTUDIO playback performance!

            // We have incoming requests for the full set of data for colour
            // managed display of a video frame - the returned data is shader
            // source code and associated LUTs needed to transform the image
            // on the GPU from source->linear->display. Between linear and display
            // we may also need addition shader code provided by other colour
            // operatiopn plugins.

            // Note that the incoming requests are made immediately before draw
            // time, as well as well ahead of draw time (during playback) so we
            // can compute and cache the data ready for a quickly returning it
            // later at draw time.

            // The computation of this data can be expensive (like loading and
            // parsing LUTs). Thus, we must cache the data. We also must be careful
            // that we don't block on the slow computatio part, so we pass this
            // expensive bit onto a worker pool - this leaves us free to quickly
            // returnon requests that are already cached.

            // this transform_id must be unique to the source as well as the
            // state of the ColourPipeline class where that state affects the
            // colour transform (for example, the id should be based on the
            // media source colourspce and the OCIO View and Display properties
            // of the ColourPipeline.
            std::string transform_id = std::to_string(fast_display_transform_hash(media_ptr));

            // check if we have already received a request for this data but are
            // still waiting for workers to finish. Stores the response promise
            // so we can 'deliver' on it when we do get the result from previous
            // requests
            if (add_in_flight_request(transform_id, rp)) {
                return rp;
            }

            // now try and retrieve the data we need from the cache_, if indeed
            // it has been cached
            if (make_colour_pipe_data_from_cached_data(media_ptr, transform_id)) {
                return rp;
            }

            request(self(), infinite, colour_pipe_linearise_data_atom_v, media_ptr)
                .then(
                    [=](ColourOperationDataPtr linearise_data) mutable {
                        request(self(), infinite, colour_pipe_display_data_atom_v, media_ptr)
                            .then(
                                [=](ColourOperationDataPtr &to_display_data) mutable {
                                    // when colour operations are applied in the
                                    // GPU display shader, we need to know what
                                    // order they happen in. Linearise operation
                                    // always comes first. Display transform is
                                    // the last to happen before fragments hit
                                    // the screen. In between, we may have other
                                    // operations (applied in linear space)
                                    // like gamma, saturation or other grading
                                    // ops
                                    linearise_data->order_index_ =
                                        std::numeric_limits<float>::lowest();
                                    to_display_data->order_index_ =
                                        std::numeric_limits<float>::max();

                                    add_cache_keys(
                                        transform_id,
                                        linearise_data->cache_id(),
                                        to_display_data->cache_id());
                                    anon_send<message_priority::high>(
                                        cache_,
                                        media_cache::store_atom_v,
                                        to_display_data->cache_id(),
                                        to_display_data);
                                    anon_send<message_priority::high>(
                                        cache_,
                                        media_cache::store_atom_v,
                                        linearise_data->cache_id(),
                                        linearise_data);

                                    finalise_colour_pipeline_data(
                                        media_ptr,
                                        transform_id,
                                        linearise_data,
                                        to_display_data);
                                },
                                [=](caf::error &err) mutable {
                                    deliver_on_reponse_promises(err, transform_id);
                                });
                    },
                    [=](caf::error &err) mutable {
                        deliver_on_reponse_promises(err, transform_id);
                    });

            return rp;
        },


        [=](display_colour_transform_hash_atom,
            const media::AVFrameID &media_ptr) -> result<size_t> {
            auto rp = make_response_promise<size_t>();
            if (thumbnail_processor_pool_) {
                rp.delegate(
                    thumbnail_processor_pool_, display_colour_transform_hash_atom_v, media_ptr);
                return rp;
            }
            rp.deliver(fast_display_transform_hash(media_ptr));
            return rp;
        },
        [=](media_reader::process_thumbnail_atom,
            const media::AVFrameID &mptr,
            const thumbnail::ThumbnailBufferPtr &buf) -> result<thumbnail::ThumbnailBufferPtr> {
            auto rp = make_response_promise<thumbnail::ThumbnailBufferPtr>();
            process_thumbnail(rp, mptr, buf);
            return rp;
        },
        [=](pixel_info_atom atom,
            const media_reader::PixelInfo &pixel_info,
            const media::AVFrameID &mptr) -> result<media_reader::PixelInfo> {
            auto rp = make_response_promise<media_reader::PixelInfo>();
            if (pixel_probe_worker_) {
                rp.delegate(pixel_probe_worker_, atom, pixel_info, mptr);
            } else {
                auto p = pixel_info;
                extend_pixel_info(p, mptr);
                rp.deliver(p);
            }
            return rp;
        },
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {
            // nop
        },
        [=](const caf::error &reason) {
            spdlog::debug("ColourPipelineActor exited: {}", to_string(reason));
        },
        [=](colour_operation_uniforms_atom atom,
            const media_reader::ImageBufPtr &image) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();

            if (!image.colour_pipe_data_) {
                rp.deliver(utility::JsonStore());
                return rp;
            }

            auto result = std::make_shared<utility::JsonStore>();
            for (const auto &op : image.colour_pipe_data_->operations()) {
                result->merge(update_shader_uniforms(image, op->user_data_));
            }
            auto rcount = std::make_shared<int>((int)colour_op_plugins_.size());

            if (!colour_op_plugins_.size()) {
                rp.deliver(*result);
                return rp;
            }
            for (auto &colour_op_plugin : colour_op_plugins_) {

                request(colour_op_plugin, infinite, colour_operation_uniforms_atom_v, image)
                    .then(
                        [=](const utility::JsonStore &uniforms) mutable {
                            result->merge(uniforms);
                            (*rcount)--;
                            if (!(*rcount)) {
                                rp.deliver(*result);
                            }
                        },
                        [=](const caf::error &err) mutable {
                            (*rcount) = 0;
                            rp.deliver(err);
                        });
            }

            return rp;
        },
        [=](get_colour_pipe_data_atom,
            const media::AVFrameIDsAndTimePoints &mptr_and_timepoints)
            -> caf::result<std::vector<ColourPipelineDataPtr>> {
            // Here we fetch colour pipeline data (Shaders, LUTS) for a bunch of media
            // pointers. We need this to fetch 'future frames' so we have pixel and LUT
            // data ready to upload to the GPU some time before we need it for drawing.
            auto rp = make_response_promise<std::vector<ColourPipelineDataPtr>>();

            auto result = std::make_shared<std::vector<ColourPipelineDataPtr>>(
                mptr_and_timepoints.size());
            auto rcount = std::make_shared<int>((int)mptr_and_timepoints.size());
            auto self   = caf::actor_cast<caf::actor>(this);

            int outer_idx = 0;

            for (const auto &mptr_and_tp : mptr_and_timepoints) {
                if (*rcount == 0) {
                    break;
                }

                const int idx = outer_idx++;

                request(self, infinite, get_colour_pipe_data_atom_v, *(mptr_and_tp.second))
                    .then(
                        [=](const ColourPipelineDataPtr &cpd) mutable {
                            (*result)[idx] = cpd;
                            (*rcount)--;
                            if (!(*rcount)) {
                                rp.deliver(*result);
                            }
                        },
                        [=](const caf::error &err) mutable {
                            (*rcount) = 0;
                            rp.deliver(err);
                        });
            }

            return rp;
        },
        [=](playhead::media_source_atom,
            const utility::UuidActor &media_source) -> result<bool> {
            // this message handler lets you force the colour pipeline actor
            // to update based on a given media actor and get a response when
            // it has finished updating. It's used by the offscreen viewport,
            // for example, to make the OCIO view/display attributes to update
            // for a given media source so we can show the view/display options
            // in the snapshot dialog
            auto rp = make_response_promise<bool>();

            if (media_source) {
                request(media_source.actor(), infinite, get_colour_pipe_params_atom_v)
                    .then(
                        [=](const utility::JsonStore colour_params) mutable {
                            fan_out_request<policy::select_all>(
                                colour_op_plugins_,
                                infinite,
                                playhead::media_source_atom_v,
                                media_source.actor(),
                                media_source.uuid(),
                                colour_params)
                                .then(
                                    [=](const std::vector<bool>) mutable {
                                        media_source_changed(media_source, colour_params);
                                        rp.deliver(true);
                                    },
                                    [=](const caf::error &err) mutable { rp.deliver(err); });
                        },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            } else {
                rp.deliver(false);
            }
            return rp;
        },
        [=](utility::event_atom,
            playhead::media_source_atom,
            caf::actor media_actor,
            const utility::Uuid &media_uuid) {
            // This message comes from the playhead when the onscreen (key)
            // media source has changed.
            if (media_actor and media_uuid) {
                request(media_actor, infinite, get_colour_pipe_params_atom_v)
                    .then(
                        [=](const utility::JsonStore &colour_params) mutable {
                            for (auto &colour_op_plugin : colour_op_plugins_) {
                                anon_send(
                                    colour_op_plugin,
                                    utility::event_atom_v,
                                    playhead::media_source_atom_v,
                                    media_actor,
                                    media_uuid,
                                    colour_params);
                            }

                            media_source_changed(media_uuid, colour_params);
                        },
                        [=](const caf::error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            }
        },
        [=](connect_to_viewport_atom,
            const std::string &viewport_name,
            const std::string &viewport_toolbar_name,
            bool connect,
            caf::actor viewport) {
            disable_linking();
            connect_to_viewport(viewport_name, viewport_toolbar_name, connect, viewport);
            enable_linking();
            connect_to_ui();
        },
        [=](xstudio::ui::viewport::screen_info_atom,
            const std::string &name,
            const std::string &model,
            const std::string &manufacturer,
            const std::string &serialNumber) {
            screen_changed(name, model, manufacturer, serialNumber);
        },
        [=](media_reader::process_thumbnail_atom,
            const media::AVFrameID &mptr,
            const thumbnail::ThumbnailBufferPtr &buf) {
            delegate(
                thumbnail_processor_pool_, media_reader::process_thumbnail_atom_v, mptr, buf);
        },
        [=](json_store::update_atom,
            const utility::JsonStore & /*change*/,
            const std::string & /*path*/,
            const utility::JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },
        [=](ui::viewport::viewport_playhead_atom, caf::actor playhead) {
            // TODO: set something up to listen to playhead pre-compute colour
        },
        [=](json_store::update_atom, const utility::JsonStore &) mutable {},
        [=](utility::serialise_atom) -> utility::JsonStore { return serialise(); },
        [=](ui::viewport::pre_render_gpu_hook_atom) -> result<plugin::GPUPreDrawHookPtr> {
            // This message handler overrides the one in PluginBase class.
            // op plugins themselves might have a GPUPreDrawHook that needs
            // to be passed back up to the Viewport object that is making this
            // request. Due to asynchronous nature of the plugin loading
            // (see load_colour_op_plugins) we therefore need our own logic here.
            auto rp = make_response_promise<plugin::GPUPreDrawHookPtr>();
            if (colour_ops_loaded_) {
                make_pre_draw_gpu_hook(rp);
            } else {
                // add to a queue of these requests pending a response
                hook_requests_.push_back(rp);
                // load_colour_op_plugins() will respond to these requests
                // when all the plugins are loaded.
            }
            return rp;
        });
}

void ColourPipeline::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    StandardPlugin::attribute_changed(attr_uuid, role);
}

bool ColourPipeline::add_in_flight_request(
    const std::string &transform_id, caf::typed_response_promise<ColourPipelineDataPtr> rp) {
    in_flight_requests_[transform_id].push_back(rp);
    return in_flight_requests_[transform_id].size() > 1; 
}

bool ColourPipeline::make_colour_pipe_data_from_cached_data(
    const media::AVFrameID &media_ptr, const std::string &transform_id) {
    auto p = cache_keys_cache_.find(transform_id);
    if (p == cache_keys_cache_.end())
        return false;

    try {
        caf::scoped_actor sys(system());
        const std::string &linearise_transform_cache_id = p->second.first;
        const std::string &display_transform_cache_id   = p->second.second;

        auto linearise_data = utility::request_receive<ColourOperationDataPtr>(
            *sys, cache_, media_cache::retrieve_atom_v, linearise_transform_cache_id);
        auto to_display_data = utility::request_receive<ColourOperationDataPtr>(
            *sys, cache_, media_cache::retrieve_atom_v, display_transform_cache_id);

        if (linearise_data && to_display_data) {
            finalise_colour_pipeline_data(
                media_ptr, transform_id, linearise_data, to_display_data);
            return true;
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return false;
}

void ColourPipeline::add_cache_keys(
    const std::string &transform_id,
    const std::string &linearise_transform_cache_id,
    const std::string &display_transform_cache_id) {
    cache_keys_cache_[transform_id] =
        std::make_pair(linearise_transform_cache_id, display_transform_cache_id);
}

void ColourPipeline::finalise_colour_pipeline_data(
    const media::AVFrameID &media_ptr,
    const std::string &transform_id,
    ColourOperationDataPtr &linearise_data,
    ColourOperationDataPtr &to_display_data) {

    auto *data = new ColourPipelineData;
    ColourPipelineDataPtr result;
    result.reset(data);
    linearise_data->order_index_  = std::numeric_limits<float>::lowest();
    to_display_data->order_index_ = std::numeric_limits<float>::max();
    result->add_operation(linearise_data);
    result->add_operation(to_display_data);
    result->set_cache_id(linearise_data->cache_id() + to_display_data->cache_id());

    add_colour_op_plugin_data(media_ptr, result, transform_id);
}

void ColourPipeline::add_colour_op_plugin_data(
    const media::AVFrameID media_ptr,
    ColourPipelineDataPtr result,
    const std::string transform_id) {

    // now we need colour_op_plugins_ to add in their own data ...
    if (!colour_op_plugins_.size()) {
        deliver_on_reponse_promises(result, transform_id);
        return;
    }

    // now extend colour pipe data with colour op plugin
    // data
    auto rcount = std::make_shared<int>((int)colour_op_plugins_.size());

    for (auto &colour_op_plugin : colour_op_plugins_) {

        request(colour_op_plugin, infinite, get_colour_pipe_data_atom_v, media_ptr)
            .then(
                [=](ColourOperationDataPtr colour_op_data) mutable {
                    if (colour_op_data) {
                        result->add_operation(colour_op_data);
                        result->set_cache_id(result->cache_id() + colour_op_data->cache_id());
                    }
                    (*rcount)--;
                    if (!(*rcount)) {

                        deliver_on_reponse_promises(result, transform_id);
                    }
                },
                [=](const caf::error &err) mutable {
                    (*rcount) = 0;
                    deliver_on_reponse_promises(err, transform_id);
                });
    }
}

void ColourPipeline::deliver_on_reponse_promises(
    ColourPipelineDataPtr &result, const std::string &transform_id) {
    auto r = in_flight_requests_.find(transform_id);
    if (r != in_flight_requests_.end()) {
        for (auto &rp : r->second) {
            rp.deliver(result);
        }
        in_flight_requests_.erase(r);
    }
}

void ColourPipeline::deliver_on_reponse_promises(
    const caf::error &err, const std::string &transform_id) {
    auto r = in_flight_requests_.find(transform_id);
    if (r != in_flight_requests_.end()) {
        for (auto &rp : r->second) {
            rp.deliver(err);
        }
        in_flight_requests_.erase(r);
    }
}

void ColourPipeline::load_colour_op_plugins() {

    // we have to do async requests, because this function is called from
    // ColourPipelineActor which is spawned byt the plugin_manager_registry...
    // If we did blocking request receive we would have a lock situation as the
    // plugin_manager_registry is busy spawning 'this'.

    auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
    request(
        pm,
        infinite,
        utility::detail_atom_v,
        plugin_manager::PluginType(plugin_manager::PluginFlags::PF_COLOUR_OPERATION))
        .then(
            [=](const std::vector<plugin_manager::PluginDetail>
                    &colour_op_plugin_details) mutable {
                auto count = std::make_shared<int>(colour_op_plugin_details.size());

                if (colour_op_plugin_details.empty()) {
                    colour_ops_loaded_ = true;
                    for (auto &hr : hook_requests_) {
                        make_pre_draw_gpu_hook(hr);
                    }
                    hook_requests_.clear();
                }

                for (const auto &pd : colour_op_plugin_details) {
                    request(
                        pm,
                        infinite,
                        plugin_manager::spawn_plugin_atom_v,
                        pd.uuid_,
                        utility::JsonStore())
                        .then(
                            [=](caf::actor colour_op_actor) mutable {
                                anon_send(colour_op_actor, module::connect_to_ui_atom_v);
                                colour_op_plugins_.push_back(colour_op_actor);

                                // TODO: uncomment this when we've fixed colour grading
                                // singleton issue!! link_to(colour_op_actor);
                                (*count)--;
                                if (!(*count)) {
                                    colour_ops_loaded_ = true;
                                    for (auto &hr : hook_requests_) {
                                        make_pre_draw_gpu_hook(hr);
                                    }
                                    hook_requests_.clear();
                                }
                            },
                            [=](const caf::error &err) mutable {
                                for (auto &hr : hook_requests_) {
                                    hr.deliver(err);
                                }
                                hook_requests_.clear();
                            });
                }
            },
            [=](const caf::error &err) mutable {
                for (auto &hr : hook_requests_) {
                    hr.deliver(err);
                }
                hook_requests_.clear();
            });
}

/* We wrap any GPUPreDrawHooks in a single GPUPreDrawHook - so if multiple
colour ops have GPUPreDrawHooks then they appear as a single one to the
Viewport that executes our wrapper. Just makes life a bit easier than passing
a set of them back to the viewport, I suppose.*/
class HookCollection : public plugin::GPUPreDrawHook {
  public:
    std::vector<plugin::GPUPreDrawHookPtr> hooks_;

    void pre_viewport_draw_gpu_hook(
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        const float viewport_du_dpixel,
        xstudio::media_reader::ImageBufPtr &image) override {
        for (auto &hook : hooks_) {
            hook->pre_viewport_draw_gpu_hook(
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                image);
        }
    }
};


void ColourPipeline::make_pre_draw_gpu_hook(
    caf::typed_response_promise<plugin::GPUPreDrawHookPtr> rp) {

    auto collection = std::make_shared<HookCollection>();

    if (colour_op_plugins_.empty()) {
        rp.deliver(collection);
        return;
    }
    caf::scoped_actor sys(system());

    // loop over the colour_op_plugins, requesting their GPUPreDrawHook class
    // instances, gather them in our 'HookCollection' and when we have all
    // the responses back we deliver.
    auto count = std::make_shared<int>(colour_op_plugins_.size());
    for (auto &colour_op_plugin : colour_op_plugins_) {

        request(colour_op_plugin, infinite, ui::viewport::pre_render_gpu_hook_atom_v)
            .then(
                [=](plugin::GPUPreDrawHookPtr &hook) mutable {
                    if (hook) {
                        // gather
                        collection->hooks_.push_back(hook);
                    }
                    (*count)--;
                    if (!(*count)) {
                        rp.deliver(collection);
                    }
                },
                [=](const caf::error &err) mutable {
                    rp.deliver(err);
                    (*count) = 0;
                });
    }
}