// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "annotation_opengl_renderer.hpp"
#include "annotation_render_data.hpp"
#include "annotations_tool.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;


AnnotationsRenderer::AnnotationsRenderer(const xstudio::ui::canvas::Canvas &interaction_canvas)
    : interaction_canvas_(interaction_canvas) {

    canvas_renderer_.reset(new ui::opengl::OpenGLCanvasRenderer());
}

void AnnotationsRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    std::unique_lock l(mutex_);
    if (!annotations_visible_)
        return;
    // the xstudio playhead takes care of attaching bookmark data to the
    // ImageBufPtr that we receive here. The bookmark data may have annotations
    // data attached which we can draw to screen....
    bool draw_interaction_canvas = false;

    for (const auto &anno : frame.bookmarks()) {

        // .. we don't draw the annotation attached to the frame if its bookmark
        // uuid is the same as the uuid of the annotation that is currently
        // being edited. The reason is that the strokes and captions of this
        // annotation are already cloned into 'interaction_canvas_' which we
        // draw below.
        if (anno->detail_.uuid_ == current_edited_bookmark_uuid_) {
            draw_interaction_canvas = true;
            continue;
        }

        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(anno->annotation_.get());
        if (my_annotation) {
            canvas_renderer_->render_canvas(
                my_annotation->canvas(),
                HandleState(),
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                have_alpha_buffer,
                1.f);
        }
    }

    if (draw_interaction_canvas || (current_edited_bookmark_uuid_.is_null() &&
                                    (frame_being_annotated_ == frame.frame_id().key()))) {
        canvas_renderer_->render_canvas(
            interaction_canvas_,
            handle_,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            have_alpha_buffer,
            1.f);
    }
}

void AnnotationsRenderer::render_viewport_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_normalised_coords,
    const float viewport_du_dpixel,
    const bool have_alpha_buffer) {
    std::unique_lock l(mutex_);
    if (laser_drawing_mode_) {
        canvas_renderer_->render_canvas(
            interaction_canvas_,
            handle_,
            transform_window_to_viewport_space,
            transform_viewport_to_normalised_coords,
            viewport_du_dpixel,
            have_alpha_buffer,
            1.f);
    }
}

void AnnotationsRenderer::update(
    const bool show_annotations,
    const xstudio::ui::canvas::HandleState &h,
    const utility::Uuid &current_edited_bookmark_uuid,
    const media::MediaKey &frame_being_annotated,
    bool laser_mode) {
    std::unique_lock l(mutex_);
    handle_                       = h;
    current_edited_bookmark_uuid_ = current_edited_bookmark_uuid;
    laser_drawing_mode_           = laser_mode;
    frame_being_annotated_        = frame_being_annotated;
    annotations_visible_          = show_annotations;
}
