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


AnnotationsRenderer::AnnotationsRenderer() {

    canvas_renderer_.reset(new ui::opengl::OpenGLCanvasRenderer());
}

void AnnotationsRenderer::render_opengl(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    utility::BlindDataObjectPtr render_data =
        frame.plugin_blind_data2(AnnotationsTool::PLUGIN_UUID);
    const auto *data = dynamic_cast<const AnnotationRenderDataSet *>(render_data.get());

    if (!data) {
        // annotation tool hasn't attached any render data to this image.
        // This means annotations aren't visible.
        return;
    }

    // if the uuid for the interaction canvas is null we always draw it because
    // it is 'lazer' pen strokes
    bool draw_interaction_canvas = data && data->current_edited_bookmark_uuid_.is_null();

    // the xstudio playhead takes care of attaching bookmark data to the
    // ImageBufPtr that we receive here. The bookmark data may have annotations
    // data attached which we can draw to screen....
    for (const auto &anno : frame.bookmarks()) {

        // .. we don't draw the annotation attached to the frame if its bookmark
        // uuid is the same as the uuid of the annotation that is currently
        // being edited. The reason is that the strokes and captions of this
        // annotation are already cloned into 'interaction_canvas_' which we
        // draw below.
        if (anno->detail_.uuid_ == data->current_edited_bookmark_uuid_) {
            draw_interaction_canvas = true;
            continue;
        }

        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(anno->annotation_.get());
        if (my_annotation) {
            canvas_renderer_->render_canvas(
                my_annotation->canvas(),
                data->handle_,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                have_alpha_buffer);
        }
    }

    // xSTUDIO supports multiple viewports, each can show different media or
    // different frames of the same media. If the user is drawing annotations in
    // one viewport we may (or may not) want to draw those strokes in realtime
    // in other viewports:
    //
    // When user is currently drawing, we store the 'key' for the frame they are
    // drawing on. Current drawings are stored in data->interaction_canvas_ ...
    // we only want to plot these if the frame we are rendering over here matches
    // the key of the frame the user is being drawn on.
    if (draw_interaction_canvas &&
        data->interaction_frame_key_ == to_string(frame.frame_id().key_)) {
        canvas_renderer_->render_canvas(
            data->interaction_canvas_,
            data->handle_,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            have_alpha_buffer);
    }
}