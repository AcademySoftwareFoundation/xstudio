// SPDX-License-Identifier: Apache-2.0
#include "exr_data_window.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"

#include <GL/glew.h>
#include <GL/gl.h>

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {
class HudData : public utility::BlindDataObject {
  public:
    HudData(const utility::JsonStore &j) : hud_params_(j) {}
    ~HudData() override = default;

    const utility::JsonStore hud_params_;
};

class EXRDataWindowRenderer : public plugin::ViewportOverlayRenderer {

  public:
    void render_opengl(
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        const float /*viewport_du_dpixel*/,
        const xstudio::media_reader::ImageBufPtr &frame,
        const bool have_alpha_buffer) override {

        auto get_transformed_point = [](Imath::V2i point,
                                        Imath::V2i image_dims,
                                        Imath::M44f transform_matrix,
                                        const float pixel_aspect) -> Imath::V2f {
            const float aspect = float(image_dims.y) / float(image_dims.x);

            float norm_x = float(point.x) / image_dims.x;
            float norm_y = float(point.y) / image_dims.y;

            Imath::V4f a(norm_x * 2.0f - 1.0f, (norm_y * 2.0f - 1.0f) * aspect, 0.0f, 1.0f);

            a.y /= pixel_aspect;
            a *= transform_matrix;

            return Imath::V2f(a.x / a.w, a.y / a.w);
        };

        bool draw_bbox         = false;
        bool draw_image_bounds = false;

        utility::BlindDataObjectPtr render_data =
            frame.plugin_blind_data(utility::Uuid("f8a09960-606d-11ed-9b6a-0242ac120002"));
        const auto *data = dynamic_cast<const HudData *>(render_data.get());
        if (data && frame) {

            Imath::M44f transform_matrix = transform_viewport_to_image_space.inverse() *
                                           transform_window_to_viewport_space;

            auto image_dims = frame ? frame->image_size_in_pixels() : Imath::V2i();
            auto image_bounds_min =
                frame ? frame->image_pixels_bounding_box().min : Imath::V2i();
            auto image_bounds_max =
                frame ? frame->image_pixels_bounding_box().max : Imath::V2i();
            auto pixel_aspect = frame ? frame->pixel_aspect() : 1.0f;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquation(GL_FUNC_ADD);
            glDisable(GL_DEPTH_TEST);

            const utility::ColourTriplet c = data->hud_params_["colour"];
            const float w                  = data->hud_params_["width"];

            Imath::V2f top_left = get_transformed_point(
                image_bounds_min, image_dims, transform_matrix, pixel_aspect);
            Imath::V2f bottom_right = get_transformed_point(
                image_bounds_max, image_dims, transform_matrix, pixel_aspect);

            glUseProgram(0);
            glLineWidth(w);
            glColor4f(c.r, c.g, c.b, 1.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(top_left.x, top_left.y);
            glVertex2f(bottom_right.x, top_left.y);
            glVertex2f(bottom_right.x, bottom_right.y);
            glVertex2f(top_left.x, bottom_right.y);
            glEnd();
        }
    }
};
} // namespace

EXRDataWindowHUD::EXRDataWindowHUD(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : HUDPluginBase(cfg, "OpenEXR Data Window", init_settings) {

    enabled_->set_preference_path("/plugin/exr_data_window/enabled");
    enabled_->set_role_data(module::Attribute::ToolbarPosition, 1.0f);

    colour_ =
        add_colour_attribute("Line Colour", "Colour", utility::ColourTriplet(0.0f, 1.0f, 0.0f));
    colour_->set_preference_path("/plugin/exr_data_window/colour");
    add_hud_settings_attribute(colour_);

    width_ = add_integer_attribute("Line Width", "Width", 1, 1, 5);
    width_->set_preference_path("/plugin/exr_data_window/width");
    add_hud_settings_attribute(width_);
}

plugin::ViewportOverlayRendererPtr EXRDataWindowHUD::make_overlay_renderer(const int) {
    return plugin::ViewportOverlayRendererPtr(new EXRDataWindowRenderer());
}

EXRDataWindowHUD::~EXRDataWindowHUD() = default;

utility::BlindDataObjectPtr EXRDataWindowHUD::prepare_overlay_data(
    const media_reader::ImageBufPtr &image, const bool /*offscreen*/) const {

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
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY,
                true,
                "Clement Jovet",
                "Viewport HUD Plugin")}));
}
}
