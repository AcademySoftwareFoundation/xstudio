// SPDX-License-Identifier: Apache-2.0
#include "image_boundary_hud.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"

#ifdef __apple__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {

class HudData : public utility::BlindDataObject {
  public:
    HudData(const utility::JsonStore &j) : hud_params_(j) {}
    ~HudData() override = default;

    const utility::JsonStore hud_params_;
};

const char *vertex_shader = R"(
    #version 330 core
    layout (location = 0) in vec4 aPos;
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform float image_aspect;

    void main()
    {
        vec4 rpos = aPos;
        rpos.y = rpos.y/image_aspect;
        gl_Position = (rpos*to_coord_system*to_canvas);
    }
    )";

const char *frag_shader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 line_colour;
    void main(void)
    {
        FragColor = vec4(line_colour, 1.0f);
    }

    )";

class ImageBoundaryRenderer : public plugin::ViewportOverlayRenderer {

  public:
    void render_image_overlay(
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        const float /*viewport_du_dpixel*/,
        const float /*device_pixel_ratio*/,
        const xstudio::media_reader::ImageBufPtr &frame) override {

        utility::BlindDataObjectPtr render_data =
            frame.plugin_blind_data(utility::Uuid("95268f7c-88d1-48da-8543-c5275ef5b2c5"));
        const auto *data = dynamic_cast<const HudData *>(render_data.get());
        if (data && frame) {

            if (!shader_)
                init_overlay_opengl();

            utility::JsonStore shader_params;
            shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
            shader_params["to_canvas"]       = transform_window_to_viewport_space;
            shader_params["image_transform_matrix"] = frame.layout_transform();
            shader_params["image_aspect"]           = image_aspect(frame);
            shader_params["line_colour"]            = data->hud_params_["colour"];
            shader_->set_shader_parameters(shader_params);

            glLineWidth(data->hud_params_["width"]);
            shader_->use();
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(vertex_array_object_);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            shader_->stop_using();
            glBindVertexArray(0);
        }
    }

    void init_overlay_opengl() {

        glGenBuffers(1, &vertex_buffer_object_);
        glGenVertexArrays(1, &vertex_array_object_);

        // NOLINT
        static std::array<float, 16> vertices = {
            -1.0,
            -1.0,
            0.0f,
            1.0f,
            1.0,
            -1.0,
            0.0f,
            1.0f,
            1.0,
            1.0,
            0.0f,
            1.0f,
            -1.0,
            1.0,
            0.0f,
            1.0f};

        glBindVertexArray(vertex_array_object_);
        // 2. copy our vertices array in a buffer for OpenGL to use
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
        // 3. then set our vertex module pointers
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
    }

    std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
    GLuint vertex_buffer_object_;
    GLuint vertex_array_object_;
};
} // namespace

ImageBoundaryHUD::ImageBoundaryHUD(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::HUDPluginBase(cfg, "Image Boundary", init_settings) {

    colour_ =
        add_colour_attribute("Line Colour", "Colour", utility::ColourTriplet(1.0f, 0.0f, 0.0f));
    colour_->set_preference_path("/plugin/image_boundary/colour");
    add_hud_settings_attribute(colour_);

    width_ = add_integer_attribute("Line Width", "Width", 1, 1, 5);
    width_->set_preference_path("/plugin/image_boundary/width");
    add_hud_settings_attribute(width_);
}

plugin::ViewportOverlayRendererPtr
ImageBoundaryHUD::make_overlay_renderer(const std::string &viewport_name) {
    return plugin::ViewportOverlayRendererPtr(new ImageBoundaryRenderer());
}

ImageBoundaryHUD::~ImageBoundaryHUD() = default;

utility::BlindDataObjectPtr ImageBoundaryHUD::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string & /*viewport_name*/,
    const utility::Uuid &playhead_uuid,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    auto r = utility::BlindDataObjectPtr();

    try {
        if (image && visible()) {
            utility::JsonStore j;
            j["colour"] = colour_->value();
            j["width"]  = width_->value();
            r.reset(new HudData(j));
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return r;
}

void ImageBoundaryHUD::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/
) {

    redraw_viewport();
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<ImageBoundaryHUD>>(
                utility::Uuid("95268f7c-88d1-48da-8543-c5275ef5b2c5"),
                "ImageBoundaryHUD",
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY |
                    plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true,
                "Clement Jovet",
                "Viewport HUD Plugin")}));
}
}
