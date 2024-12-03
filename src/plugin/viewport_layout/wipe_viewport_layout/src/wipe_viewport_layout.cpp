// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "wipe_viewport_layout.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_multi_buffered_texture.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio;


using namespace xstudio::ui::opengl;

class OpenGLViewportWipeRenderer: public OpenGLViewportRenderer {

    public:

    OpenGLViewportWipeRenderer(const std::string &window_id) : OpenGLViewportRenderer(window_id) {}

    virtual ~OpenGLViewportWipeRenderer() {
        if (wipe_vbo_) glDeleteBuffers(1, &wipe_vbo_);
        if (wipe_vao_) glDeleteVertexArrays(1, &wipe_vao_);
    }

    void pre_init() override {
        OpenGLViewportRenderer::pre_init();
        glGenBuffers(1, &wipe_vbo_);
        glGenVertexArrays(1, &wipe_vao_);
    }

    void draw_image(
                const media_reader::ImageBufPtr &image,
                const media_reader::ImageSetLayoutDataPtr &layout_data,
                const int index,
                const Imath::M44f &window_to_viewport_matrix,
                const Imath::M44f &viewport_to_image_space,
                const float viewport_du_dx) override;

    unsigned int wipe_vbo_ = 0;
    unsigned int wipe_vao_ = 0;

};

void OpenGLViewportWipeRenderer::draw_image(
    const media_reader::ImageBufPtr &image_to_be_drawn,
    const media_reader::ImageSetLayoutDataPtr &layout_data,
    const int index,
    const Imath::M44f &window_to_viewport_matrix,
    const Imath::M44f &viewport_to_image_space,
    const float viewport_du_dx) {

    // wipe value is the position of the wipe handle normalised to the 
    // viewport width.
    float wipe_screen_space = 0.5f;
    if (layout_data && layout_data->custom_layout_data_.contains("wipe_pos")) {
        wipe_screen_space = layout_data->custom_layout_data_.value("wipe_pos", 0.5f);
    }

    const bool first_image = layout_data->image_draw_order_hint_.size() > 1 && index == layout_data->image_draw_order_hint_[1] ? false : true;

    if (wipe_screen_space <= 0.011f && first_image) {
        // wipe at the far left of the screen. Don't draw the wipe
        return;
    } else if (wipe_screen_space > 0.989f && !first_image) {
        // wipe at the far right of the screen. Don't draw the wipe
        return;
    }

    // re-normalise to -1.0,1.0 and multiply by projection matrix to get wipe
    // position in image space
    Imath::V4f w(-1.0f+wipe_screen_space*2.0f, 0.0f, 0.0f, 1.0f);
    w *= viewport_to_image_space;
    float wipe_pos_image_space = w.x/w.w;

    if (wipe_pos_image_space <= -1.0f && first_image) {
        // wipe is all the way left. Only draw second image!
        return;
    } else if (wipe_pos_image_space > 0.9999f && !first_image) {
        // wipe is all the way right. Don't draw second image!
        return;
    }

    const bool no_wipe = layout_data->image_draw_order_hint_.size() < 2 || (wipe_screen_space <= 0.011f || wipe_screen_space > 0.989f)
        || (wipe_pos_image_space <= -1.0f || wipe_pos_image_space > 0.9999f);


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

    {

        float left = no_wipe ? -1.0f : first_image ? -1.0 : wipe_pos_image_space;
        float right = no_wipe ? 1.0f : first_image ? wipe_pos_image_space : 1.0;
        std::array<float, 24> vertices = {
        // 1st triangle
        left, 1.0f, 0.0f, 1.0f, // top left
        right, 1.0f, 0.0f, 1.0f, // top right
        right, -1.0f, 0.0f, 1.0f, // bottom right
        // 2nd triangle
        right, -1.0f, 0.0f, 1.0f, // bottom right
        left, 1.0f, 0.0f, 1.0f, // top left
        left, -1.0f, 0.0f, 1.0f // bottom left
        };

        glBindVertexArray(wipe_vao_);
        // 2. copy our vertices array in a buffer for OpenGL to use
        glBindBuffer(GL_ARRAY_BUFFER, wipe_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
        // 3. then set our vertex module pointers
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // the actual draw!
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);

    }


    glUseProgram(0);

}

WipeViewportLayout::WipeViewportLayout(
    caf::actor_config &cfg,
    const utility::JsonStore &init_settings)
    : ViewportLayoutPlugin(cfg, init_settings) {

    wipe_position_ = add_vec4f_attribute("Wipe Position", "Wipe Position", Imath::V4f(0.5f,0.5f,0.0f,1.0f));
    wipe_position_->set_redraw_viewport_on_change(true);
    wipe_position_->set_role_data(module::Attribute::ToolTip, "Spacing between images in grid layout as a % of image size.");
    add_layout_settings_attribute(wipe_position_, "Wipe");
    wipe_position_->expose_in_ui_attrs_group("wipe_layout_attrs");

    add_layout_mode("Wipe", playhead::AssemblyMode::AM_TEN, playhead::AutoAlignMode::AAM_ALIGN_FRAMES);

    add_viewport_layout_qml_overlay(
        "Wipe",
        R"(
            import WipeLayoutOverlay 1.0
            WipeLayoutOverlay {
            }
        )"
        );
}

void WipeViewportLayout::do_layout(
    const std::string &layout_mode,
    const media_reader::ImageBufDisplaySetPtr &image_set,
    media_reader::ImageSetLayoutData &layout_data
    )
{
    const int num_images = image_set->num_onscreen_images();
    if (!num_images) {
        return;
    }

    // if 'hero' image is at index 0, we wipe between image 0 and image 1
    // otherwise we wipe between hero image and image 0.
    int wipeA = image_set->hero_sub_playhead_index();

    layout_data.image_draw_order_hint_.push_back(wipeA);
    if (num_images > 1) {
        int wipeB = image_set->previous_hero_sub_playhead_index() != -1 ? image_set->previous_hero_sub_playhead_index() : wipeA ? 0 : 1;
        layout_data.image_draw_order_hint_.push_back(wipeB);
    }

    // identity matrices here, no transform for A/B wipes
    layout_data.image_transforms_.resize(image_set->num_onscreen_images());
    layout_data.custom_layout_data_["wipe_pos"] = wipe_position_->value().x;

    layout_data.layout_aspect_ = image_set->onscreen_image(wipeA) ? image_set->onscreen_image(wipeA)->image_aspect() : 16.0f/9.0f;    

}

ViewportRenderer * WipeViewportLayout::make_renderer(const std::string &window_id) {
    return new OpenGLViewportWipeRenderer(window_id);
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<WipeViewportLayout>>(
                WipeViewportLayout::PLUGIN_UUID,
                "WipeViewportLayout",
                plugin_manager::PluginFlags::PF_VIEWPORT_RENDERER,
                true,
                "Ted Waine",
                "Wipe Viewport Layout Plugin")}));
}
}
