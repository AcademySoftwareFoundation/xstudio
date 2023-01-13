// SPDX-License-Identifier: Apache-2.0
#include "viewport_hud.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"

#include <GL/glew.h>
#include <GL/gl.h>

using namespace xstudio;
using namespace xstudio::ui::viewport;

void ViewportHUDRenderer::render_opengl_before_image(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const xstudio::media_reader::ImageBufPtr &frame) {

    bool hud_on            = false;
    bool draw_bbox         = false;
    bool draw_image_bounds = false;

    utility::BlindDataObjectPtr render_data =
        frame.plugin_blind_data(utility::Uuid("f8a09960-606d-11ed-9b6a-0242ac120002"));
    const auto *data = dynamic_cast<const HudData *>(render_data.get());
    if (data) {
        hud_on            = data->hud_params_["hud_selection"];
        draw_bbox         = hud_on && frame && data->hud_params_["bounding_box"];
        draw_image_bounds = hud_on && frame && data->hud_params_["image_borders"];
    } else {
        return;
    }

    Imath::M44f transform_matrix =
        transform_viewport_to_image_space.inverse() * transform_window_to_viewport_space;

    auto image_dims       = frame ? frame->image_size_in_pixels() : Imath::V2i();
    auto image_bounds_min = frame ? frame->image_pixels_bounding_box().min : Imath::V2i();
    auto image_bounds_max = frame ? frame->image_pixels_bounding_box().max : Imath::V2i();
    auto pixel_aspect     = frame ? frame->pixel_aspect() : 1.0f;

    if (draw_bbox) {
        draw_gl_box(
            image_bounds_min,
            image_bounds_max,
            image_dims,
            transform_matrix,
            pixel_aspect,
            Imath::V3f(0.f, 1.f, 0.f));
    }

    if (draw_image_bounds) {
        draw_gl_box(
            Imath::V2f(0.f, 0.f),
            image_dims,
            image_dims,
            transform_matrix,
            pixel_aspect,
            Imath::V3f(1.f, 0.f, 0.f));
    }
}

void ViewportHUDRenderer::draw_gl_box(
    Imath::V2i top_left_pt,
    Imath::V2i bottom_right_pt,
    Imath::V2i image_dims,
    Imath::M44f transform_matrix,
    const float pixel_aspect,
    Imath::V3f color) {

    Imath::V2f top_left =
        get_transformed_point(top_left_pt, image_dims, transform_matrix, pixel_aspect);
    Imath::V2f bottom_right =
        get_transformed_point(bottom_right_pt, image_dims, transform_matrix, pixel_aspect);

    glUseProgram(0);
    glLineWidth(3.0);
    glColor4f(color.x, color.y, color.z, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(top_left.x, top_left.y);
    glVertex2f(bottom_right.x, top_left.y);
    glVertex2f(bottom_right.x, bottom_right.y);
    glVertex2f(top_left.x, bottom_right.y);
    glEnd();
}

Imath::V2f ViewportHUDRenderer::get_transformed_point(
    Imath::V2i point,
    Imath::V2i image_dims,
    Imath::M44f transform_matrix,
    const float pixel_aspect) {
    const float aspect = float(image_dims.y) / float(image_dims.x);

    float norm_x = float(point.x) / image_dims.x;
    float norm_y = float(point.y) / image_dims.y;

    Imath::V4f a(norm_x * 2.0f - 1.0f, (norm_y * 2.0f - 1.0f) * aspect, 0.0f, 1.0f);

    a.y /= pixel_aspect;
    a *= transform_matrix;

    return Imath::V2f(a.x / a.w, a.y / a.w);
}

ViewportHUD::ViewportHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "ViewportHUD", init_settings) {
    bounding_box = add_boolean_attribute("Show Image Bounding Box", "bbox", true);

    bounding_box->expose_in_ui_attrs_group("viewport_hud_settings");

    image_borders_box = add_boolean_attribute("Show EXR Data Window", "imgbox", true);

    image_borders_box->expose_in_ui_attrs_group("viewport_hud_settings");

    hud_selection_ =
        add_string_choice_attribute("Hud", "Hud", "Off", {"Off", "On"}, {"Off", "On"});
    hud_selection_->set_tool_tip("Access HUD controls");
    hud_selection_->expose_in_ui_attrs_group("main_toolbar");
    hud_selection_->expose_in_ui_attrs_group("popout_toolbar");

    bounding_box->set_preference_path("/plugin/viewport_hud/show_image_bounding_box");
    image_borders_box->set_preference_path("/plugin/viewport_hud/show_exr_data_window");
    hud_selection_->set_preference_path("/plugin/viewport_hud/enable_hud");

    // here we set custom QML code to implement a custom widget that is inserted
    // into the viewer toolbox.
    hud_selection_->set_role_data(
        module::Attribute::QmlCode,
        R"(
            import ViewportHUD 1.0
            ViewportHUDButton {
                id: control
                anchors.fill: parent
            }
        )");

    hud_selection_->set_role_data(module::Attribute::ToolbarPosition, 0.0f);
}

ViewportHUD::~ViewportHUD() = default;

utility::BlindDataObjectPtr
ViewportHUD::prepare_render_data(const media_reader::ImageBufPtr &image) const {

    auto r = utility::BlindDataObjectPtr();

    try {
        utility::JsonStore j;
        j["bounding_box"]  = bounding_box->value();
        j["image_borders"] = image_borders_box->value();
        j["hud_selection"] = hud_selection_->value() == "On";
        r.reset(new HudData(j));

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return r;
}

void ViewportHUD::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/
) {

    redraw_viewport();
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<ViewportHUD>>(
                utility::Uuid("f8a09960-606d-11ed-9b6a-0242ac120002"),
                "ViewportHUD",
                plugin_manager::PluginType::PT_VIEWPORT_OVERLAY,
                true,
                "Clement Jovet",
                "Viewport HUD Plugin")}));
}
}
