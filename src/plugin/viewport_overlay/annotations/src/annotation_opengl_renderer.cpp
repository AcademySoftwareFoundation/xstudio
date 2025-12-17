// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "annotation_opengl_renderer.hpp"
#include "annotation_render_data.hpp"
#include "annotations_core_plugin.hpp"
#include "annotations_ui_plugin.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/helpers.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;

namespace {
// shaders for the pixel patch (colour dropper) tool
const char *vertex_shader = R"(
    #version 410 core
    layout (location = 1) in vec4 pos;
    layout (location = 0) in vec4 color;

    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform vec2 v_pos;
    uniform float viewport_du_dpixel;
    out vec4 t_color;
    out vec2 nrd;

    void main()
    {
        nrd = pos.xy;
        float l = length(nrd);
        float F = 5;
        float lens_scale = 2*l - l*l/F;
        nrd = normalize(nrd)*lens_scale;
        vec4 rpos = vec4((nrd.x+4.0)*50.0*viewport_du_dpixel, -(nrd.y+4.0)*50.0*viewport_du_dpixel, 0.0, 1.0);
        rpos.x += v_pos.x;
        rpos.y += v_pos.y;
        gl_Position = rpos*to_coord_system*to_canvas;
        t_color = color;
    }
    )";

const char *frag_shader = R"(
    #version 410 core
    out vec4 FragColor;
    uniform vec3 line_colour;
    in vec4 t_color;
    in vec2 nrd;

    void main(void)
    {
        float c = length(nrd);
        FragColor = t_color*smoothstep(4.1,4.0,c);
    }

    )";

// shader for caption box and handles
const char *caption_box_vertex_shader = R"(
    #version 410 core
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform vec2 box_position;
    uniform vec2 box_size;
    uniform vec2 aa_nudge;
    uniform float du_dx;
    layout (location = 0) in vec2 aPos;
    //layout (location = 1) in vec2 bPos;
    out vec2 screen_pixel;

    void main()
    {

        // now we 'emit' one of four vertices to make a quad. We do it by adding
        // or subtracting the tangent to the line segment , depending of the
        // vertex index in the quad
        vec2 vertex_pos = aPos.xy;
        vertex_pos.x = vertex_pos.x*box_size.x;
        vertex_pos.y = vertex_pos.y*box_size.y;
        vertex_pos += box_position + aa_nudge*du_dx;
        screen_pixel = vertex_pos/du_dx;
        gl_Position = vec4(vertex_pos,0.0,1.0)*to_coord_system*to_canvas;
    }
)";

const char *caption_box_frag_shader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform bool shadow;
    uniform int box_type;
    uniform float opacity;
    in vec2 screen_pixel;
    void main(void)
    {
        ivec2 offset_screen_pixel = ivec2(screen_pixel) + ivec2(5000,5000); // move away from origin
        if (box_type==1) {
            // draws a dotted line
            if (((offset_screen_pixel.x/20) & 1) == ((offset_screen_pixel.y/20) & 1)) {
                FragColor = vec4(0.0f, 0.0f, 0.0f, opacity);
            } else {
                FragColor = vec4(1.0f, 1.0f, 1.0f, opacity);
            }
        } else if (box_type==2) {
            FragColor = vec4(0.0f, 0.0f, 0.0f, opacity);
        } else if (box_type==3) {
            FragColor = vec4(0.7f, 0.7f, 0.7f, opacity);
        } else {
            FragColor = vec4(1.0f, 1.0f, 1.0f, opacity);
        }
    }
)";

// for drawing the 3 handles the show up on the live caption (text box)
static std::array<Imath::V2f, 38> handles_vertices = {
    // unit box for drawing boxes!
    Imath::V2f(0.0f, 0.0f),
    Imath::V2f(1.0f, 0.0f),
    Imath::V2f(1.0f, 1.0f),
    Imath::V2f(0.0f, 1.0f),

    // double headed arrow, vertical
    Imath::V2f(0.5f, 0.0f),
    Imath::V2f(0.5f, 1.0f),

    Imath::V2f(0.5f, 0.0f),
    Imath::V2f(0.5f - 0.2f, 0.2f),

    Imath::V2f(0.5f, 0.0f),
    Imath::V2f(0.5f + 0.2f, 0.2f),

    Imath::V2f(0.5f, 1.0f),
    Imath::V2f(0.5f - 0.2f, 1.0f - 0.2f),

    Imath::V2f(0.5f, 1.0f),
    Imath::V2f(0.5f + 0.2f, 1.0f - 0.2f),

    // double headed arrow, horizontal
    Imath::V2f(0.0f, 0.5f),
    Imath::V2f(1.0f, 0.5f),

    Imath::V2f(0.0f, 0.5f),
    Imath::V2f(0.2f, 0.5f - 0.2f),

    Imath::V2f(0.0f, 0.5f),
    Imath::V2f(0.2f, 0.5f + 0.2f),

    Imath::V2f(1.0f, 0.5f),
    Imath::V2f(1.0f - 0.2f, 0.5f - 0.2f),

    Imath::V2f(1.0f, 0.5f),
    Imath::V2f(1.0f - 0.2f, 0.5f + 0.2f),

    // crossed lines
    Imath::V2f(0.2f, 0.2f),
    Imath::V2f(0.8f, 0.8f),
    Imath::V2f(0.8f, 0.2f),
    Imath::V2f(0.2f, 0.8f),

};

} // namespace

AnnotationsRenderer::AnnotationsRenderer(
    const std::string &viewport_name,
    std::atomic_bool &cursor_blink,
    std::atomic_bool &hide_all,
    std::atomic_bool *hide_strokes)
    : viewport_name_(viewport_name), cursor_blink_(cursor_blink), hide_all_(hide_all), hide_strokes_(hide_strokes) {
    canvas_renderer_.reset(new ui::opengl::OpenGLCanvasRenderer());
    texthandle_renderer_.reset(new CaptionHandleRenderer());
}

void AnnotationsRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame) {

    if (hide_all_) return;

    // 'live' annotation edit data (strokes & shapes under construction) is
    // attached to the frame as plugin blind data. We get to it here (if it
    // is available)
    const PerImageAnnotationRenderDataSet * live_canvas_data = nullptr;
    auto render_data = frame.plugin_blind_data(AnnotationsCore::PLUGIN_UUID);
    if (render_data) {
        live_canvas_data = dynamic_cast<const PerImageAnnotationRenderDataSet *>(render_data.get());
    }

    // render annotated bookmarks .. loop on bookmars attached to the frame
    for (const auto &bookmark : frame.bookmarks()) {

        // get to annotation data by dynamic casing the annotation_ pointer on
        // the bookmark to our 'Annotation' class.
        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(bookmark->annotation_.get());
        if (my_annotation && live_canvas_data) {
            // 'live' erase strokes must be injected so that they are applied
            // correctly to the existing bookmark.
            canvas_renderer_->render_canvas(
                my_annotation->canvas(),
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio,
                1.f,
                *hide_strokes_,
                live_canvas_data->live_erase_strokes(bookmark->detail_.uuid_),
                live_canvas_data->skip_captions()
            );
        } else if (my_annotation) {
            canvas_renderer_->render_canvas(
                my_annotation->canvas(),
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio,
                1.f,
                *hide_strokes_);
        }
    }

    if (live_canvas_data) {

        // now we draw 'live' stroke data for the given image
        if (!live_canvas_data->strokes().empty())
            canvas_renderer_->render_strokes(
                live_canvas_data->strokes(),
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio);

        const auto &captions = live_canvas_data->captions();
        auto caption_handle_states = live_canvas_data->handle_states().begin();

        for (const auto &caption: captions) {

            canvas_renderer_->render_single_caption(
                *caption,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio);            

            const auto cursor = caption->cursor_position_on_image();

            texthandle_renderer_->render_caption_handle(
                *caption_handle_states,
                caption->bounding_box(),
                true,
                cursor.data(),
                cursor_blink_,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio);

            caption_handle_states++;

        }

        const auto &hovered_caption_bdbs = live_canvas_data->hovered_caption_boxes();
        for (const auto &bdb: hovered_caption_bdbs) {
            texthandle_renderer_->render_caption_handle(
                HandleHoverState::HoveredInCaptionArea,
                bdb,
                false,
                nullptr,
                false,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio);
        }
    }

    //restore_opengl_status();

    //glClear(GL_DEPTH_BUFFER_BIT);
}

void AnnotationsRenderer::render_viewport_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_normalised_coords,
    const media_reader::ImageBufDisplaySetPtr &on_screen_frames,
    const float viewport_du_dpixel,
    const float device_pixel_ratio)
{

    if (hide_all_) return;

    if (on_screen_frames) {

        auto overlays_data = on_screen_frames->plugin_blind_data<const LaserStrokesRenderDataSet>(AnnotationsCore::PLUGIN_UUID);
        if (overlays_data) {

            canvas_renderer_->render_strokes(
                overlays_data->laser_strokes(),
                transform_window_to_viewport_space,
                transform_viewport_to_normalised_coords,
                viewport_du_dpixel,
                device_pixel_ratio);

        }
    }

}

void AnnotationsExtrasRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame) 
{

    auto overlays_data = frame.plugin_blind_data<const AnnotationExtrasRenderDataSet>(AnnotationsUI::PLUGIN_UUID);
    if (overlays_data) {
    
    }

}

void AnnotationsExtrasRenderer::render_viewport_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_normalised_coords,
    const media_reader::ImageBufDisplaySetPtr &on_screen_frames,
    const float viewport_du_dpixel,
    const float device_pixel_ratio)
{

    if (pixel_patch_.skip_render(viewport_name_))
        return;

    std::lock_guard l(pixel_patch_.mutex());

    if (!shader_)
        init_overlay_opengl();

    glBindVertexArray(vao_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        pixel_patch_.patch_vertex_data().size() * sizeof(Imath::V4f),
        pixel_patch_.patch_vertex_data().data(),
        GL_STREAM_DRAW);
    // 3. then set our vertex module pointers
    Imath::V4f *ptr = 0;
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(Imath::V4f), (void *)ptr);
    ptr += 1;
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(Imath::V4f), (void *)ptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    utility::JsonStore shader_params;
    shader_params["to_canvas"]          = transform_window_to_viewport_space;
    shader_params["to_coord_system"]    = transform_viewport_to_normalised_coords.inverse();
    shader_params["v_pos"]              = pixel_patch_.position();
    shader_params["viewport_du_dpixel"] = viewport_du_dpixel;
    shader_->set_shader_parameters(shader_params);
    shader_->use();
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glDrawArrays(GL_TRIANGLES, 0, (pixel_patch_.patch_vertex_data().size()) / 2 - 4);

    // inside square highlight
    if (pixel_patch_.pressed()) {
        glLineWidth(3.0f);
    } else {
        glLineWidth(1.0f);
    }
    glDrawArrays(GL_LINE_LOOP, (pixel_patch_.patch_vertex_data().size()) / 2 - 4, 4);

    // outside square highlight
    // glDrawArrays(GL_LINE_LOOP, (pixel_patch_.patch_vertex_data().size()) / 2 - 4, 4);

    shader_->stop_using();
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);

}

void AnnotationsExtrasRenderer::init_overlay_opengl()
{
    glGenBuffers(1, &vbo_);
    glGenVertexArrays(1, &vao_);
    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
}


static struct AAJitterTable {

    struct {
        Imath::V2f operator()(int N, int i, int j) {
            auto x = -0.5f + (i + 0.5f) / N;
            auto y = -0.5f + (j + 0.5f) / N;
            return {x, y};
        }
    } gridLookup;

    AAJitterTable() {
        aa_nudge.resize(16);
        int lookup[16] = {11, 6, 10, 8, 9, 12, 7, 1, 3, 13, 5, 4, 2, 15, 0, 14};
        int ct         = 0;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                aa_nudge[lookup[ct]]["aa_nudge"] = gridLookup(4, i, j);
                ct++;
            }
        }
    }

    std::vector<xstudio::utility::JsonStore> aa_nudge;

} aa_jitter_table;


CaptionHandleRenderer::~CaptionHandleRenderer() { cleanup_gl(); }

void CaptionHandleRenderer::init_gl() {

    if (!shader_) {
        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(caption_box_vertex_shader, caption_box_frag_shader);
    }

    if (!handles_vertex_buffer_obj_ && !handles_vertex_array_) {
        glGenBuffers(1, &handles_vertex_buffer_obj_);
        glGenVertexArrays(1, &handles_vertex_array_);

        glBindVertexArray(handles_vertex_array_);

        glBindBuffer(GL_ARRAY_BUFFER, handles_vertex_buffer_obj_);
        glBufferData(
            GL_ARRAY_BUFFER, sizeof(handles_vertices), handles_vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Imath::V2f), nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);
    }
}

void CaptionHandleRenderer::cleanup_gl() {

    if (handles_vertex_array_) {
        glDeleteVertexArrays(1, &handles_vertex_array_);
        handles_vertex_array_ = 0;
    }

    if (handles_vertex_buffer_obj_) {
        glDeleteBuffers(1, &handles_vertex_buffer_obj_);
        handles_vertex_buffer_obj_ = 0;
    }
}

void CaptionHandleRenderer::render_caption_handle(
    const HandleHoverState &handle_state,
    const Imath::Box2f &caption_box,
    const bool is_live_caption,
    const Imath::V2f *cursor,
    const bool cursor_blink_state,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dx,
    const float device_pixel_ratio) {

    if (!shader_)
        init_gl();

    utility::JsonStore shader_params;

    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["du_dx"]           = viewport_du_dx;
    shader_params["box_type"]        = 0;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    shader_->use();
    shader_->set_shader_parameters(shader_params);

    utility::JsonStore shader_params2;

    // draw the box around the current edited caption
    shader_params2["box_position"] = caption_box.min;
    shader_params2["box_size"]     = caption_box.size();
    shader_params2["opacity"]      = 0.6;
    shader_params2["box_type"]     = 1;
    shader_params2["aa_nudge"]     = Imath::V2f(0.0f, 0.0f);

    shader_->set_shader_parameters(shader_params2);
    glBindVertexArray(handles_vertex_array_);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    if (is_live_caption) {

        const auto handle_size = Imath::V2f(
            CAPTION_HANDLE_SIZE * viewport_du_dx * device_pixel_ratio,
            CAPTION_HANDLE_SIZE * viewport_du_dx * device_pixel_ratio);

        // Draw the three
        static const auto hndls = std::vector<HandleHoverState>(
            {HandleHoverState::HoveredOnMoveHandle,
             HandleHoverState::HoveredOnResizeHandle,
             HandleHoverState::HoveredOnDeleteHandle});

        static const auto vtx_offsets = std::vector<int>({4, 14, 24});
        static const auto vtx_counts  = std::vector<int>({20, 10, 4});

        const auto positions = std::vector<Imath::V2f>(
            {caption_box.min - handle_size,
             caption_box.max,
             {caption_box.max.x,
              caption_box.min.y - handle_size.y}});

        shader_params2["box_size"] = handle_size;

        glBindVertexArray(handles_vertex_array_);

        // draw a grey box for each handle
        shader_params2["opacity"] = 0.6f;
        for (size_t i = 0; i < hndls.size(); ++i) {
            shader_params2["box_position"] = positions[i];
            shader_params2["box_type"]     = 2;
            shader_->set_shader_parameters(shader_params2);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
        }

        static const auto aa_jitter = std::vector<Imath::V2f>(
            {{-0.33f, -0.33f},
             {-0.0f, -0.33f},
             {0.33f, -0.33f},
             {-0.33f, 0.0f},
             {0.0f, 0.0f},
             {0.33f, 0.0f},
             {-0.33f, 0.33f},
             {0.0f, 0.33f},
             {0.33f, 0.33f}});


        shader_params2["box_size"] = handle_size * 0.8f;
        // draw the lines for each handle
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        shader_params2["opacity"] = 1.0f / 16.0f;
        for (size_t i = 0; i < hndls.size(); ++i) {

            shader_params2["box_position"] = positions[i] + 0.1f * handle_size;
            shader_params2["box_type"]     = handle_state == hndls[i] ? 4 : 3;

            shader_->set_shader_parameters(shader_params2);
            // plot 9 times with anti-aliasing jitter to get a better looking result
            for (const auto &aa_nudge : aa_jitter_table.aa_nudge) {
                shader_->set_shader_parameters(aa_nudge);
                glDrawArrays(GL_LINES, vtx_offsets[i], vtx_counts[i]);
            }
        }

    } else {

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (!caption_box.isEmpty()) {

            shader_params2["box_position"] = caption_box.min;
            shader_params2["box_size"]     = caption_box.size();
            shader_params2["opacity"]      = 0.3;
            shader_params2["box_type"]     = 1;

            shader_->set_shader_parameters(shader_params2);

            glBindVertexArray(handles_vertex_array_);

            glLineWidth(2.0f);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
        }

    }

    if (cursor) {//handle_state.cursor_position[0] != Imath::V2f(0.0f, 0.0f)) {

        shader_params2["opacity"]      = 0.6f;
        shader_params2["box_position"] = cursor[0];
        shader_params2["box_size"] =
            cursor[1] - cursor[0];
        shader_params2["box_type"] = cursor_blink_state ? 2 : 0;
        shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(handles_vertex_array_);
        glLineWidth(3.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    glBindVertexArray(0);
}
