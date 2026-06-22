// SPDX-License-Identifier: Apache-2.0
#include "exr_data_window.hpp"
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
        //rpos.y = rpos.y/image_aspect;
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

class EXRDataWindowRenderer : public plugin::ViewportOverlayRenderer {

  public:
    Imath::V2f
    get_transformed_point(Imath::V2i point, Imath::V2i image_dims, const float pixel_aspect) {
        const float aspect = float(image_dims.y) / float(image_dims.x);

        float norm_x = float(point.x) / image_dims.x;
        float norm_y = float(point.y) / image_dims.y;

        return Imath::V2f(norm_x * 2.0f - 1.0f, (norm_y * 2.0f - 1.0f) * aspect / pixel_aspect);
    };

    void render_image_overlay(
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        const float /*viewport_du_dpixel*/,
        const float /*device_pixel_ratio*/,
        const xstudio::media_reader::ImageBufPtr &frame) override {

        utility::BlindDataObjectPtr render_data =
            frame.plugin_blind_data(utility::Uuid("f8a09960-606d-11ed-9b6a-0242ac120002"));
        const auto *data = dynamic_cast<const HudData *>(render_data.get());
        if (data && frame) {

            if (!shader_)
                init_overlay_opengl();

            auto image_dims = frame ? frame->image_size_in_pixels() : Imath::V2i();
            auto image_bounds_min =
                frame ? frame->image_pixels_bounding_box().min : Imath::V2i();
            auto image_bounds_max =
                frame ? frame->image_pixels_bounding_box().max : Imath::V2i();

            Imath::V2f top_left = get_transformed_point(
                image_bounds_min, image_dims, frame.frame_id().pixel_aspect());
            Imath::V2f bottom_right = get_transformed_point(
                image_bounds_max, image_dims, frame.frame_id().pixel_aspect());

            // NOLINT
            std::array<float, 16> vertices = {
                top_left.x,
                top_left.y,
                0.0f,
                1.0f,
                top_left.x,
                bottom_right.y,
                0.0f,
                1.0f,
                bottom_right.x,
                bottom_right.y,
                0.0f,
                1.0f,
                bottom_right.x,
                top_left.y,
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
        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
    }

    std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
    GLuint vertex_buffer_object_;
    GLuint vertex_array_object_;
};
} // namespace

EXRDataWindowHUD::EXRDataWindowHUD(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::HUDPluginBase(cfg, "OpenEXR Data Window", init_settings, 1.0f) {

    colour_ =
        add_colour_attribute("Line Colour", "Colour", utility::ColourTriplet(0.0f, 1.0f, 0.0f));
    colour_->set_preference_path("/plugin/exr_data_window/colour");
    add_hud_settings_attribute(colour_);

    width_ = add_integer_attribute("Line Width", "Width", 1, 1, 5);
    width_->set_preference_path("/plugin/exr_data_window/width");
    add_hud_settings_attribute(width_);
}

plugin::ViewportOverlayRendererPtr
EXRDataWindowHUD::make_overlay_renderer(const std::string &viewport_name) {
    return plugin::ViewportOverlayRendererPtr(new EXRDataWindowRenderer());
}

EXRDataWindowHUD::~EXRDataWindowHUD() = default;

utility::BlindDataObjectPtr EXRDataWindowHUD::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string & /*viewport_name*/,
    const utility::Uuid &playhead_uuid,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    auto r = utility::BlindDataObjectPtr();

    try {
        if (image && visible() && image->params()["reader"] == "OpenEXR") {
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

void EXRDataWindowHUD::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/
) {

    redraw_viewport();
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<EXRDataWindowHUD>>(
                utility::Uuid("f8a09960-606d-11ed-9b6a-0242ac120002"),
                "EXRDataWindowHUD",
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY |
                    plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true,
                "Clement Jovet",
                "Viewport HUD Plugin")}));
}
}
