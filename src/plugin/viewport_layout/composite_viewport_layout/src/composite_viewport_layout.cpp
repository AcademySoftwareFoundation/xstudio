// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "composite_viewport_layout.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/opengl/opengl_offscreen_renderer.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_multi_buffered_texture.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio;

namespace {
const char *vertex_shader = R"(
    #version 330 core
    layout (location = 0) in vec4 aPos;
    uniform mat4 to_canvas;
    uniform mat4 canvas_to_image;
    out vec2 canvasCoordinate;

    void main()
    {
        vec4 rpos = aPos;
        gl_Position = (rpos*to_canvas);
        vec4 ipos = (aPos*canvas_to_image);
        canvasCoordinate = (rpos.xy + vec2(1.0, 1.0))*0.5f;
    }
    )";

const char *frag_shader = R"(
    #version 410 core
    out vec4 FragColor;
    uniform sampler2D textureSamplerA;
    uniform sampler2D textureSamplerB;
    in vec2 canvasCoordinate;
    uniform float boost;
    uniform bool screen;
    uniform bool monochrome;

    void main(void)
    {
        vec4 a = texture(textureSamplerA, canvasCoordinate);
        vec4 b = texture(textureSamplerB, canvasCoordinate);
        if (screen) {

            vec4 m = max(a, b);
            vec4 screen = vec4(1.0) - (vec4(1.0) - a)*(vec4(1.0) - b);
            FragColor = vec4(
                m.x > 1.0 ? m.x : screen.x,
                m.y > 1.0 ? m.y : screen.y,
                m.z > 1.0 ? m.z : screen.z,
                m.w > 1.0 ? m.w : screen.w
                );

        } else if (monochrome) {
            float al = length(a.rgb);
            float bl = length(b.xyz);
            float scale = pow(2.0, boost);
            float d = (al - bl)*scale;
            FragColor = vec4(0.5) + vec4(vec3(d), (a.a-b.a)*scale);
        } else {
            float scale = pow(2.0, boost);
            vec4 d = (a - b)*scale;
            FragColor = vec4(0.5, 0.5, 0.5, 0.5) + d;        
        }
    }

    )";
} // namespace

using namespace xstudio::ui::opengl;

class OpenGLViewportCompositeRenderer : public OpenGLViewportRenderer {

  public:
    OpenGLViewportCompositeRenderer(
        const std::string &window_id, const utility::JsonStore &prefs)
        : OpenGLViewportRenderer(window_id, prefs) {}

    virtual ~OpenGLViewportCompositeRenderer() = default;

    void pre_init() override {
        OpenGLViewportRenderer::pre_init();
        offscreen_texture_target_A_ = std::make_unique<OpenGLOffscreenRenderer>(GL_RGBA32F);
        offscreen_texture_target_B_ = std::make_unique<OpenGLOffscreenRenderer>(GL_RGBA32F);
        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
    }

    void draw_image(
        const media_reader::ImageBufPtr &image,
        const media_reader::ImageSetLayoutDataPtr &layout_data,
        const int index,
        const Imath::M44f &window_to_viewport_matrix,
        const Imath::M44f &viewport_to_image_space,
        const float viewport_du_dx) override;

    void render_difference(
        const media_reader::ImageBufPtr &image_to_be_drawn,
        const bool first_im,
        const Imath::M44f &window_to_viewport_matrix,
        const Imath::M44f &viewport_to_image_space,
        const float viewport_du_dx,
        const utility::JsonStore &params);

    void render_screen(
        const media_reader::ImageBufPtr &image_to_be_drawn,
        const bool first_im,
        const Imath::M44f &window_to_viewport_matrix,
        const Imath::M44f &viewport_to_image_space,
        const float viewport_du_dx);

    OpenGLOffscreenRendererPtr offscreen_texture_target_A_;
    OpenGLOffscreenRendererPtr offscreen_texture_target_B_;
    std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
};

void OpenGLViewportCompositeRenderer::draw_image(
    const media_reader::ImageBufPtr &image_to_be_drawn,
    const media_reader::ImageSetLayoutDataPtr &layout_data,
    const int index,
    const Imath::M44f &window_to_viewport_matrix,
    const Imath::M44f &viewport_to_image_space,
    const float viewport_du_dx) {

    active_shader_program_->use();

    int mode         = layout_data->custom_layout_data_.value("mode", 0);
    int first_im_idx = layout_data->custom_layout_data_.value("first_im", 0);

    if (mode == 3 || mode == 4) {

        render_difference(
            image_to_be_drawn,
            index == first_im_idx,
            window_to_viewport_matrix,
            viewport_to_image_space,
            viewport_du_dx,
            layout_data->custom_layout_data_);
        return;
    }
    // set-up core shader parameters (e.g. image transform matrix etc)
    utility::JsonStore shader_params = core_shader_params(
        image_to_be_drawn,
        window_to_viewport_matrix,
        viewport_to_image_space,
        viewport_du_dx,
        layout_data->custom_layout_data_,
        index);

    if (mode == 0) {
        float blend_ratio = layout_data->custom_layout_data_.value("blend_ratio", 0.5f);
        if (index == first_im_idx) {
            // we don't blend the first image, just draw it
            glDisable(GL_BLEND);
        } else {
            glBlendColor(blend_ratio, blend_ratio, blend_ratio, blend_ratio);
            glEnable(GL_BLEND);
            glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR);
            glBlendEquation(GL_FUNC_ADD);
        }
    } else {

        if (index == first_im_idx) {
            // we don't pre-mult the first image, just draw it
            glDisable(GL_BLEND);
        } else {
            shader_params["use_alpha"] = true;
            glEnable(GL_BLEND);
            if (mode == 1) {
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
            } else {
                // 'Add' mode
                glBlendFunc(GL_ONE, GL_ONE);
            }
            glBlendEquation(GL_FUNC_ADD);
        }
    }

    active_shader_program_->set_shader_parameters(shader_params);

    // the actual draw .. a quad that spans -1.0, 1.0 in x & y.
    glBindVertexArray(vao());
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);

    glUseProgram(0);

    // this set-up allows the image to be drawn 'under' overlays that are
    // drawn first onto the black canvas - (e.g. annotations that can
    // be drawn better this way)
    glDisable(GL_BLEND);
}

void OpenGLViewportCompositeRenderer::render_difference(
    const media_reader::ImageBufPtr &image_to_be_drawn,
    const bool first_im,
    const Imath::M44f &window_to_viewport_matrix,
    const Imath::M44f &viewport_to_image_space,
    const float viewport_du_dx,
    const utility::JsonStore &mode_params) {
    {
        auto offscreen_texture_target =
            first_im ? offscreen_texture_target_A_.get() : offscreen_texture_target_B_.get();

        // STEP 1 - render the viewport (for given image) to a texture
        offscreen_texture_target->resize(
            Imath::V2f(viewport_coords_in_window()[2], viewport_coords_in_window()[3]));
        offscreen_texture_target->begin();

        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        utility::JsonStore j;
        utility::JsonStore shader_params = core_shader_params(
            image_to_be_drawn, Imath::M44f(), viewport_to_image_space, viewport_du_dx, j, 0);

        active_shader_program_->set_shader_parameters(shader_params);

        // the actual draw .. a quad that spans -1.0, 1.0 in x & y.
        glBindVertexArray(vao());
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);

        glUseProgram(0);
        offscreen_texture_target->end();
    }

    // if this is first image, return as we only continue to render the difference
    // if we've got the 2nd image rendered to a texture
    if (first_im)
        return;


    // STEP 2 - render the difference image
    utility::JsonStore params;
    params["to_canvas"]       = window_to_viewport_matrix;
    params["canvas_to_image"] = viewport_to_image_space;
    params["textureSamplerA"] = 10;
    params["textureSamplerB"] = 11;
    params["monochrome"]      = mode_params.value("monochrome", true);
    params["boost"]           = mode_params.value("boost", 0.0f);
    params["screen"]          = mode_params.value("screen", false);

    // set the active tex IDs for our texture targets
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, offscreen_texture_target_A_->texture_handle());
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, offscreen_texture_target_B_->texture_handle());

    shader_->use();
    shader_->set_shader_parameters(params);

    glEnable(GL_SCISSOR_TEST);

    glBindVertexArray(vao());
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);

    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

CompositeViewportLayout::CompositeViewportLayout(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ViewportLayoutPlugin(cfg, init_settings) {

    blend_ratio_ = add_float_attribute("Blend Ratio", "Blend Ratio", 0.5f, 0.0f, 1.0f, 0.005f);
    difference_boost_ =
        add_float_attribute("Difference Boost", "Diff Boost", 0.0f, -5.0f, 5.0f, 0.005f);
    monochrome_ = add_boolean_attribute("Monochrome", "Monochrome", true);

    add_layout_mode(
        "Over", 1.0, playhead::AssemblyMode::AM_TEN, playhead::AutoAlignMode::AAM_ALIGN_FRAMES);

    add_layout_mode(
        "A/B Blend",
        2.0,
        playhead::AssemblyMode::AM_TEN,
        playhead::AutoAlignMode::AAM_ALIGN_FRAMES);

    add_layout_mode(
        "Add", 3.0, playhead::AssemblyMode::AM_TEN, playhead::AutoAlignMode::AAM_ALIGN_FRAMES);

    add_layout_mode(
        "A/B Difference",
        4.0,
        playhead::AssemblyMode::AM_TEN,
        playhead::AutoAlignMode::AAM_ALIGN_FRAMES);

    add_layout_mode(
        "Screen",
        5.0,
        playhead::AssemblyMode::AM_TEN,
        playhead::AutoAlignMode::AAM_ALIGN_FRAMES);

    add_layout_settings_attribute(blend_ratio_, "A/B Blend");
    add_layout_settings_attribute(difference_boost_, "A/B Difference");
    add_layout_settings_attribute(monochrome_, "A/B Difference");

    blend_ratio_->set_preference_path("/plugin/composite_vp_modes/blend_ratio");
    difference_boost_->set_preference_path("/plugin/composite_vp_modes/difference_boost");
    monochrome_->set_preference_path("/plugin/composite_vp_modes/difference_monochrome");
}

ViewportRenderer *CompositeViewportLayout::make_renderer(
    const std::string &window_id, const utility::JsonStore &prefs) {
    return new OpenGLViewportCompositeRenderer(window_id, prefs);
}

void CompositeViewportLayout::do_layout(
    const std::string &layout_mode,
    const media_reader::ImageBufDisplaySetPtr &image_set,
    media_reader::ImageSetLayoutData &layout_data) {
    const int num_images = image_set->num_onscreen_images();
    if (!num_images) {
        return;
    }

    layout_data.draw_hero_overlays_only_ = true;

    if (num_images >= 2 && (layout_mode == "A/B Blend" || layout_mode == "A/B Difference" ||
                            layout_mode == "Screen")) {

        // if 'hero' image is at index 0, we wipe between image 0 and image 1
        // otherwise we wipe between hero image and image 0.
        int wipeA                                   = image_set->hero_sub_playhead_index();
        layout_data.custom_layout_data_["first_im"] = wipeA;

        layout_data.image_draw_order_hint_.push_back(wipeA);
        if (num_images > 1) {
            int wipeB = image_set->previous_hero_sub_playhead_index() != -1
                            ? image_set->previous_hero_sub_playhead_index()
                        : wipeA ? 0
                                : 1;
            layout_data.image_draw_order_hint_.push_back(wipeB);
        }

        // identity matrices here, no transform for A/B wipes
        layout_data.image_transforms_.resize(image_set->num_onscreen_images());

        if (layout_mode == "A/B Blend") {
            layout_data.custom_layout_data_["blend_ratio"] = blend_ratio_->value();
            layout_data.custom_layout_data_["mode"]        = 0;
        } else if (layout_mode == "A/B Difference") {
            layout_data.custom_layout_data_["mode"]       = 3;
            layout_data.custom_layout_data_["monochrome"] = monochrome_->value();
            layout_data.custom_layout_data_["boost"]      = difference_boost_->value();
        } else if (layout_mode == "Screen") {
            layout_data.custom_layout_data_["mode"]   = 4;
            layout_data.custom_layout_data_["screen"] = true;
        }

        layout_data.layout_aspect_ = image_aspect(image_set->onscreen_image(wipeA));

    } else {

        // draw the images in reverse order. So Image selection 0 is drawn
        // 'over' image 1 etc (if in over mode)
        for (int i = 0; i < num_images; ++i) {
            layout_data.image_draw_order_hint_.push_back(num_images - i - 1);
        }
        layout_data.custom_layout_data_["first_im"] =
            layout_data.image_draw_order_hint_.front();

        // identity matrices here, no transform for 'over' mode'
        layout_data.image_transforms_.resize(image_set->num_onscreen_images());

        layout_data.layout_aspect_ = image_aspect(image_set->onscreen_image(num_images - 1));
        layout_data.custom_layout_data_["mode"] = layout_mode == "Over" ? 1 : 2;
    }
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<CompositeViewportLayout>>(
                CompositeViewportLayout::PLUGIN_UUID,
                "CompositeViewportLayout",
                plugin_manager::PluginFlags::PF_VIEWPORT_RENDERER,
                true,
                "Ted Waine",
                "Composite Viewport Layout Plugin")}));
}
}
