// SPDX-License-Identifier: Apache-2.0
#include "basic_viewport_masking.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

#include <GL/gl.h>
#include <GL/glew.h>

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {
const char *vertex_shader = R"(
    #version 330 core
    layout (location = 0) in vec4 aPos;
    out vec2 normImageCoordinate;
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform float image_aspect;

    void main()
    {
        vec4 rpos = aPos;
        rpos.y = rpos.y/image_aspect;
        gl_Position = (rpos*to_coord_system*to_canvas);
        normImageCoordinate = rpos.xy;
    }
    )";

const char *frag_shader = R"(
    #version 330 core
    in vec2 normImageCoordinate;
    out vec4 FragColor;
    uniform float image_aspect;
    uniform float mask_safety;
    uniform float mask_aspect_ratio;
    uniform float mask_opac;
    uniform float line_opac;
    uniform float line_thickness;
    uniform float uv_line_width;
    uniform float viewport_du_dx;

    vec4 gen_mask_colour(vec2 uv, float mask_width, float mask_height) {

        float x_p = min(uv.x+mask_width, mask_width-uv.x);
        float y_p = min(uv.y+mask_height, mask_height-uv.y);
        float p_p = min(x_p, y_p);

        // compute line colour from the overlap of the line thickness with
        // the fragment - viewport_du_dx etc. tell us how 'wide' a display
        // pixel is in units of the coordinate system of the viewport
        float p_p0 = p_p-viewport_du_dx*0.5;
        float p_p1 = p_p+viewport_du_dx*0.5;

        float line_start = -line_thickness*viewport_du_dx;
        float line_end = line_thickness*viewport_du_dx;

        float line_strength = 0.0;
        if (p_p0 > line_start && p_p1 < line_end) {
            line_strength = line_opac;
        } else if (p_p0 < line_end && line_end < p_p1) {
            line_strength = line_opac*(line_end-p_p0)/(line_thickness*viewport_du_dx);
        } else if (p_p1 > line_start && p_p1 < line_end) {
            line_strength = line_opac*(p_p1-line_start)/(line_thickness*viewport_du_dx);
        }

        if (line_strength > 0.0)
            return vec4(line_strength, line_strength, line_strength, line_strength);
        return p_p > 0.0 ? vec4(0.0,0.0,0.0,0.0) : vec4(0.0,0.0,0.0,mask_opac);

    }

    void main(void)
    {
        vec2 uv = vec2(normImageCoordinate.x, normImageCoordinate.y);

        // here the maske is 'fitted' to the image width ... the image is always
        // fitted into the viewport coordinates -1.0 to 1.0
        float mask_width = (100.0-mask_safety)/100.0f;
        float mask_height = (100.0-mask_safety)/(100.0f*mask_aspect_ratio);
        FragColor = gen_mask_colour(uv, mask_width, mask_height);
    }

    )";
} // namespace

void BasicMaskRenderer::render_opengl(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    if (!shader_)
        init_overlay_opengl();

    auto render_data =
        frame.plugin_blind_data(utility::Uuid("4006826a-6ff2-41ec-8ef2-d7a40bfd65e4"));
    const auto *data = dynamic_cast<const MaskData *>(render_data.get());
    if (data) {
        shader_->set_shader_parameters(data->mask_shader_params_);
    } else {
        return;
    }

    utility::JsonStore shader_params;
    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["viewport_du_dx"]  = viewport_du_dpixel;
    shader_params["image_transform_matrix"]  = frame.layout_transform();
    shader_params["image_aspect"]  = frame ? frame->image_aspect() : 16.0f/9.0f;
    shader_->set_shader_parameters(shader_params);

    shader_->use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(vertex_array_object_);
    glDrawArrays(GL_QUADS, 0, 4);
    shader_->stop_using();
    glBindVertexArray(0);

    auto aspect = data->mask_shader_params_["mask_aspect_ratio"].get<float>();

    // to make the label a costant size, regardless of the viewport transformation,
    // we scale the font plotting size with 'viewport_du_dx'
    auto font_scale = data->label_size_ * 1920.0f * viewport_du_dpixel;

    auto mask_safety    = data->mask_shader_params_["mask_safety"].get<float>();
    auto line_thickness = data->mask_shader_params_["line_thickness"].get<float>();
    auto ma             = mask_safety + aspect + line_thickness;

    if (text_ != data->mask_caption_ || font_scale != font_scale_ || ma != ma_) {
        font_scale_ = font_scale;
        text_       = data->mask_caption_;

        const Imath::V2f text_position =
            Imath::V2f(
                -1.0 - (line_thickness * viewport_du_dpixel),
                -1.0 / aspect - (line_thickness * viewport_du_dpixel) -
                    (viewport_du_dpixel * 24.0f)) *
            ((100.0f - mask_safety) / 100.0f);

        std::ignore = text_renderer_->precompute_text_rendering_vertex_layout(
            precomputed_text_vertex_buffer_,
            text_,
            text_position,
            1.0f,
            font_scale,
            JustifyLeft,
            1.0f);
    }

    text_renderer_->render_text(
        precomputed_text_vertex_buffer_,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        utility::ColourTriplet(1.0, 1.0, 1.0),
        viewport_du_dpixel,
        font_scale,
        data->mask_shader_params_["line_opac"].get<float>());
}

void BasicMaskRenderer::init_overlay_opengl() {

    glGenBuffers(1, &vertex_buffer_object_);
    glGenVertexArrays(1, &vertex_array_object_);

    static std::array<float, 16> vertices = {
        -1.0,
        1.0,
        0.0f,
        1.0f,
        1.0,
        1.0,
        0.0f,
        1.0f,
        1.0,
        -1.0,
        0.0f,
        1.0f,
        -1.0,
        -1.0,
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

    text_renderer_ = std::make_unique<ui::opengl::OpenGLTextRendererSDF>(
        utility::xstudio_root("/fonts/Overpass-Regular.ttf"), 96);
}

BasicViewportMasking::BasicViewportMasking(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::HUDPluginBase(cfg, "Mask", init_settings, 0.0f) {

    mask_aspect_ratio_ =
        add_float_attribute("Mask Aspect Ratio", "Aspect Ratio", 1.78f, 1.33f, 2.40f, 0.01f);
    add_hud_settings_attribute(mask_aspect_ratio_);
    mask_aspect_ratio_->set_tool_tip("Sets the mask aspect ratio");

    aspect_ratio_presets_ = add_string_choice_attribute(
        "Aspect Ratio Presets",
        "AR Presets",
        "...",
        std::vector<std::string>({"...", "1.33", "1.78", "2.0", "2.35", "2.39", "2.40"}));
    aspect_ratio_presets_->expose_in_ui_attrs_group("viewport_mask_settings");

    line_thickness_ =
        add_float_attribute("Line Thickness", "Thickness", 0.0f, 1.0f, 20.0f, 0.25f);
    add_hud_settings_attribute(line_thickness_);
    line_thickness_->set_tool_tip("Sets the thickness of the masking lines");
    line_thickness_->set_redraw_viewport_on_change(true);

    line_opacity_ = add_float_attribute("Line Opacity", "Line Opac.", 1.0f, 0.0f, 1.0f, 0.1f);
    add_hud_settings_attribute(line_opacity_);
    line_opacity_->set_tool_tip(
        "Sets the opacity of the masking lines. Set to zero to hide the lines");

    mask_opacity_ = add_float_attribute("Mask Opacity", "Opac.", 1.0f, 0.0f, 1.0f, 0.1f);
    add_hud_settings_attribute(mask_opacity_);
    mask_opacity_->set_tool_tip("Sets the opacity of the black masking overlay. Set to zero to "
                                "hids the mask completely.");

    safety_percent_ = add_float_attribute("Safety Percent", "Safety", 0.0f, 0.0f, 20.0f, 0.1f);
    add_hud_settings_attribute(safety_percent_);
    safety_percent_->set_tool_tip(
        "Sets the percentage of the image that is outside the mask area.");

    mask_label_size_ =
        add_float_attribute("Label Size", "Label Size", 16.0f, 10.0f, 40.0f, 1.0f);
    add_hud_settings_attribute(mask_label_size_);
    mask_label_size_->set_tool_tip("Sets the font size of the mask label.");


    show_mask_label_ = add_boolean_attribute("Show Mask Label", "Label", true);
    add_hud_settings_attribute(show_mask_label_);
    show_mask_label_->set_tool_tip(
        "Toggles a text label indicating the name of the current masking.");

    mask_render_method_ = add_string_choice_attribute(
        "Mask Render Method",
        "Render Method",
        "QML",
        std::vector<std::string>({"QML", "OpenGL"}));
    add_hud_settings_attribute(mask_render_method_);

    // Here we declare QML code to instantiate the actual item that draws
    // the overlay on the viewport.
    hud_element_qml(
        R"(
            import BasicViewportMask 1.0
            BasicViewportMaskOverlay {
            }
        )");

    // we can register a preference path with each of these attributes. xStudio
    // will then automatically intialised the attribute values from preference
    // file(s) and also, if the attribute is changed, the new value will be
    // written out to preference files.
    mask_aspect_ratio_->set_preference_path("/plugin/basic_masking/mask_aspect_ratio");
    line_thickness_->set_preference_path("/plugin/basic_masking/mask_line_thickness");
    line_opacity_->set_preference_path("/plugin/basic_masking/mask_line_opacity");
    mask_opacity_->set_preference_path("/plugin/basic_masking/mask_opacity");
    show_mask_label_->set_preference_path("/plugin/basic_masking/show_mask_label");
    safety_percent_->set_preference_path("/plugin/basic_masking/safety_percent");
    mask_label_size_->set_preference_path("/plugin/basic_masking/mask_label_size");
    mask_render_method_->set_preference_path("/plugin/basic_masking/mask_render_method");
}

BasicViewportMasking::~BasicViewportMasking() = default;
void BasicViewportMasking::register_hotkeys() {

    // Add "M" hotkey to activate/deactivate the mask
    mask_hotkey_ = register_hotkey(
        int('M'),
        ui::NoModifier,
        "Toggle Mask",
        "Toggles viewport masking. Find mask settings in the toolbar under the 'Mask' button",
        false,
        "Viewer");
}

utility::BlindDataObjectPtr BasicViewportMasking::onscreen_render_data(
    const media_reader::ImageBufPtr &image, const std::string & /*viewport_name*/) const {

    auto r = utility::BlindDataObjectPtr();
    if (visible() && mask_render_method_->value() == "OpenGL" && image) {

        try {

            auto image_dims = image->image_size_in_pixels();
            utility::JsonStore j;
            j["image_aspect"]      = float(image_dims.x * image->pixel_aspect() / image_dims.y);
            j["mask_aspect_ratio"] = mask_aspect_ratio_->value();
            j["mask_safety"]       = safety_percent_->value();
            j["mask_opac"]         = mask_opacity_->value();
            j["line_opac"]         = line_opacity_->value();
            j["line_thickness"]    = line_thickness_->value();
            std::array<char, 64> buf;
            std::snprintf(buf.data(), sizeof(buf), "%.2f", mask_aspect_ratio_->value());
            r.reset(new MaskData(j, buf.data(), mask_label_size_->value()));

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    } else {
        // Unvalidate existing shader, so it won't be displayed if mask is off
        r.reset();
    }

    return r;
}

void BasicViewportMasking::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/
) {

    if (attribute_uuid == aspect_ratio_presets_->uuid()) {
        if (aspect_ratio_presets_->value() != "...") {
            mask_aspect_ratio_->set_value(std::stof(aspect_ratio_presets_->value()));
        }
    } else if (attribute_uuid == mask_aspect_ratio_->uuid()) {
        // Needed conversion for float value to be x.xx so strings compare
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << mask_aspect_ratio_->value();
        std::string aspect_str = ss.str();

        if (aspect_str != aspect_ratio_presets_->value()) {
            aspect_ratio_presets_->set_value("...");
        }
    }

    redraw_viewport();
}

void BasicViewportMasking::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &, const std::string &) {
    if (hotkey_uuid == mask_hotkey_) {
        hud_data_->set_value(!hud_data_->value());
    }
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<BasicViewportMasking>>(
                utility::Uuid("4006826a-6ff2-41ec-8ef2-d7a40bfd65e4"),
                "BasicViewportMasking",
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY |
                    plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true,
                "Ted Waine",
                "Basic Viewport Masking Plugin")}));
}
}
