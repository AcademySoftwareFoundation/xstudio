// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

#include "grading.h"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"
#include "grading_colour_op.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;
using namespace xstudio::ui::viewport;


GradingMaskRenderer::GradingMaskRenderer() {

    canvas_renderer_.reset(new ui::opengl::OpenGLCanvasRenderer());
}

void GradingMaskRenderer::pre_viewport_draw_gpu_hook(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    xstudio::media_reader::ImageBufPtr &image) {


    // the data on any grading mask layers and brushstrokes can come from two
    // sources.
    // 1) The data can be attached to the bookmarks that are accessed from
    // ImageBufPtr::bookmarks() - this is grading data that has been saved to
    // a bookmark as annotation data.
    //
    // 2) The data can be attached to the image as part of the 'plugin_blind_data'.
    // We request plugin data using our plugin UUID and then dynamic cast to
    // get to our 'GradingMaskRenderData' object. (Note that this was set in
    // GradingTool::prepare_overlay_data).
    //
    // If we have data via the blind data, this is immediate updated grading data
    // when the user is interacting with the render by painting a mask. The data
    // for the *same* grading note can also come up from the bookmark system
    // but it will be slightly out-of-date vs. the blind data that is updated
    // on every mouse event when the user is painting the mask. So we use the
    // blind data version in favour of the bookmark version when we have both.

    utility::BlindDataObjectPtr blind_data =
        image.plugin_blind_data(utility::Uuid(colour_pipeline::GradingTool::PLUGIN_UUID));

    if (blind_data) {
        const GradingMaskRenderData *render_data =
            dynamic_cast<const GradingMaskRenderData *>(blind_data.get());
        if (render_data) {
            renderGradingDataMasks(&(render_data->interaction_grading_data_), image);
            // we exit here as we don't support multiple grading ops on a given
            // media source
            return;
        }
    }

    for (auto &bookmark : image.bookmarks()) {

        const GradingData *data =
            dynamic_cast<const GradingData *>(bookmark->annotation_.get());
        if (data) {
            renderGradingDataMasks(data, image);
            break; // we're only supporting a single grading op on a given source
        }
    }
}

void GradingMaskRenderer::renderGradingDataMasks(
    const GradingData *data, xstudio::media_reader::ImageBufPtr &image) {

    // First grab the ColourOperationData for this plugin which has already
    // been added to the image. This data is the static data for the colour
    // operation - namely the shader code itself that implements the grading
    // op and also any colour LUT textures needed for the operation. It does
    // not (yet) include dynamic texture data such as the mask that the user
    // can paint the grade through.
    colour_pipeline::ColourOperationDataPtr colour_op_data =
        image.colour_pipe_data_ ? image.colour_pipe_data_->get_operation_data(
                                      colour_pipeline::GradingColourOperator::PLUGIN_UUID)
                                : colour_pipeline::ColourOperationDataPtr();

    if (!colour_op_data)
        return;

    // Because we are going to modify the member data of colour_op_data we need
    // to make ourselves a 'deep' copy since this is shared data and it could
    // be simultaneously accessed in other places in the application.
    colour_op_data = std::make_shared<colour_pipeline::ColourOperationData>(*colour_op_data);

    while (data->size() > layer_count()) {
        add_layer();
    }

    // Paint the canvas for each grading data / layer
    size_t layer_index = 0;
    std::string cache_id_modifier;
    for (auto &layer_data : *data) {

        // here the mask is rendered to a GL texture
        render_layer(layer_data, render_layers_[layer_index], image, true);

        // here we add info on the texture to the colour_op_data since
        // the colour op needs to use the texture
        colour_op_data->textures_.emplace_back(colour_pipeline::ColourTexture{
            fmt::format("layer{}_mask", layer_index),
            colour_pipeline::ColourTextureTarget::TEXTURE_2D,
            render_layers_[layer_index].offscreen_renderer->texture_handle()});

        // adding info on the mask texture layers to the cache id will
        // force the viewport to assign new active texture indices to
        // the layer mask texture, if the number of layers has changed
        cache_id_modifier += std::to_string(layer_index);

        layer_index++;
    }

    // Again, colour_pipe_data_ is a shared ptr and we don't know who else might
    // be holding/using this data so we make a deep copy. (This is not
    // expensive as the contents of ColourPipelineData is only a vector
    // to shared ptrs itself, we're not modifying elements of that vector though)
    image.colour_pipe_data_.reset(
        new colour_pipeline::ColourPipelineData(*(image.colour_pipe_data_)));

    // here the relevant shared ptr to the colour op data is reset
    image.colour_pipe_data_->overwrite_operation_data(colour_op_data);

    image.colour_pipe_data_->cache_id_ += cache_id_modifier;
}

void GradingMaskRenderer::add_layer() {

    // using 8 bit texture - should be more efficient than float32
    RenderLayer rl;
    rl.offscreen_renderer = std::make_unique<OpenGLOffscreenRenderer>(GL_RGBA8);
    render_layers_.push_back(std::move(rl));
}

size_t GradingMaskRenderer::layer_count() const { return render_layers_.size(); }

void GradingMaskRenderer::render_layer(
    const LayerData &data,
    RenderLayer &layer,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    if (data.mask().uuid() != layer.last_canvas_uuid ||
        data.mask().last_change_time() != layer.last_canvas_change_time) {

        // instead of varying the canvas size to match the image size, we can
        // use a fixed canvas size. The grade mask doesn't need to be
        // high res as the strokes have some softness. Low res will perform
        // better and have less footprint.
        layer.offscreen_renderer->resize(
            Imath::V2i(960, 540)); // frame->image_size_in_pixels());
        layer.offscreen_renderer->begin();

        if (data.mask().size()) {
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClearDepth(0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Imath::M44f to_canvas;

            // see comment above
            /*const float image_aspect_ratio =
                frame->pixel_aspect() *
                (1.0f * 960 / 540);
                    const float image_aspect_ratio =
                frame->pixel_aspect() *
                (1.0f * frame->image_size_in_pixels().x / frame->image_size_in_pixels().y);
            to_canvas.setScale(Imath::V3f(1.0f, image_aspect_ratio, 1.0f));*/

            to_canvas.setScale(Imath::V3f(1.0f, 16.0f / 9.0f, 1.0f));

            canvas_renderer_->render_canvas(
                data.mask(),
                HandleState(),
                // We are drawing to an offscreen texture and don't need
                // any view / projection matrix to account for the viewport
                // transformation. However, we still need to account for the
                // image aspect ratio.
                to_canvas,
                Imath::M44f(),
                2.0 / 960, // 2.0f / frame->image_size_in_pixels().x (see note A)
                have_alpha_buffer);
        } else {
            // blank (empty) maske with no stokes. In this case we flood
            // texture with 1.0s
            glClearColor(1.0, 1.0, 1.0, 1.0);
            glClearDepth(0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // note A1: This value is the 'viewport_du_dpixel; which  tells us how
        // many units of xstudio's viewport coordinate space are covered by a
        // pixel. This is used to scale the 'softness' of strokes. In our case
        // the number of pixels is the set to the canvas size - this is width
        // fitted to the span of -1.0 to 1.0 in xSTUDIO's coordinate system.

        layer.offscreen_renderer->end();
        layer.last_canvas_change_time = data.mask().last_change_time();
        layer.last_canvas_uuid        = data.mask().uuid();
    }
}