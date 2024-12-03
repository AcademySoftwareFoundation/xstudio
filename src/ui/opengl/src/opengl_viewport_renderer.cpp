// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/ui/opengl/no_image_shader_program.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_colour_lut_texture.hpp"
#include "xstudio/ui/opengl/opengl_rgba8bit_image_texture.hpp"
#include "xstudio/ui/opengl/opengl_ssbo_image_texture.hpp"
#include "xstudio/ui/opengl/opengl_multi_buffered_texture.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/opengl/gl_debug_utils.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::ui::opengl;
using namespace xstudio::ui::viewport;
using namespace xstudio::media_reader;
using namespace xstudio::colour_pipeline;
using namespace xstudio::utility;

namespace {

static const std::string default_vertex_shader = R"(
#version 330 core
vec2 calc_pixel_coordinate(vec2 viewport_coordinate)
{
    return vec2(viewport_coordinate.x+1.0,1.0-viewport_coordinate.y)*0.5;
}
)";

} // namespace

using nlohmann::json;


ColourPipeLutCollection::ColourPipeLutCollection(const ColourPipeLutCollection &o) {
    lut_textures_ = o.lut_textures_;
}

void ColourPipeLutCollection::upload_luts(
    const std::vector<colour_pipeline::ColourLUTPtr> &luts) {

    for (const auto &lut : luts) {

        if (lut_textures_.find(lut->texture_name_and_desc()) == lut_textures_.end()) {
            lut_textures_[lut->texture_name_and_desc()].reset(
                new GLColourLutTexture(lut->descriptor(), lut->texture_name()));
        }
        lut_textures_[lut->texture_name_and_desc()]->upload_texture_data(lut);
        active_luts_.push_back(lut_textures_[lut->texture_name_and_desc()]);
    }
}

void ColourPipeLutCollection::register_texture(
    const std::vector<colour_pipeline::ColourTexture> &textures) {

    for (const auto &tex : textures) {
        active_textures_[tex.name] = tex;
    }
}

void ColourPipeLutCollection::bind_luts(GLShaderProgramPtr shader, int &tex_idx) {
    for (const auto &lut : active_luts_) {
        utility::JsonStore txshder_param(nlohmann::json{{lut->texture_name(), tex_idx}});
        lut->bind(tex_idx);
        shader->set_shader_parameters(txshder_param);
        tex_idx++;
    }
    for (const auto &[name, tex] : active_textures_) {
        glActiveTexture(GL_TEXTURE0 + tex_idx);
        switch (tex.target) {
        case colour_pipeline::ColourTextureTarget::TEXTURE_2D: {
            glBindTexture(GL_TEXTURE_2D, tex.id);
        }
        }

        utility::JsonStore txshder_param(nlohmann::json{{tex.name, tex_idx}});
        shader->set_shader_parameters(txshder_param);

        tex_idx++;
    }
}

std::map<std::string, OpenGLViewportRenderer::SharedResourcesPtr> OpenGLViewportRenderer::s_resources_store_;

OpenGLViewportRenderer::OpenGLViewportRenderer(const std::string &window_id)
    : viewport::ViewportRenderer(), window_id_(window_id)
{
    // Instances of OpenGLViewportRenderer that are in the same xstudio 
    // window need to share texture resources as they display exactly the 
    // same image.
    if (s_resources_store_.contains(window_id)) {
        resources_ = s_resources_store_[window_id];
    } else {
        s_resources_store_[window_id].reset(new SharedResources);
        resources_ = s_resources_store_[window_id];
    }
}

OpenGLViewportRenderer::~OpenGLViewportRenderer() {

    resources_.reset();

    // check if we were the last instance of OpenGLViewportRenderer to be
    // using the entry in s_resources_store_ .. if so, remove it from 
    // s_resources_store_ so it gets destroyed and texture resources etc are
    // released
    auto p = s_resources_store_.find(window_id_);
    if (p != s_resources_store_.end() && p->second.use_count() == 1) {
        s_resources_store_.erase(p);
    }
}


void OpenGLViewportRenderer::upload_image_and_colour_data(
    const media_reader::ImageBufPtr &image) {


    colour_pipeline::ColourPipelineDataPtr colour_pipe_data = image.colour_pipe_data_;

    if (!textures().size())
        return;

    if (image) {
        if (image->error_state() == BufferErrorState::HAS_ERROR) {
            // the frame contains errors, no need to continue from that point
            active_shader_program_ = no_image_shader_program();
            return;
        }

        // check if the frame we need to draw has already been
        // uploaded to texture memory and set the 'draw_texture_index_'
        // accordingly
        // textures()[0]->upload_image(image);
    }

    if (colour_pipe_data && colour_pipe_data->cache_id() != latest_colour_pipe_data_cacheid_) {
        colour_pipe_textures().clear();
        for (const auto &op : colour_pipe_data->operations()) {
            colour_pipe_textures().upload_luts(op->luts_);
            colour_pipe_textures().register_texture(op->textures_);
        }
        latest_colour_pipe_data_cacheid_ = colour_pipe_data->cache_id();
    }

    if (image && colour_pipe_data &&
        activate_shader(image->shader(), colour_pipe_data->operations())) {

        active_shader_program_->set_shader_parameters(image);
        active_shader_program_->set_shader_parameters(image.colour_pipe_uniforms_);

    } else {
        active_shader_program_ = no_image_shader_program();
    }

    bind_textures(image);
}

void OpenGLViewportRenderer::bind_textures(const media_reader::ImageBufPtr &image) {

    if (!active_shader_program_ || active_shader_program_ == no_image_shader_program())
        return;

    int tex_idx = 1;
    Imath::V2i tex_dims;
    textures()[0]->bind(image, tex_idx, tex_dims);
    utility::JsonStore txshder_param;
    txshder_param["the_tex"]  = tex_idx;
    txshder_param["tex_dims"] = tex_dims;

    tex_idx++;

    active_shader_program_->set_shader_parameters(txshder_param);
    colour_pipe_textures().bind_luts(active_shader_program_, tex_idx);

    // return active texture to default
    glActiveTexture(GL_TEXTURE0);
}

void OpenGLViewportRenderer::release_textures() {

    if (active_shader_program_ == no_image_shader_program())
        return;

    // textures()[0]->release();
}

// static std::mutex m;

void OpenGLViewportRenderer::clear_viewport_area(const Imath::M44f &window_to_viewport_matrix) {

    // The GL viewport does not necessarily match the area of the xstudio
    // viewport ... for example, the xstudio viewport is typically embedded in
    // a QML interface with various other widgets arranged around it. At render
    // time, we take account of this using the 'window_to_viewport_matrix' which maps from
    // the current GL_VIEWPORT to the area of the xstudio viewport within it.
    // In this case, we are using this to run glClear but only on the area of
    // the viewport. This might save a tiny bit of time when rendering the whole
    // interface, but also means we don't stomp on something that has been drawn
    // into the GL canvas that is supposed to be there!
    std::array<int, 4> vp;
    glGetIntegerv(GL_VIEWPORT, vp.data());

    // Our ref coord system maps -1.0, -1.0 to the bottom left of the viewport and
    // 1.0, 1.0 to the top right
    Imath::V4f botomleft(-1.0, -1.0, 0.0f, 1.0f);
    Imath::V4f topright(1.0, 1.0, 0.0f, 1.0f);

    topright  = topright * window_to_viewport_matrix;
    botomleft = botomleft * window_to_viewport_matrix;

    // Now convert to window pixels for glScissor window
    botomleft *= 1.0f / botomleft.w;
    topright *= 1.0f / topright.w;

    float left  = 0.5f * (botomleft.x + 1.0f) * float(vp[2]);
    float bottom = 0.5f * (botomleft.y + 1.0f) * float(vp[3]);
    float top    = 0.5f * (topright.y + 1.0f) * float(vp[3]);
    float right = 0.5f * (topright.x + 1.0f) * float(vp[2]);

    scissor_coords_[0] = (int)round(left);
    scissor_coords_[1] = (int)round(bottom);
    scissor_coords_[2] = (int)round(right - left);
    scissor_coords_[3] = (int)round(top - bottom);

    glEnable(GL_SCISSOR_TEST);
    glScissor(
        scissor_coords_[0],
        scissor_coords_[1],
        scissor_coords_[2],
        scissor_coords_[3]);
    glClearColor(0.0f, 0.0f, 0.0f, use_alpha_ ? 1.0f : 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

}

utility::JsonStore OpenGLViewportRenderer::default_prefs() {
    JsonStore r(R"("texture_mode": {
				"path": "/ui/viewport/texture_mode",
				"default_value": "Image Texture",
				"description": "Viewport Low Level Texture mode - SSBO can give better performance on some systems but not all",
				"value": "Image Texture",
				"value_range": ["Image Texture", "SSBO"]
				"datatype": "string",
				"context": ["APPLICATION"]
			})");
    return r;
}

void OpenGLViewportRenderer::set_prefs(const utility::JsonStore &prefs) {
    const std::string tex_mode = prefs.value("texture_mode", "SSBO");
    if (tex_mode == "SSBO")
        use_ssbo_ = true;
    else
        use_ssbo_ = false;
}

void OpenGLViewportRenderer::render(
    const media_reader::ImageBufDisplaySetPtr &images,
    const Imath::M44f &window_to_viewport_matrix,
    const Imath::M44f &viewport_to_image_space,
    const std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> &overlay_renderers) {

    init();

    auto t0 = utility::clock::now();

    // this value tells us how much we are zoomed into the image in the viewport (in
    // the x dimension). If the image is width-fitted exactly to the viewport, then this
    // value will be 1.0 (what it means is the coordinates -1.0 to 1.0 are mapped to
    // the width of the viewport)
    const float image_zoom_in_viewport = viewport_to_image_space[0][0];


    // this value gives us how much of the parent window is covered by the viewport.
    // So if the xstudio window is 1000px in width, and the viewport is 500px wide
    // (with the rest of the UI taking up the remainder) then this value will be 0.5
    const float viewport_x_size_in_window = window_to_viewport_matrix[0][0] / window_to_viewport_matrix[3][3];

    // the gl viewport corresponds to the parent window size.
    std::array<int, 4> gl_viewport;
    glGetIntegerv(GL_VIEWPORT, gl_viewport.data());
    const auto viewport_width = (float)gl_viewport[2];

    // this value tells us how much a screen pixel width in the viewport is in the units
    // of viewport coordinate space
    const float viewport_du_dx =
        image_zoom_in_viewport / (viewport_width * viewport_x_size_in_window);

    /* we do our own clear of the viewport */
    clear_viewport_area(window_to_viewport_matrix);

    glUseProgram(0);

    if (images && images->layout_data()) {

        glDisable(GL_DEPTH_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);

        textures()[0]->queue_image_set_for_upload(images);

        for (const auto &idx: images->layout_data()->image_draw_order_hint_) {
            __draw_image(images, idx, window_to_viewport_matrix,  viewport_to_image_space, viewport_du_dx, overlay_renderers);
        }

    }

#ifdef DEBUG_GRAB_FRAMEBUFFER
    grab_framebuffer_to_disk();
#endif


    // The use of this is questionable - I'm not sure how it plays with the QML
    // rendering engine. The idea is that we are guaranteeing the image has
    // been rendered at this stage so we are sure it's on screen for the next
    // refresh. In testing, it seems to help smooth playback under PCOIP when
    // doing demanding rendering - e.g. high frame rate, high res sources with
    // bilinear filtering.
    // glFinish();
    /*std::cerr << "DRAW completed uploaded in " <<
    std::chrono::duration_cast<std::chrono::microseconds>(utility::clock::now()-t0).count()
    <<
    "\n";*/

}

void OpenGLViewportRenderer::__draw_image(
    const media_reader::ImageBufDisplaySetPtr &images,
    const int index,
    const Imath::M44f &window_to_viewport_matrix,
    const Imath::M44f &viewport_to_image_space,
    const float viewport_du_dx,
    const std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> &overlay_renderers) {

    media_reader::ImageBufPtr image_to_be_drawn;
    if (!images || images->empty()) {
        active_shader_program_ = no_image_shader_program();
    } else {
        image_to_be_drawn = images->onscreen_image(index);
    }

    Imath::M44f to_image_matrix = (image_to_be_drawn.layout_transform() * viewport_to_image_space.inverse()).inverse();

    /* Here we allow plugins to run arbitrary GPU draw & computation routines.
    This will allow pixel data to be rendered to textures (offscreen), for example,
    which can then be sampled at actual draw time.*/
    if (image_to_be_drawn) {

        for (auto hook : pre_render_gpu_hooks_) {
            hook.second->pre_viewport_draw_gpu_hook(
                window_to_viewport_matrix,
                to_image_matrix,
                viewport_du_dx,
                image_to_be_drawn);
        }

        // if we've received a new image and/or colour pipeline data (LUTs etc) since the last
        // draw, upload the data
        upload_image_and_colour_data(image_to_be_drawn);
    }

    // we must avoid painting outside the geometry of the viewport window, or it will be
    // visible underneath QML elements with opactity etc. or other viewports in the
    // same window
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        scissor_coords_[0],
        scissor_coords_[1],
        scissor_coords_[2],
        scissor_coords_[3]);

    /* Call the render functions of overlay plugins - for the BeforeImage pass, we only call
    this if we have an alpha buffer that allows us to 'under' the image with the overlay
    drawings. */
    if (image_to_be_drawn && use_alpha_) {
        for (auto orf : overlay_renderers) {
            if (orf.second->preferred_render_pass() ==
                plugin::ViewportOverlayRenderer::BeforeImage) {
                orf.second->render_opengl(
                    window_to_viewport_matrix,
                    to_image_matrix,
                    abs(viewport_du_dx),
                    image_to_be_drawn,
                    use_alpha_);
            }
        }
    }
    if (image_to_be_drawn) {
        // scissor test on main draw
        draw_image(image_to_be_drawn, images->layout_data(), index, window_to_viewport_matrix, viewport_to_image_space, viewport_du_dx);
    }

    /* Call the render functions of overlay plugins - note that if the overlay prefers to draw
    before the image but we have no alpha channel, we still call its render function here */
    if (image_to_be_drawn) {
        textures()[0]->release(image_to_be_drawn);
        for (auto orf : overlay_renderers) {
            if (orf.second->preferred_render_pass() ==
                    plugin::ViewportOverlayRenderer::AfterImage ||
                !use_alpha_) {
                orf.second->render_opengl(
                    window_to_viewport_matrix,
                    to_image_matrix,
                    abs(viewport_du_dx),
                    image_to_be_drawn,
                    use_alpha_);
            }
        }
    }
    glDisable(GL_SCISSOR_TEST);

}

void OpenGLViewportRenderer::draw_image(
    const media_reader::ImageBufPtr &image_to_be_drawn,
    const media_reader::ImageSetLayoutDataPtr &layout_data,
    const int index,
    const Imath::M44f &window_to_viewport_matrix,
    const Imath::M44f &viewport_to_image_space,
    const float viewport_du_dx) {


    active_shader_program_->use();

    // set-up core shader parameters (e.g. image transform matrix etc)
    utility::JsonStore shader_params = core_shader_params(
        image_to_be_drawn,
        window_to_viewport_matrix,
        viewport_to_image_space,
        viewport_du_dx,
        layout_data->custom_layout_data_,
        index
        );

    active_shader_program_->set_shader_parameters(shader_params);


    if (use_alpha_) {

        // this set-up allows the image to be drawn 'under' overlays that are 
        // drawn first onto the black canvas - (e.g. annotations that can
        // be drawn better this way)
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
    }

    // the actual draw .. a quad that spans -1.0, 1.0 in x & y.
    glBindVertexArray(vao());
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);

    glUseProgram(0);

    if (use_alpha_) {
        // this set-up allows the image to be drawn 'under' overlays that are 
        // drawn first onto the black canvas - (e.g. annotations that can
        // be drawn better this way)
        glDisable(GL_BLEND);
    }

}

bool OpenGLViewportRenderer::activate_shader(
    const viewport::GPUShaderPtr &virt_image_buffer_unpack_shader,
    const std::vector<colour_pipeline::ColourOperationDataPtr> &colour_operations) {

    if (!virt_image_buffer_unpack_shader ||
        virt_image_buffer_unpack_shader->graphics_api() != GraphicsAPI::OpenGL) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, "No shader passed with image buffer.");
        return false;
    }

    auto image_buffer_unpack_shader =
        static_cast<opengl::OpenGLShader const *>(virt_image_buffer_unpack_shader.get());
    if (!image_buffer_unpack_shader) {
    }

    std::string shader_id = to_string(image_buffer_unpack_shader->shader_id());

    for (const auto &op : colour_operations) {
        shader_id += op->cache_id();
    }

    // do we already have this shader compiled?
    if (shader_programs().find(shader_id) == shader_programs().end()) {

        // try to compile the shader for this combo of image buffer unpack
        // and colour pipeline components

        try {

            std::vector<std::string> shader_components;
            for (const auto &colour_op : colour_operations) {
                // sanity check - this should be impossible, though
                if (colour_op->shader_->graphics_api() != GraphicsAPI::OpenGL) {
                    throw std::runtime_error(
                        "Non-OpenGL shader data in colour operation chain!");
                }
                auto pr = static_cast<opengl::OpenGLShader const *>(colour_op->shader_.get());
                shader_components.push_back(pr->shader_code());
            }

            shader_programs()[shader_id].reset(new GLShaderProgram(
                default_vertex_shader,
                image_buffer_unpack_shader->shader_code(),
                shader_components,
                use_ssbo_));

        } catch (std::exception &e) {
            spdlog::error("{}", e.what());
            shader_programs()[shader_id].reset();
        }
    }

    if (shader_programs()[shader_id]) {
        active_shader_program_ = shader_programs()[shader_id];
    } else {
        active_shader_program_ = no_image_shader_program();
    }

    return active_shader_program_ != no_image_shader_program();
}

void OpenGLViewportRenderer::pre_init() {

    // we need to know if we have alpha in our draw buffer, which might require
    // different strategies for drawing overlays
    int alpha_bits;
    glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
    // TODO: not using alpha buffer at all right now, making offscreen rendering
    // of annotations go wrong.
    use_alpha_ = false;// alpha_bits != 0;
    resources_->init();

}

OpenGLViewportRenderer::SharedResources::~SharedResources() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}


void OpenGLViewportRenderer::SharedResources::init() {

    if (textures_.size()) return;

    glewInit();

// #define OPENGL_DEBUG_CB
#ifdef OPENGL_DEBUG_CB
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debug_message_callback, nullptr);
    // For simplicity we filter messages inside the callback
    // Alternative would be using glDebugMessageControl
#endif

    textures_.emplace_back(GLDoubleBufferedTexture::create<GLBlindRGBA8bitTex>());

    // Set up the geometry used at draw time ... it couldn't be more simple,
    // it's just two triangles to make a rectangle
    glGenBuffers(1, &vbo_);
    glGenVertexArrays(1, &vao_);

    static std::array<float, 24> vertices = {
        // 1st triangle
        -1.0f, 1.0f, 0.0f, 1.0f, // top left
        1.0f, 1.0f, 0.0f, 1.0f, // top right
        1.0f, -1.0f, 0.0f, 1.0f, // bottom right
        // 2nd triangle
        1.0f, -1.0f, 0.0f, 1.0f, // bottom right
        -1.0f, 1.0f, 0.0f, 1.0f, // top left
        -1.0f, -1.0f, 0.0f, 1.0f // bottom left
        };

    glBindVertexArray(vao_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
    // 3. then set our vertex module pointers
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //glDeleteBuffers(1, &vbo);

    // add shader for no image render
    no_image_shader_program_ =
        GLShaderProgramPtr(static_cast<GLShaderProgram *>(new NoImageShaderProgram()));
}

