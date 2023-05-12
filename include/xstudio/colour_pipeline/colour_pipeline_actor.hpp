// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/policy/select_all.hpp>

#include "colour_pipeline.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/pixel_info.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace colour_pipeline {

    // TODO: This should be pulled from the preferences
    const int colour_pipeline_worker_count{4};

    template <typename T>
    class ColourPipelinePlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        ColourPipelinePlugin(
            utility::Uuid uuid,
            std::string name        = "",
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginType::PT_COLOUR_MANAGEMENT,
                  false,
                  author,
                  description,
                  version) {}
        ~ColourPipelinePlugin() override = default;
    };

    class GlobalColourPipelineActor : public caf::event_based_actor, public module::Module {
      public:
        GlobalColourPipelineActor(caf::actor_config &cfg);
        ~GlobalColourPipelineActor() override = default;

        caf::behavior make_behavior() override;

        const char *name() const override { return NAME.c_str(); }

        void on_exit() override;

      private:
        void load_colour_pipe_details();

        void make_or_get_colour_pipeline(
            const std::string &pipe_name,
            const utility::JsonStore &jsn,
            caf::typed_response_promise<caf::actor> &rp);

        caf::actor
        make_colour_pipeline(const std::string &pipe_name, const utility::JsonStore &jsn);

      private:
        inline static const std::string NAME                = "GlobalColourPipelineActor";
        inline static const std::string BUILTIN_PLUGIN_NAME = "OCIOColourPipeline";

        std::vector<plugin_manager::PluginDetail> colour_pipe_plugin_details_;
        std::map<std::string, caf::actor> colour_pipeline_actors_;
        caf::actor active_in_ui_colour_pipeline_;
        std::string default_plugin_name_;
        utility::JsonStore prefs_jsn_;
    };

    template <typename T> class ColourPipelineWorkerActor : public caf::event_based_actor {
      public:
        ColourPipelineWorkerActor(
            caf::actor_config &cfg, const utility::JsonStore &jsn, T *cpipe, caf::actor cache)
            : caf::event_based_actor(cfg), colour_pipeline_(cpipe), cache_(std::move(cache)) {
            utility::print_on_create(this, "ColourPipelineWorkerActor");
            utility::print_on_exit(this, "ColourPipelineWorkerActor");
        }
        ~ColourPipelineWorkerActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        caf::behavior make_behavior() override {
            return colour_pipeline_->message_handler().or_else(caf::message_handler{
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {
                    // nop
                },
                [=](json_store::update_atom,
                    const utility::JsonStore & /*change*/,
                    const std::string & /*path*/,
                    const utility::JsonStore &full) {
                    delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
                },
                [=](json_store::update_atom, const utility::JsonStore & /*prefs*/) mutable {
                    // nop
                },
                [=](get_colour_pipe_data_atom,
                    const media::AVFrameID &media_ptr) -> caf::result<ColourPipelineDataPtr> {
                    auto rp = make_response_promise<ColourPipelineDataPtr>();
                    get_cached_colour_pipeline_data(rp, media_ptr);
                    return rp;
                },
                [=](display_colour_transform_hash_atom,
                    const media::AVFrameID &media_ptr) -> std::string {
                    return colour_pipeline_->fast_display_transform_hash(media_ptr);
                },
                [=](media_reader::process_thumbnail_atom,
                    const media::AVFrameID &mptr,
                    const thumbnail::ThumbnailBufferPtr &buf)
                    -> result<thumbnail::ThumbnailBufferPtr> {
                    try {
                        return colour_pipeline_->process_thumbnail(mptr, buf);
                    } catch (const std::exception &err) {
                        return make_error(sec::runtime_error, err.what());
                    }
                },
                [=](pixel_info_atom atom,
                    const media_reader::PixelInfo &pixel_info,
                    const media::AVFrameID &mptr) -> media_reader::PixelInfo {
                    auto p = pixel_info;
                    colour_pipeline_->extend_pixel_info(p, mptr);
                    return p;
                }});
        }

      private:
        // Pass ``media_ptr`` by value to make sure it's available in the lambda.
        void get_cached_colour_pipeline_data(
            caf::typed_response_promise<ColourPipelineDataPtr> rp,
            const media::AVFrameID media_ptr) {
            try {
                const std::string key =
                    colour_pipeline_->compute_hash(media_ptr.source_uuid_, media_ptr.params_);

                if (bool(cache_)) {
                    request(cache_, infinite, media_cache::retrieve_atom_v, key)
                        .then(
                            [=](ColourPipelineDataPtr buf) mutable {
                                if (buf) {
                                    colour_pipeline_->update_shader_uniforms(
                                        buf, media_ptr.source_uuid_);
                                    rp.deliver(buf);
                                } else {
                                    get_colour_pipeline_data(rp, media_ptr);
                                }
                            },
                            [=](const caf::error &err) mutable {
                                spdlog::warn(
                                    "Failed cache retrieve colour_pipeline {} {}",
                                    key,
                                    to_string(err));
                                rp.deliver(err);
                            });
                } else {
                    get_colour_pipeline_data(rp, media_ptr);
                }
            } catch (std::exception &e) {
                rp.deliver(make_error(xstudio_error::error, e.what()));
            }
        }

        void get_colour_pipeline_data(
            caf::typed_response_promise<ColourPipelineDataPtr> rp,
            const media::AVFrameID &media_ptr) {
            ColourPipelineDataPtr data = colour_pipeline_->make_empty_data();

            colour_pipeline_->setup_shader(*data, media_ptr.source_uuid_, media_ptr.params_);
            if (cache_) {
                anon_send<message_priority::high>(
                    cache_, media_cache::store_atom_v, data->cache_id_, data);
            }
            colour_pipeline_->update_shader_uniforms(data, media_ptr.source_uuid_);

            rp.deliver(data);
        }

      private:
        inline static const std::string NAME = "ColourPipelineWorkerActor";

        T *colour_pipeline_;
        caf::actor cache_;
    };

    template <typename T> class ColourPipelineActor : public caf::event_based_actor {
      public:
        ColourPipelineActor(caf::actor_config &cfg, const utility::JsonStore &init_settings)
            : caf::event_based_actor(cfg),
              uuid_(utility::Uuid::generate()),
              colour_pipeline_(init_settings) {
            cache_ = system().registry().template get<caf::actor>(colour_cache_registry);

            event_group_ = spawn<broadcast::BroadcastActor>(this);
            link_to(event_group_);

            // Handle colour pipeline module messages
            colour_pipeline_.set_parent_actor_addr(caf::actor_cast<actor_addr>(this));

            // Workers will share the colour pipeline and used for processing heavy tasks
            // Spawn event-based actors that will be scheduled on CAF workers
            pool_ = caf::actor_pool::make(
                system().dummy_execution_unit(),
                colour_pipeline_worker_count,
                [&] {
                    return system().template spawn<ColourPipelineWorkerActor<T>>(
                        init_settings, &colour_pipeline_, cache_);
                },
                caf::actor_pool::round_robin());
            link_to(pool_);

            thumbnail_processor_pool_ = caf::actor_pool::make(
                system().dummy_execution_unit(),
                colour_pipeline_worker_count,
                [&] {
                    return system().template spawn<ColourPipelineWorkerActor<T>>(
                        init_settings, &colour_pipeline_, cache_);
                },
                caf::actor_pool::round_robin());
            link_to(thumbnail_processor_pool_);
        }

        ~ColourPipelineActor() override = default;

        caf::behavior make_behavior() override;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ColourPipelineActor";

        caf::behavior behavior_;
        caf::actor cache_;
        caf::actor event_group_;
        caf::actor pool_;
        caf::actor thumbnail_processor_pool_;
        utility::Uuid uuid_;
        T colour_pipeline_;
    };

    template <class T> caf::behavior ColourPipelineActor<T>::make_behavior() {
        return colour_pipeline_.message_handler().or_else(caf::message_handler{
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {
                // nop
            },
            [=](const caf::error &reason) {
                spdlog::debug("ColourPipelineActor exited: {}", to_string(reason));
            },
            [=](get_colour_pipe_data_atom atom, const media::AVFrameID &media_ptr) {
                delegate(pool_, atom, media_ptr);
            },
            [=](get_colour_pipe_data_atom atom,
                const media::AVFrameID &media_ptr,
                bool use_thumbnail_pool) {
                delegate(thumbnail_processor_pool_, atom, media_ptr);
            },
            [=](display_colour_transform_hash_atom atom, const media::AVFrameID &media_ptr) {
                delegate(thumbnail_processor_pool_, atom, media_ptr);
            },
            [=](pixel_info_atom atom,
                const media_reader::PixelInfo &pixel_info,
                const media::AVFrameID &mptr) { delegate(pool_, atom, pixel_info, mptr); },
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
            [=](utility::event_atom,
                playhead::media_source_atom,
                caf::actor media_actor,
                const utility::Uuid &media_uuid) {
                // This message comes from the playhead when the onscreen (key)
                // media source has changed.
                if (media_actor and media_uuid) {
                    request<message_priority::high>(
                        media_actor, infinite, get_colour_pipe_params_atom_v)
                        .then(
                            [=](const utility::JsonStore &colour_params) mutable {
                                colour_pipeline_.media_source_changed(
                                    media_uuid, colour_params);
                            },
                            [=](const caf::error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                }
            },
            [=](xstudio::ui::viewport::screen_info_atom,
                const bool &is_primary_viewer,
                const std::string &name,
                const std::string &model,
                const std::string &manufacturer,
                const std::string &serialNumber) {
                colour_pipeline_.screen_changed(
                    is_primary_viewer, name, model, manufacturer, serialNumber);
            },
            [=](media_reader::process_thumbnail_atom,
                const media::AVFrameID &mptr,
                const thumbnail::ThumbnailBufferPtr &buf) {
                delegate(
                    thumbnail_processor_pool_,
                    media_reader::process_thumbnail_atom_v,
                    mptr,
                    buf);
            },
            [=](json_store::update_atom,
                const utility::JsonStore & /*change*/,
                const std::string & /*path*/,
                const utility::JsonStore &full) {
                delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
            },
            [=](json_store::update_atom, const utility::JsonStore &) mutable {
                /*try {
                    size_t count = 6;//preference_value<size_t>(j,
                "/core/colour_pipeline/max_worker_count"); if (count > worker_count) {
                        spdlog::debug(
                            "colour_pipeline workers changed old {} new {}", worker_count,
                count); while (worker_count < count) { anon_send( pool_, sys_atom_v,
                                put_atom_v,
                                system().spawn<ColourPipelineWorkerActor>());
                            worker_count++;
                        }
                    } else if (count < worker_count) {
                        // get actors..
                        spdlog::debug(
                            "colour_pipeline workers changed old {} new {}", worker_count,
                count); worker_count = count; request(pool_, infinite, sys_atom_v,
                get_atom_v) .await(
                                [=](const std::vector<actor> &ws) {
                                    for (auto i = worker_count; i < ws.size(); i++) {
                                        anon_send(pool_, sys_atom_v, delete_atom_v, ws[i]);
                                    }
                                },
                                [=](const error &err) {
                                    throw std::runtime_error(
                                        "Failed to find pool " + to_string(err));
                                });
                    }
                } catch (...) {
                }*/
            },
            [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },
            [=](utility::serialise_atom) -> utility::JsonStore {
                return colour_pipeline_.serialise();
            }});
    }

} // namespace colour_pipeline
} // namespace xstudio
