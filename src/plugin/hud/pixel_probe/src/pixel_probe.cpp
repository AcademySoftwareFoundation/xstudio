// SPDX-License-Identifier: Apache-2.0
#include "pixel_probe.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

#include <GL/glew.h>
#include <GL/gl.h>

using namespace xstudio;
using namespace xstudio::ui::viewport;

/*void PixelProbeHUDRenderer::render_opengl(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    if (frame) {

        std::lock_guard<std::mutex> m(mutex_);

        Imath::V2i image_dims  = frame->image_size_in_pixels();
        // Image is width-fitted to viewport coordinates -1.0 to 1.0:
        const float image_aspect =
            image_dims.y / (image_dims.x * frame->pixel_aspect());
        Imath::V2i image_coord(
            int(round(
                (mouse_pointer_position_.x + 1.0f) * 0.5f *
                frame->image_size_in_pixels().x)),
            int(round(
                (mouse_pointer_position_.y/image_aspect + 1.0f) * 0.5f *
                frame->image_size_in_pixels().y)));

        const auto pixel_info = frame->pixel_info(image_coord);

        if (!text_renderer_) {
            text_renderer_ = std::make_unique<ui::opengl::OpenGLTextRendererSDF>(
                utility::xstudio_root("/fonts/Overpass-Regular.ttf"), 96);
        }

        auto font_scale = 30.0f * 1920.0f * viewport_du_dpixel;

        std::stringstream ss;
        for (const auto cd_val: pixel_info.code_values_data()) {
            ss << "Code Val (" << cd_val.channel_name << ") : " << cd_val.pixel_value << "\n";
        }
        for (const auto cd_val: pixel_info.channels_data()) {
            ss << cd_val.channel_name << " (RAW) : " << cd_val.pixel_value << "\n";
        }

        ss << " @ pixel (" << pixel_info.location_in_image().x << "," <<
pixel_info.location_in_image().y << ")";

        text_renderer_->precompute_text_rendering_vertex_layout(
            precomputed_text_vertex_buffer_,
            ss.str(),
            Imath::V2f(0.0f,0.0f),
            1.0f,
            font_scale,
            JustifyLeft,
            1.0f);

        text_renderer_->render_text(
            precomputed_text_vertex_buffer_,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            utility::ColourTriplet(1.0, 0.0, 1.0),
            viewport_du_dpixel,
            font_scale,
            1.0f);
    }

}

void PixelProbeHUDRenderer::set_mouse_pointer_position(const Imath::V2f p)
{
    std::lock_guard<std::mutex> m(mutex_);
    mouse_pointer_position_ = p;
}*/

PixelProbeHUD::PixelProbeHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : HUDPluginBase(cfg, "Pixel Probe", init_settings) {
    pixel_info_text_ = add_string_attribute("Pixel Info", "Pixel Info", "");
    pixel_info_text_->expose_in_ui_attrs_group("pixel_info_attributes");

    pixel_info_title_ = add_string_attribute("Pixel Info Title", "Pixel Info Title", "");
    pixel_info_title_->expose_in_ui_attrs_group("pixel_info_attributes");

    // expose the enabled toggle (from HUDPluginBase)
    enabled_->set_value(false);
    enabled_->expose_in_ui_attrs_group("pixel_info_attributes");
    enabled_->set_preference_path("/plugin/pixel_probe/enabled");
    enabled_->set_role_data(module::Attribute::ToolbarPosition, 3.0f);

    // Here we declare QML code that is reponsible for drawing the pixel info over the xSTUDIO
    // viewport. The main Viewport class will take care of instanciating the qml object from
    // this code.
    hud_element_qml(
        R"(
            import PixelProbe 1.0
            PixelProbeOverlay {
            }
        )",
        HUDPluginBase::TopLeft);

    auto font_size = add_float_attribute("Font Size", "Font Size", 10.0f, 5.0f, 50.0f, 1.0f);
    font_size->expose_in_ui_attrs_group("pixel_info_attributes");
    font_size->set_tool_tip("Sets the font size for the pixel info overlay.");
    add_hud_settings_attribute(font_size);

    auto font_opacity =
        add_float_attribute("Font Opacity", "Font Opacity", 0.8f, 0.0f, 1.0f, 0.05f);
    font_opacity->expose_in_ui_attrs_group("pixel_info_attributes");
    font_opacity->set_tool_tip("Sets the opactity for the pixel info text.");
    add_hud_settings_attribute(font_opacity);

    auto bg_opacity = add_float_attribute("Bg Opacity", "Bg Opacity", 0.5f, 0.0f, 1.0f, 0.05f);
    bg_opacity->expose_in_ui_attrs_group("pixel_info_attributes");
    bg_opacity->set_tool_tip("Sets the opactity for dark backdrop behind the pixel info text.");
    add_hud_settings_attribute(bg_opacity);

    show_code_values_ = add_boolean_attribute("Code Values", "Code Values", false);
    add_hud_settings_attribute(show_code_values_);
    show_code_values_->set_tool_tip(
        "Toggles whether raw pixel code values are shown, if available.");
    add_hud_settings_attribute(show_code_values_);

    show_raw_pixel_values_ =
        add_boolean_attribute("Raw Pixel RGB Channel Values", "Raw Pix Vals", false);
    add_hud_settings_attribute(show_raw_pixel_values_);
    show_raw_pixel_values_->set_tool_tip(
        "Toggles whether raw pixel values are shown (after conversion to nermalized RGB).");

    show_linear_pixel_values_ =
        add_boolean_attribute("Linear Pixel Channel Values", "Lin Pix Vals", false);
    add_hud_settings_attribute(show_linear_pixel_values_);
    show_linear_pixel_values_->set_tool_tip(
        "Toggles whether pixel values are shown after conversion to linear colour space (where "
        "available).");

    show_final_screen_rgb_pixel_values_ =
        add_boolean_attribute("Final Pixel Channel Values", "Final Pix Vals", false);
    add_hud_settings_attribute(show_final_screen_rgb_pixel_values_);
    show_final_screen_rgb_pixel_values_->set_tool_tip(
        "Toggles whether pixel values are shown after display transformation (viewer LUT) has "
        "been applies.");

    value_precision_ = add_integer_attribute("Decimals", "Decimals", 2, 1, 6);
    add_hud_settings_attribute(value_precision_);
    value_precision_->set_tool_tip(
        "Set the number of decimal places when displaying float pixel values.");

    font_size->set_preference_path("/plugin/pixel_probe/font_size");
    font_opacity->set_preference_path("/plugin/pixel_probe/font_opacity");
    bg_opacity->set_preference_path("/plugin/pixel_probe/bg_opacity");
    show_code_values_->set_preference_path("/plugin/pixel_probe/show_code_values");
    show_raw_pixel_values_->set_preference_path("/plugin/pixel_probe/show_raw_pixel_values");
    show_linear_pixel_values_->set_preference_path(
        "/plugin/pixel_probe/show_linear_pixel_values");
    show_final_screen_rgb_pixel_values_->set_preference_path(
        "/plugin/pixel_probe/show_final_screen_rgb_pixel_values");
    value_precision_->set_preference_path("/plugin/pixel_probe/decimals");
}

PixelProbeHUD::~PixelProbeHUD() { colour_pipeline_ = caf::actor(); }


bool PixelProbeHUD::pointer_event(const ui::PointerEvent &e) {

    last_pointer_pos_ = e.position_in_viewport_coord_sys();
    update_onscreen_info();
    return true;
}

void PixelProbeHUD::update_onscreen_info() {

    if (is_enabled_ != enabled_->value()) {
        is_enabled_ = enabled_->value();
        if (is_enabled_) {
            listen_to_playhead_events();
            connect_to_ui();
        } else {
            listen_to_playhead_events(false);
            current_onscreen_image_ = media_reader::ImageBufPtr();
            // disconnect_from_ui();
        }
    }

    static Imath::V2i image_dims(1920, 1080);

    if (current_onscreen_image_ && enabled_->value()) {

        // WIP!
        image_dims = current_onscreen_image_->image_size_in_pixels();

        // Image is width-fitted to viewport coordinates -1.0 to 1.0:
        const float image_aspect =
            image_dims.y / (image_dims.x * current_onscreen_image_->pixel_aspect());
        Imath::V2i image_coord(
            int(round((last_pointer_pos_.x + 1.0f) * 0.5f * image_dims.x)),
            int(round((last_pointer_pos_.y / image_aspect + 1.0f) * 0.5f * image_dims.y)));

        const auto pixel_info = current_onscreen_image_->pixel_info(image_coord);

        if (!colour_pipeline_)
            get_colour_pipeline_actor();

        if (colour_pipeline_) {

            // we send the pixel info to the colour pipeline to add it's own colourspace
            // transforms and info, if it needs/wants to
            request(
                colour_pipeline_,
                infinite,
                colour_pipeline::pixel_info_atom_v,
                pixel_info,
                current_onscreen_image_.frame_id())
                .then(
                    [=](const media_reader::PixelInfo &extended_info) mutable {
                        make_pixel_info_onscreen_text(extended_info);
                    },
                    [=](caf::error &err) {

                    });
        }

    } else if (enabled_->value()) {
        const float image_aspect = image_dims.y / (image_dims.x);
        Imath::V2i image_coord(
            int(round((last_pointer_pos_.x + 1.0f) * 0.5f * image_dims.x)),
            int(round((last_pointer_pos_.y / image_aspect + 1.0f) * 0.5f * image_dims.y)));
        make_pixel_info_onscreen_text(media_reader::PixelInfo(image_coord));
    } else {
        pixel_info_text_->set_value("");
        pixel_info_title_->set_value("");
    }
}

void PixelProbeHUD::make_pixel_info_onscreen_text(const media_reader::PixelInfo &pixel_info) {

    static auto pixel_info_to_display_text =
        [](const std::string &name,
           const media_reader::PixelInfo::PixelChannelsData &info,
           const int precision,
           std::stringstream &ss) {
            if (info.empty()) {
                ss << name << " (): N/A";
                return;
            }
            ss << name << " (";
            bool is_code_value = false;
            for (size_t i = 0; i < info.size(); ++i) {
                ss << info[i].channel_name;
                if (info[i].pixel_value == media_reader::PixelInfo::NullPixelValue)
                    is_code_value = true;
                if (i != (info.size() - 1))
                    ss << ",";
            }
            ss << "): ";
            if (is_code_value) {
                char buf[128];
                for (size_t i = 0; i < info.size(); ++i) {
                    sprintf(buf, "%d", info[i].code_value);
                    int l = strlen(buf);
                    while (l < 4) {
                        ss << " ";
                        l++;
                    }
                    ss << buf;
                    if (i != (info.size() - 1))
                        ss << ",";
                }
            } else {
                char prc[128];
                sprintf(prc, "%%.%df", precision);
                char buf[128];
                for (size_t i = 0; i < info.size(); ++i) {
                    sprintf(buf, prc, info[i].pixel_value);
                    ss << buf;
                    if (i != (info.size() - 1))
                        ss << ", ";
                }
            }
        };

    std::stringstream ss;
    if (show_code_values_->value()) {
        pixel_info_to_display_text(
            "Code Values", pixel_info.code_values_info(), value_precision_->value(), ss);
    }
    if (show_raw_pixel_values_->value()) {
        if (ss.str().size())
            ss << "\n";
        pixel_info_to_display_text(
            pixel_info.raw_colourspace_name(),
            pixel_info.raw_channels_info(),
            value_precision_->value(),
            ss);
    }
    if (show_linear_pixel_values_->value()) {
        if (ss.str().size())
            ss << "\n";
        pixel_info_to_display_text(
            pixel_info.linear_colourspace_name(),
            pixel_info.linear_channels_info(),
            value_precision_->value(),
            ss);
    }
    if (show_final_screen_rgb_pixel_values_->value()) {
        if (ss.str().size())
            ss << "\n";
        pixel_info_to_display_text(
            pixel_info.display_colourspace_name(),
            pixel_info.rgb_display_info(),
            value_precision_->value(),
            ss);
    }
    pixel_info_text_->set_value(ss.str());

    std::stringstream ss2;
    ss2 << "Pixel Values ";
    if (!pixel_info.layer_name().empty()) {
        ss2 << "(" << pixel_info.layer_name() << ") ";
    }
    ss2 << "@ " << pixel_info.location_in_image().x << ", " << pixel_info.location_in_image().y;
    pixel_info_title_->set_value(ss2.str());
}


void PixelProbeHUD::get_colour_pipeline_actor() {

    auto colour_pipe_manager = system().registry().get<caf::actor>(colour_pipeline_registry);
    caf::scoped_actor sys(system());
    colour_pipeline_ = utility::request_receive<caf::actor>(
        *sys,
        colour_pipe_manager,
        xstudio::colour_pipeline::colour_pipeline_atom_v,
        "viewport0");
    link_to(colour_pipeline_);
}

void PixelProbeHUD::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {
    update_onscreen_info();
    HUDPluginBase::attribute_changed(attribute_uuid, role);
}

void PixelProbeHUD::on_screen_image(const media_reader::ImageBufPtr &buf) {
    current_onscreen_image_ = buf;
    update_onscreen_info();
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<PixelProbeHUD>>(
                utility::Uuid("9437e200-80da-4725-97d7-02d5a11b3af1"),
                "PixelProbeHUD",
                plugin_manager::PluginType::PT_HEAD_UP_DISPLAY,
                true,
                "Ted Waine",
                "Viewport HUD Plugin")}));
}
}
