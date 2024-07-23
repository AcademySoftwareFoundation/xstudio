// SPDX-License-Identifier: Apache-2.0
#include <sstream>

#include "xstudio/colour_pipeline/colour_operation.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"

using namespace xstudio::colour_pipeline;

caf::message_handler ColourOpPlugin::message_handler_extensions() {
    return caf::message_handler(

        [=](colour_operation_uniforms_atom,
            const media_reader::ImageBufPtr &image) -> result<utility::JsonStore> {
            try {

                return update_shader_uniforms(image);

            } catch (std::exception &e) {

                return caf::make_error(xstudio_error::error, e.what());
            }
        },

        [=](get_colour_pipe_data_atom,
            const media::AVFrameID &media_ptr) -> result<ColourOperationDataPtr> {
            auto rp = make_response_promise<ColourOperationDataPtr>();


            // use the AVFrameID to get to the MediaSourceActor
            auto media_source = utility::UuidActor(
                media_ptr.source_uuid_, caf::actor_cast<caf::actor>(media_ptr.actor_addr_));

            // now get to the MediaActor that owns the MediaSourceActor
            request(media_source.actor(), infinite, utility::parent_atom_v)
                .then(
                    [=](caf::actor media_actor) mutable {
                        auto media = utility::UuidActor(media_ptr.media_uuid_, media_actor);

                        ColourOperationDataPtr result =
                            colour_op_graphics_data(media_source, media_ptr.params_);

                        rp.deliver(result);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },
        [=](get_colour_pipe_data_atom,
            const media::AVFrameIDsAndTimePoints &mptr_and_timepoints)
            -> result<std::vector<ColourOperationDataPtr>> {
            // Somewhat ugly - we have to build an ordered vector of results
            // by making multiple (async) requests. Remember, there is no
            // quarantee that the response to a requests will come back in
            // order, so we use the
            auto result = std::make_shared<std::vector<ColourOperationDataPtr>>();
            result->resize(mptr_and_timepoints.size());
            auto rp    = make_response_promise<std::vector<ColourOperationDataPtr>>();
            auto self  = caf::actor_cast<caf::actor>(this);
            auto count = std::make_shared<int>(mptr_and_timepoints.size());
            for (size_t i = 0; i < mptr_and_timepoints.size(); ++i) {
                request(
                    self,
                    infinite,
                    get_colour_pipe_data_atom_v,
                    *(mptr_and_timepoints[i].second))
                    .then(
                        [=](const ColourOperationDataPtr &d) mutable {
                            (*result)[i] = d;
                            (*count)--;
                            if (!(*count)) {
                                rp.deliver(*result);
                            }
                        },
                        [=](caf::error &err) mutable {
                            (*count) = 0;
                            rp.deliver(err);
                        });
            }
            return rp;
        },
        [=](utility::event_atom,
            playhead::media_source_atom,
            caf::actor media_actor,
            const utility::Uuid &media_uuid,
            const utility::JsonStore &source_colour_params) {
            on_screen_source_ = utility::UuidActor(media_uuid, media_actor);
            onscreen_media_source_changed(on_screen_source_, source_colour_params);
        }
        /*,
        [=](media::media_source_atom, utility::UuidActor media_source, const utility::JsonStore
        & colour_params) { media_source_changed(media_source, colour_params);
        }*/
    );
}

void ColourOpPlugin::onscreen_source_colour_metadata_merge(
    const utility::JsonStore &additional_colour_params) {
    if (on_screen_source_.actor()) {

        request(
            on_screen_source_.actor(), infinite, colour_pipeline::get_colour_pipe_params_atom_v)
            .then(
                [=](utility::JsonStore params) {
                    params.merge(additional_colour_params);
                    request(
                        on_screen_source_.actor(),
                        infinite,
                        colour_pipeline::set_colour_pipe_params_atom_v,
                        params)
                        .then(
                            [=](bool) {

                            },
                            [=](caf::error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                },
                [=](caf::error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}
