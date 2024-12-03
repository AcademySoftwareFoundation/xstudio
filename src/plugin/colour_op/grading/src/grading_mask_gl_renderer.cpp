// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

#include "grading.h"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"
#include "grading_colour_op.hpp"
#include "grading_common.h"

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
    // GradingTool::onscreen_render_data).
    //
    // If we have data via the blind data, this is immediate updated grading data
    // when the user is interacting with the render by painting a mask. The data
    // for the *same* grading note can also come up from the bookmark system
    // but it will be slightly out-of-date vs. the blind data that is updated
    // on every mouse event when the user is painting the mask. So we use the
    // blind data version in favour of the bookmark version when we have both.

    auto active_grades = get_active_grades(image);
    if (active_grades.size()) {
        render_grading_data_masks(active_grades, image);
    }
}

void GradingMaskRenderer::add_layer() {

    // using 8 bit texture - should be more efficient than float32
    RenderLayer rl;
    rl.offscreen_renderer = std::make_unique<OpenGLOffscreenRenderer>(GL_RGBA8);
    render_layers_.push_back(std::move(rl));
}

size_t GradingMaskRenderer::layer_count() const { return render_layers_.size(); }

void GradingMaskRenderer::render_grading_data_masks(
    const std::vector<GradingInfo> &grades, xstudio::media_reader::ImageBufPtr &image) {

    // First grab the ColourOperationData for this plugin which has already
    // been added to the image. This data is the static data for the colour
    // operation - namely the shader code itself that implements the grading
    // op and also any colour LUT textures needed for the operation. It does
    // not (yet) include dynamic texture data such as the mask that the user
    // can paint the grade through.
    colour_pipeline::ColourOperationDataPtr colour_op_data;
    if (image.colour_pipe_data_) {
        colour_op_data = image.colour_pipe_data_->get_operation_data(
            colour_pipeline::GradingColourOperator::PLUGIN_UUID);
    } else {
        return;
    }

    // Because we are going to modify the member data of colour_op_data we need
    // to make ourselves a 'deep' copy since this is shared data and it could
    // be simultaneously accessed in other places in the application.
    colour_op_data = std::make_shared<colour_pipeline::ColourOperationData>(*colour_op_data);

    // Paint the canvas associated with each grade (if any)
    std::string cache_id_modifier;

    size_t grade_index        = 0;
    size_t masked_grade_index = 0;
    for (const auto grade_info : grades) {

        auto grade = grade_info.data;

        // Ignore grade without mask
        if (grade->mask().empty()) {
            grade_index++;
            continue;
        }

        if (masked_grade_index >= layer_count()) {
            // TODO: ColSci
            // We add layer here but never reclaim / free them later.
            add_layer();
        }

        // here the mask is rendered to a GL texture
        render_layer(*grade, render_layers_[masked_grade_index], image, true);

        // here we add info on the texture to the colour_op_data since
        // the colour op needs to use the texture
        colour_op_data->textures_.emplace_back(colour_pipeline::ColourTexture{
            fmt::format("masks[{}]", grade_index),
            colour_pipeline::ColourTextureTarget::TEXTURE_2D,
            render_layers_[masked_grade_index].offscreen_renderer->texture_handle()});

        // adding info on the mask texture layers to the cache id will
        // force the viewport to assign new active texture indices to
        // the layer mask texture, if the number of layers has changed
        cache_id_modifier += std::to_string(grade_index);

        masked_grade_index++;
        grade_index++;
    }

    // Again, colour_pipe_data_ is a shared ptr and we don't know who else might
    // be holding/using this data so we make a deep copy. (This is not
    // expensive as the contents of ColourPipelineData is only a vector
    // to shared ptrs itself, we're not modifying elements of that vector though)
    image.colour_pipe_data_.reset(
        new colour_pipeline::ColourPipelineData(*(image.colour_pipe_data_)));

    // here the relevant shared ptr to the colour op data is reset
    image.colour_pipe_data_->overwrite_operation_data(colour_op_data);

    image.colour_pipe_data_->set_cache_id(image.colour_pipe_data_->cache_id() + cache_id_modifier);
}

void GradingMaskRenderer::render_layer(
    const GradingData &data,
    RenderLayer &layer,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    // Use fixed resolution for better performance
    const Imath::V2i mask_resolution(960, 540);
    const float image_aspect_ratio =
        (1.0f * frame->image_size_in_pixels().x / frame->image_size_in_pixels().y);
    const float canvas_aspect_ratio = 1.0f * mask_resolution.x / mask_resolution.y;

    if (data.mask().uuid() != layer.last_canvas_uuid ||
        data.mask().last_change_time() != layer.last_canvas_change_time ||
        image_aspect_ratio != layer.last_image_aspect_ratio) {

        layer.offscreen_renderer->resize(mask_resolution);
        layer.offscreen_renderer->begin();

        if (!data.mask().empty()) {

            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClearDepth(0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Mask drawing don't depend on viewport transformation, however we still
            // need to account for the mask texture aspect ratio.
            Imath::M44f to_canvas;
            to_canvas.setScale(Imath::V3f(1.0f, canvas_aspect_ratio, 1.0f));

            canvas_renderer_->render_canvas(
                data.mask(),
                HandleState(),
                to_canvas,
                Imath::M44f(),
                2.0f / mask_resolution.x, // See A1
                have_alpha_buffer,
                image_aspect_ratio);
        } else {

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
        layer.last_image_aspect_ratio = image_aspect_ratio;
    }
}