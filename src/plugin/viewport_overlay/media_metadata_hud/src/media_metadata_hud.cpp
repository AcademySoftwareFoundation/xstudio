// SPDX-License-Identifier: Apache-2.0
#include "media_metadata_hud.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"

#ifdef __apple__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {
const char *vertex_shader = R"(
    #version 330 core
    layout (location = 0) in vec4 aPos;
    uniform mat4 tform;
    uniform vec2 bdb_min;
    uniform vec2 bdb_max;

    void main()
    {
        vec4 rpos = aPos;
        rpos.x = bdb_min.x + (bdb_max.x - bdb_min.x)*(rpos.x + 1.0f)*0.5f;
        rpos.y = bdb_min.y + (bdb_max.y - bdb_min.y)*(rpos.y + 1.0f)*0.5f;
        gl_Position = (rpos*tform);
    }
    )";

const char *frag_shader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform float opacity;

    void main(void)
    {
        FragColor = vec4(0.0, 0.0, 0.0, opacity);
    }

    )";

inline size_t __hash_combine(size_t lhs, size_t rhs) {
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

} // namespace

void MediaMetadataRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    auto t0 = utility::clock::now();

    if (!text_renderer_) {
        init_overlay_opengl();
    }

    auto render_data =
        frame.plugin_blind_data(utility::Uuid("f3e7c2db-2578-45d6-8ad5-743779057a63"));
    const auto *data = dynamic_cast<const MediaMetadata *>(render_data.get());
    if (!data) {
        return;
    }


    // the gl viewport corresponds to the parent window size.
    // TODO: For Qt6 viewport dims will be passed into this function, we can't
    // read viewport like this
    std::array<int, 4> gl_viewport;
    glGetIntegerv(GL_VIEWPORT, gl_viewport.data());
    const auto viewport_width  = (float)gl_viewport[2];
    const auto viewport_height = (float)gl_viewport[3];

    if (display_settings_ != data->display_settings_) {

        display_settings_ = data->display_settings_;
    }
    glDisable(GL_BLEND);


    // draw BG boxes
    if (display_settings_->bg_opacity > 0.0f) {
        glBindVertexArray(vao_);
        utility::JsonStore bdb_param;
        bdb_param["opacity"] = display_settings_->bg_opacity;
        shader_->use();
        glEnableVertexAttribArray(0);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
        auto p = data->positions_.begin();
        for (const auto &bdb : data->bdbs_) {
            // 'anchor' position of text: -1.0,-1.0 is top left of viewport,
            // 1.0, 1.0 is bottom right
            Imath::V4f anchor = *p;
            p++;
            // the gl viewport is set to the entire window size, not the viewport
            // within the window. So we use 'transform_window_to_viewport_space'
            // matrx here ...
            Imath::M44f m1, m2;
            if (data->grid_layout_) {
                m1 = transform_viewport_to_image_space.inverse();
                m1.translate(Imath::V3f(anchor.x, -anchor.y / image_aspect(frame), 0.0f));
                m1 *= transform_window_to_viewport_space;
            } else {
                anchor *= transform_window_to_viewport_space;
                m2.scale(
                    Imath::V3f(viewport_width / 1920.0f, -viewport_height / 1920.0f, 1.0f));
                m2.translate(Imath::V3f(-anchor.x / anchor.w, -anchor.y / anchor.w, 0.0f));
            }
            bdb_param["bdb_min"] = bdb.min;
            bdb_param["bdb_max"] = bdb.max;
            bdb_param["tform"]   = m1 * (m2.inverse());
            shader_->set_shader_parameters(bdb_param);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glDisable(GL_BLEND);
        shader_->stop_using();
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // draw text - note, if images are in grid layout, the metadata text is anchored
    // to the image. Otherwise the text is anchored to the viewport boundary
    auto p = data->positions_.begin();
    for (const auto &v : data->verts_) {

        Imath::V4f anchor = *p;
        p++;

        Imath::M44f m1, m2;
        if (data->grid_layout_) {
            m1 = transform_viewport_to_image_space.inverse();
            m1.translate(Imath::V3f(anchor.x, -anchor.y / image_aspect(frame), 0.0f));
            m1 *= transform_window_to_viewport_space;
        } else {
            anchor *= transform_window_to_viewport_space;
            m2.scale(Imath::V3f(viewport_width / 1920.0f, -viewport_height / 1920.0f, 1.0f));
            m2.translate(Imath::V3f(-anchor.x / anchor.w, -anchor.y / anchor.w, 0.0f));
        }

        text_renderer_->render_text(
            *v,
            m1,
            m2,
            display_settings_->text_colour,
            1.0 / 1920.0,
            display_settings_->font_size,
            display_settings_->text_opacity);
    }
}

void MediaMetadataRenderer::init_overlay_opengl() {

    text_renderer_ = std::make_unique<ui::opengl::OpenGLTextRendererSDF>(
        utility::xstudio_resources_dir("fonts/VeraMono.ttf"), 96);

    // Set up the geometry used at draw time ... it couldn't be more simple,
    // it's just two triangles to make a rectangle
    glGenBuffers(1, &vbo_);
    glGenVertexArrays(1, &vao_);

    static std::array<float, 24> vertices = {
        // 1st triangle
        -1.0f,
        1.0f,
        0.0f,
        1.0f, // top left
        1.0f,
        1.0f,
        0.0f,
        1.0f, // top right
        1.0f,
        -1.0f,
        0.0f,
        1.0f, // bottom right
        // 2nd triangle
        1.0f,
        -1.0f,
        0.0f,
        1.0f, // bottom right
        -1.0f,
        1.0f,
        0.0f,
        1.0f, // top left
        -1.0f,
        -1.0f,
        0.0f,
        1.0f // bottom left
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

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
}

MediaMetadataHUD::MediaMetadataHUD(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::HUDPluginBase(cfg, "Media Metadata", init_settings, 2.0f),
      overlay_font_(utility::xstudio_resources_dir("fonts/VeraMono.ttf"), 96) {

    bg_opacity_ =
        add_float_attribute("Background Opacity", "Bg. Opac.", 1.0f, 0.0f, 1.0f, 0.1f);
    add_hud_settings_attribute(bg_opacity_);
    bg_opacity_->set_tool_tip("Sets the opacity of the background behind the overlay.");
    bg_opacity_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/bg_opacity");

    text_size_ = add_float_attribute("Text Size", "Text Size", 16.0f, 10.0f, 40.0f, 0.1f);
    add_hud_settings_attribute(text_size_);
    text_size_->set_tool_tip("Sets the text size of the metadata overlay.");
    text_size_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/text_size");

    text_opacity_ = add_float_attribute("Text Opacity", "Txt. Opac.", 1.0f, 0.0f, 1.0f, 0.1f);
    add_hud_settings_attribute(text_opacity_);
    text_opacity_->set_tool_tip("Sets the opacity of the text.");
    text_opacity_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/text_opacity");

    text_colour_ = add_colour_attribute(
        "Text Colour", "Txt. Clr.", utility::ColourTriplet(1.0f, 1.0f, 1.0f));
    add_hud_settings_attribute(text_colour_);
    text_colour_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/text_colour");

    text_spacing_ = add_float_attribute("Text Spacing", "Txt. Spg.", 1.0f, 1.0f, 3.0f, 0.1f);
    add_hud_settings_attribute(text_spacing_);
    text_spacing_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/text_spacing");

    inset_ = add_float_attribute("Caption Inset %", "Inset", 0.0f, 0.0f, 20.0f, 0.1f);
    add_hud_settings_attribute(inset_);
    inset_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/caption_inset");

    hide_missing_fields_ =
        add_boolean_attribute("Hide Field if Data is Missing", "Hide Field", false);
    add_hud_settings_attribute(hide_missing_fields_);
    hide_missing_fields_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/hide_missing_fields");

    current_profile_ = add_string_choice_attribute(
        "Current Profile", "Prfl", "Default", {"Default"}, {"Default"});
    current_profile_->expose_in_ui_attrs_group("metadata_hud_attrs", true);
    current_profile_->set_role_data(
        module::Attribute::PreferencePath,
        "/plugin/media_metadata/metadata_hud_current_profile");

    search_string_ = add_string_attribute("Search String", "Srch. strg", "");
    search_string_->expose_in_ui_attrs_group("metadata_hud_attrs", true);

    full_metadata_set_ = add_json_attribute("Full Onscreen Image Metadata");
    full_metadata_set_->expose_in_ui_attrs_group("metadata_hud_attrs", true);

    profile_data_ = add_json_attribute("Profile Fields Data");
    profile_data_->expose_in_ui_attrs_group("metadata_hud_attrs", true);

    profile_values_ = add_json_attribute("Profile Metadata Values");
    profile_values_->expose_in_ui_attrs_group("metadata_hud_attrs", true);

    profile_fields_ = add_json_attribute("Profile Fields");
    profile_fields_->expose_in_ui_attrs_group("metadata_hud_attrs", true);

    config_ = add_json_attribute("Configuration");
    config_->set_role_data(
        module::Attribute::PreferencePath, "/plugin/media_metadata/metadata_hud_config");
    config_->set_value(R"({"Default":[]})"_json);
    config_data_ = config_->value();

    config_defaults_ = add_json_attribute("Configuration Defaults");
    config_defaults_->set_role_data(
        module::Attribute::PreferencePath,
        "/plugin/media_metadata/metadata_hud_config_defaults");
    config_defaults_->set_value(R"({})"_json);

    action_attr_ = add_action_attribute("Action", "Action");
    action_attr_->expose_in_ui_attrs_group("metadata_hud_attrs", true);

    set_custom_settings_qml(
        R"(
            import MediaMetadataHUD 1.0
            MediaMetadataHUDSettings {
            }
        )");

    // call this to initialise display_settings_ member ptr
    attribute_changed(utility::Uuid(), module::Attribute::Value);
}

void MediaMetadataHUD::register_hotkeys() {

    show_viewer_hotkey_ = register_hotkey(
        int('Q'),
        ui::NoModifier,
        "Show Metadata Viewrt",
        "Shows or hides the pop-out Media Metadata Panel");
}

void MediaMetadataHUD::hotkey_pressed(
    const utility::Uuid &hotkey_uuid,
    const std::string &context,
    const std::string & /*window*/) {

    // This shortcut should be active even when no grading panel is visible
    if (hotkey_uuid == show_viewer_hotkey_ && context.find("viewport") == 0) {
        show_metadata_viewer_for_viewport(context);
    }
}

void MediaMetadataHUD::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {
    if (attribute_uuid == action_attr_->uuid()) {

        const auto action_data =
            action_attr_->get_role_data<std::vector<std::string>>(module::Attribute::Value);
        if (action_data.size() == 1) {
            if (action_data[0] == "DELETE CURRENT_PROFILE") {
                delete_current_profile();
            }
        } else if (action_data.size() == 2) {
            if (action_data[0] == "ADD PROFILE") {
                add_new_profile(action_data[1]);
            } else if (action_data[0] == "SHOWING METADATA VIEWER") {
                metadata_viewer_viewport_name_ = action_data[1];
            } else if (action_data[0] == "HIDING METADATA VIEWER") {
                metadata_viewer_viewport_name_.clear();
            } else if (action_data[0] == "TOGGLE METADATA FIELD SELECTION") {
                profile_toggle_field(action_data[1]);
            }
        } else if (action_data.size() == 3) {
            if (action_data[0] == "CHANGE FIELD LABEL") {
                set_profile_field_label(action_data[1], action_data[2]);
            } else if (action_data[0] == "CHANGE SCREEN POSITION") {
                set_field_screen_position(action_data[1], action_data[2]);
            }
        }

        // clear the attr ready for next 'action' (without notifying ourselves)
        action_attr_->set_role_data(
            module::Attribute::Value, std::vector<std::string>(), false);

    } else if (attribute_uuid == search_string_->uuid()) {

        search_filter_string_ = utility::to_lower(search_string_->value());
        update_full_metadata_set(true);

    } else if (
        (attribute_uuid == config_->uuid() || attribute_uuid == config_defaults_->uuid()) &&
        role == module::Attribute::Value) {

        update_configs_data();

    } else if (attribute_uuid == current_profile_->uuid() && role == module::Attribute::Value) {

        // config has been updated from prefs (or maybe and API call)
        if (config_data_.contains(current_profile_->value())) {
            profile_data_store_ = config_data_[current_profile_->value()];
            profile_changed();
        }

    } else {

        // update display settings
        DisplaySettings *display_settings = new DisplaySettings;
        display_settings->text_colour     = text_colour_->value();
        display_settings->font_size       = text_size_->value();
        display_settings->text_opacity    = text_opacity_->value();
        display_settings->bg_opacity      = bg_opacity_->value();
        display_settings->text_spacing    = text_spacing_->value();
        display_settings_.reset(display_settings);
        render_data_per_metadata_cache_.clear();
        render_data_per_image_cache_.clear();
    }
    redraw_viewport();
    plugin::HUDPluginBase::attribute_changed(attribute_uuid, role);
}

MediaMetadataHUD::~MediaMetadataHUD() = default;

void MediaMetadataHUD::update_configs_data() {

    // config has been updated from prefs (or maybe and API call)
    config_data_ = config_->value();

    // merge in the 'default' configs that are read-only
    auto defaults = config_defaults_->value();
    for (auto p = defaults.begin(); p != defaults.end(); ++p) {
        config_data_[p.key() + " (default)"] = p.value();
    }

    update_profiles_list();

    if (config_data_.contains(current_profile_->value())) {
        profile_data_store_ = config_data_[current_profile_->value()];
        profile_changed();
    }
}

void MediaMetadataHUD::update_profiles_list() {

    StringVec profiles;
    for (auto p = config_data_.begin(); p != config_data_.end(); ++p) {
        profiles.push_back(p.key());
    }

    std::sort(
        profiles.begin(),
        profiles.end(),
        [](const std::string &a, const std::string &b) -> bool {
            if (a.find(" (default)") != std::string::npos &&
                b.find(" (default)") == std::string::npos)
                return false;
            else if (
                a.find(" (default)") == std::string::npos &&
                b.find(" (default)") != std::string::npos)
                return true;
            return a < b;
        });

    current_profile_->set_role_data(module::Attribute::StringChoices, profiles);
}

void MediaMetadataHUD::media_due_on_screen_soon(
    const media::AVFrameIDsAndTimePoints &frame_ids) {

    // this callback is made during playback - it gives us a list of frame IDs, one for each
    // individual piece of media that is expected to be put on the screen in the immediate
    // future
    for (const auto &p : frame_ids) {
        update_media_metadata_for_media(*(p.second));
    }
}

utility::BlindDataObjectPtr MediaMetadataHUD::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string &viewport_name,
    const utility::Uuid &playhead_uuid,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    // this method is called *just* before the given image is drawn into the
    // given named viewport.
    // This method is thread-safe as far as our plugin is concerned, but it
    // blocks the viewport redraw so it must be very fast .

    // We return a pointer to our own data that has already been pre-fetched
    // elsewhere.

    auto r = utility::BlindDataObjectPtr();

    if (visible() && (images_are_in_grid_layout || is_hero_image)) {

        // do we need to capture image metadata?
        const auto key =
            to_string(image.frame_id().key()) + to_string(image.frame_id().source_uuid());

        auto p = render_data_per_image_cache_.find(key);
        if (p != render_data_per_image_cache_.end()) {
            p->second->last_used_ = utility::clock::now();
            return p->second;
        }

        return (const_cast<MediaMetadataHUD *>(this))
            ->make_onscreen_data(image, key, images_are_in_grid_layout);
    }

    return r;
}

MediaMetadataPtr MediaMetadataHUD::make_onscreen_data(
    const media_reader::ImageBufPtr &image,
    const std::string &key,
    const bool images_are_in_grid_layout) {

    try {

        static const utility::JsonStore empty_store;

        const auto p = media_metadata_.find(image.frame_id().source_uuid());
        const utility::JsonStore &media_metadata =
            p != media_metadata_.end() ? *(p->second) : empty_store;
        const utility::JsonStore image_metadata = image.metadata();
        if (p != media_metadata_.end()) {
            p->second.last_used_ = utility::clock::now();
        }


        size_t hash = __hash_combine(
            std::hash<nlohmann::json>{}(media_metadata.ref()),
            std::hash<nlohmann::json>{}(image_metadata.ref()));

        auto cache_entry = render_data_per_metadata_cache_.find(hash);
        if (cache_entry != render_data_per_metadata_cache_.end()) {
            cache_entry->second->last_used_ = utility::clock::now();
            return cache_entry->second;
        }


        const bool hide_missing_fields = hide_missing_fields_->value();
        std::map<plugin::HUDElementPosition, StringPtr> positioned_text;

        for (const auto &q : profile_essential_data_) {

            std::string v;
            if (!q.label.empty()) {
                v = q.label + ": ";
            }
            if (q.is_from_image) {
                if (image_metadata.contains(q.real_metadata_path) &&
                    !image_metadata.at(q.real_metadata_path).is_null()) {
                    if (image_metadata.at(q.real_metadata_path).is_string()) {
                        v += image_metadata.at(q.real_metadata_path).get<std::string>();
                    } else {
                        v += image_metadata.at(q.real_metadata_path).dump();
                    }
                } else if (hide_missing_fields) {
                    continue;
                } else {
                    v += "--";
                }
            } else {
                if (media_metadata.contains(q.real_metadata_path) &&
                    !media_metadata.at(q.real_metadata_path).is_null()) {
                    if (media_metadata.at(q.real_metadata_path).is_string()) {
                        v += media_metadata.at(q.real_metadata_path).get<std::string>();
                    } else {
                        v += media_metadata.at(q.real_metadata_path).dump();
                    }
                } else if (hide_missing_fields) {
                    continue;
                } else {
                    v += "--";
                }
            }

            if (positioned_text.find(q.screen_position) == positioned_text.end()) {
                positioned_text[q.screen_position].reset(new std::string());
            } else {
                *(positioned_text[q.screen_position]) += "\n";
            }
            *(positioned_text[q.screen_position]) += v;
        }

        MediaMetadataPtr result(
            new MediaMetadata(display_settings_, images_are_in_grid_layout));

        for (const auto &p : positioned_text) {

            Justification justification = JustifyLeft;
            VerticalJustification vjust = JustifyTop;

            Imath::V4f anchor;
            const float inset = inset_->value() / 100.0f;
            if (p.first == plugin::TopLeft) {
                anchor = Imath::V4f(-1.0 + inset, 1.0 - inset, 0.0, 1.0f);
            } else if (p.first == plugin::TopCenter) {
                justification = JustifyCentre;
                anchor        = Imath::V4f(0.0, 1.0 - inset, 0.0, 1.0f);
            } else if (p.first == plugin::TopRight) {
                justification = JustifyRight;
                anchor        = Imath::V4f(1.0 - inset, 1.0 - inset, 0.0, 1.0f);
            } else if (p.first == plugin::BottomLeft) {
                anchor = Imath::V4f(inset - 1.0, inset - 1.0, 0.0, 1.0f);
                vjust  = JustifyBottom;
            } else if (p.first == plugin::BottomCenter) {
                justification = JustifyCentre;
                anchor        = Imath::V4f(0.0, inset - 1.0, 0.0, 1.0f);
                vjust         = JustifyBottom;
            } else if (p.first == plugin::BottomRight) {
                justification = JustifyRight;
                anchor        = Imath::V4f(1.0 - inset, inset - 1.0, 0.0, 1.0f);
                vjust         = JustifyBottom;
            }

            Imath::Box2f bdba = overlay_font_.precompute_text_rendering_vertex_layout(
                result->make_empty_vec_for_text_render_vtxs(anchor),
                *(p.second),
                Imath::V2f(0.0f, 0.0f),
                0.0f,
                display_settings_->font_size,
                justification,
                display_settings_->text_spacing,
                true,
                vjust);

            result->add_bounding_box(bdba);
        }

        if (p != media_metadata_.end()) {
            render_data_per_image_cache_[key]     = result;
            render_data_per_metadata_cache_[hash] = result;
        }
        trim_cache();

        return result;

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return MediaMetadataPtr();
}

void MediaMetadataHUD::trim_cache() {

    while (render_data_per_image_cache_.size() > 16) {
        auto p  = render_data_per_image_cache_.begin();
        auto q  = p;
        auto tp = p->second->last_used_;
        while (p != render_data_per_image_cache_.end()) {
            if (p->second->last_used_ < tp) {
                tp = p->second->last_used_;
                q  = p;
            }
            p++;
        }
        render_data_per_image_cache_.erase(q);
    }

    while (render_data_per_metadata_cache_.size() > 16) {
        auto p  = render_data_per_metadata_cache_.begin();
        auto q  = p;
        auto tp = p->second->last_used_;
        while (p != render_data_per_metadata_cache_.end()) {
            if (p->second->last_used_ < tp) {
                tp = p->second->last_used_;
                q  = p;
            }
            p++;
        }
        render_data_per_metadata_cache_.erase(q);
    }

    while (media_metadata_.size() > 16) {
        auto p  = media_metadata_.begin();
        auto q  = p;
        auto tp = p->second.last_used_;
        while (p != media_metadata_.end()) {
            if (p->second.last_used_ < tp) {
                tp = p->second.last_used_;
                q  = p;
            }
            p++;
        }
        media_metadata_.erase(q);
    }
}


void MediaMetadataHUD::images_going_on_screen(
    const media_reader::ImageBufDisplaySetPtr &image_set,
    const std::string viewport_name,
    const bool playhead_playing) {

    // this method is called a few tens of milliseconds before the given image
    // set will actually draw to the screen. It does not block viewport redraw
    // but we want it to be fast so that any on-screen data that we are showing
    // is updated and ready.

    on_screen_images_[viewport_name] = image_set;

    if (!playhead_playing) {
        if (metadata_viewer_viewport_name_ == viewport_name) {
            update_full_metadata_set();
            update_metadata_values_for_profile();
        }
    }

    if (visible()) {
        if (image_set->has_grid_layout()) {
            // we need to gather media metadata for all images in the set
            const auto &im_order = image_set->layout_data()->image_draw_order_hint_;
            for (const auto &i : im_order) {
                const media_reader::ImageBufPtr &im = image_set->onscreen_image(i);
                update_media_metadata_for_media(im.frame_id(), false);
            }
        } else {
            // images are overlaid on each other (e.g. A/B compare mode) ... we
            // only display metadata for the 'hero' image.
            update_media_metadata_for_media(image_set->hero_image().frame_id(), false);
        }
    }
}

void MediaMetadataHUD::update_profile_metadata() {

    // The contents of the metadata_selection_ attr should be a json array,
    // each element should be an array of 2 elements. The first is the metadata
    // path/key and the second is the user display name of the metata field.

    // We need to separate these required metadata into string vectors of
    // metadata paths for each of Image, Media and Media Metadata. We also
    // need to record the ordering of the metadata items so we can re-assemble
    // the metadata from Image, Media and Media Metadata into one set for display.

    media_selected_metadata_fields_.clear();
    media_source_selected_metadata_fields_.clear();
    image_selected_metadata_fields_.clear();
    profile_essential_data_.clear();

    for (size_t i = 0; i < profile_data_store_.size(); ++i) {
        const nlohmann::json &field_data = profile_data_store_[i];


        const auto field_path = field_data["metadata_field"].get<std::string>();
        const auto field_name = field_data["metadata_field_label"].get<std::string>();
        const plugin::HUDElementPosition screen_pos =
            (plugin::HUDElementPosition)field_data["screen_position"].get<int>();

        std::string stripped_path;
        bool is_image_metadata = false;
        if (field_path.find("Pipeline Metadata/") == 0) {

            stripped_path = std::string(field_path, 17);
            media_selected_metadata_fields_.push_back(stripped_path);

        } else if (field_path.find("Media Metadata/") == 0) {

            stripped_path = std::string(field_path, 14);
            media_source_selected_metadata_fields_.push_back(stripped_path);

        } else if (field_path.find("Frame Metadata/") == 0) {

            stripped_path = std::string(field_path, 15);
            image_selected_metadata_fields_.push_back(stripped_path);
            is_image_metadata = true;
        }

        ProfileFieldItem item;
        item.is_from_image      = is_image_metadata;
        item.real_metadata_path = stripped_path;
        item.label              = field_name;
        item.screen_position    = screen_pos;
        profile_essential_data_.push_back(std::move(item));
    }

    media_metadata_.clear();
    // update our cache for on-screen media
    for (auto p : on_screen_images_) {
        if (p.second->has_grid_layout()) {
            // we need to gather media metadata for all images in the set
            const auto &im_order = p.second->layout_data()->image_draw_order_hint_;
            for (const auto &i : im_order) {
                const media_reader::ImageBufPtr &im = p.second->onscreen_image(i);
                update_media_metadata_for_media(
                    im.frame_id(), p.first == metadata_viewer_viewport_name_);
            }
        } else {
            update_media_metadata_for_media(
                p.second->hero_image().frame_id(), p.first == metadata_viewer_viewport_name_);
        }
    }
}

void MediaMetadataHUD::update_media_metadata_for_media(
    const media::AVFrameID frame_id, const bool update_profile_attr) {

    const auto media_source_actor = frame_id.media_source_actor();

    if (media_metadata_.find(media_source_actor.uuid()) != media_metadata_.end() ||
        !media_source_actor) {
        // already have up-to-date metadata for this media item
        return;
    }

    const auto media_actor = frame_id.media_actor();

    utility::JsonStore *result = new utility::JsonStore;
    mail(json_store::get_json_atom_v, media_selected_metadata_fields_)
        .request(media_actor.actor(), infinite)
        .then(
            [=](utility::JsonStore selected_media_metadata) mutable {
                result->merge(selected_media_metadata);

                mail(json_store::get_json_atom_v, media_source_selected_metadata_fields_)
                    .request(media_source_actor.actor(), infinite)
                    .then(
                        [=](utility::JsonStore selected_media_source_metadata) mutable {
                            result->merge(selected_media_source_metadata);
                            media_metadata_[media_source_actor.uuid()].reset(result);
                            if (update_profile_attr)
                                update_metadata_values_for_profile(true);
                        },
                        [=](error &err) mutable {
                            media_metadata_[media_source_actor.uuid()].reset(result);
                            if (update_profile_attr)
                                update_metadata_values_for_profile(true);
                        });
            },
            [=](error &err) mutable {
                media_metadata_[media_source_actor.uuid()].reset(result);
                if (update_profile_attr)
                    update_metadata_values_for_profile(true);
            });
}

void MediaMetadataHUD::update_metadata_values_for_profile(const bool force_update) {

    auto q = on_screen_images_.find(metadata_viewer_viewport_name_);
    if (q == on_screen_images_.end())
        return;

    const media_reader::ImageBufPtr &image = q->second->hero_image();

    if (image == previous_metadata_source_image_ && !force_update)
        return;

    nlohmann::json result = nlohmann::json::array();
    const auto p          = media_metadata_.find(image.frame_id().source_uuid());
    if (p != media_metadata_.end()) {
        p->second.last_used_ = utility::clock::now();
    }

    static const utility::JsonStore empty_store;
    const utility::JsonStore &media_metadata =
        p != media_metadata_.end() ? *(p->second) : empty_store;
    const utility::JsonStore image_metadata = image.metadata();

    for (const auto &q : profile_essential_data_) {

        if (q.is_from_image) {
            if (image_metadata.contains(q.real_metadata_path)) {
                result.push_back(image_metadata[q.real_metadata_path]);
            } else {
                result.push_back("--");
            }
        } else {
            if (media_metadata.contains(q.real_metadata_path)) {
                result.push_back(media_metadata[q.real_metadata_path]);
            } else {
                result.push_back("--");
            }
        }
    }

    profile_values_->set_value(result, false);
}

void MediaMetadataHUD::profile_changed() {

    StringVec profile_selected_fields;
    StringVec profile_selected_field_labels;
    for (size_t i = 0; i < profile_data_store_.size(); ++i) {
        const nlohmann::json &field_data = profile_data_store_[i];
        profile_selected_fields.push_back(field_data["metadata_field"].get<std::string>());
        profile_selected_field_labels.push_back(
            field_data["metadata_field_label"].get<std::string>());
    }

    profile_data_->set_value(profile_data_store_);
    profile_fields_->set_value(profile_selected_fields);

    if (current_profile_->value().find("(read only)") == std::string::npos &&
        config_data_[current_profile_->value()] != profile_data_store_) {
        config_data_[current_profile_->value()] = profile_data_store_;
        config_->set_value(config_data_, false);
    }

    update_profile_metadata();
}

void MediaMetadataHUD::connect_to_viewport(
    const std::string &viewport_name,
    const std::string &viewport_toolbar_name,
    bool connect,
    caf::actor viewport) {

    // Here we can add attrs to show up in the viewer context menu (right click)
    std::string viewport_context_menu_model_name = viewport_name + "_context_menu";
    if (viewport_menus_.find(viewport_name) == viewport_menus_.end()) {
        viewport_menus_[viewport_name] = insert_menu_item(
            viewport_context_menu_model_name,
            "Metadata Viewer",
            "",
            100.0f,
            nullptr,
            false,
            show_viewer_hotkey_,
            viewport_name);
    }
    Module::connect_to_viewport(viewport_name, viewport_toolbar_name, connect, viewport);
}

void MediaMetadataHUD::menu_item_activated(
    const utility::JsonStore &menu_item_data, const std::string &user_data) {
    utility::Uuid menu_item_id = menu_item_data.value("uuid", utility::Uuid());
    for (const auto &p : viewport_menus_) {
        if (p.second == menu_item_id) {
            show_metadata_viewer_for_viewport(p.first);
        }
    }
}

void MediaMetadataHUD::show_metadata_viewer_for_viewport(const std::string viewport_name) {
    // reveal metadata viewer for a given viewport. The viewport name is
    // 'user_data' which was passed in when we create menu items aboc
    if (metadata_viewers_.find(viewport_name) == metadata_viewers_.end()) {
        metadata_viewers_[viewport_name] = add_qml_code_attribute(
            viewport_name + " metadata viewer attr",
            R"(
            import MediaMetadataHUD 1.0
            MediaMetadataViewerWindow {
            }
            )");
        metadata_viewers_[viewport_name]->set_role_data(module::Attribute::Enabled, false);

        // this is the 'magic'! The data in this attr will be injected into a QML model
        // which then automatically executes the code above.
        metadata_viewers_[viewport_name]->expose_in_ui_attrs_group(
            viewport_name + " dynamic widgets", true);
        metadata_viewers_[viewport_name]->set_role_data(module::Attribute::Enabled, true);

    } else {
        metadata_viewers_[viewport_name]->set_role_data(
            module::Attribute::Enabled,
            !metadata_viewers_[viewport_name]->get_role_data<bool>(module::Attribute::Enabled));
    }
}


void MediaMetadataHUD::profile_toggle_field(const std::string &field) {

    for (auto p = profile_data_store_.begin(); p != profile_data_store_.end(); ++p) {
        if (p.value()["metadata_field"].get<std::string>() == field) {
            profile_data_store_.erase(p);
            profile_changed();
            return;
        }
    }

    nlohmann::json field_data;
    field_data["metadata_field"]  = field;
    field_data["screen_position"] = plugin::TopLeft;
    if (field.rfind("/") != std::string::npos) {
        field_data["metadata_field_label"] = std::string(field, field.rfind("/") + 1);
    } else {
        field_data["metadata_field_label"] = field;
    }
    profile_data_store_.push_back(field_data);

    profile_changed();
}

void MediaMetadataHUD::set_profile_field_label(
    const std::string &field, const std::string &field_label) {
    for (auto p = profile_data_store_.begin(); p != profile_data_store_.end(); ++p) {
        if (p.value()["metadata_field"].get<std::string>() == field) {
            p.value()["metadata_field_label"] = field_label;
            break;
        }
    }
    profile_changed();
}

void MediaMetadataHUD::set_field_screen_position(
    const std::string &field, const std::string &screen_position) {

    static const std::map<std::string, plugin::HUDElementPosition> positions(
        {{"BottomLeft", plugin::BottomLeft},
         {"BottomCenter", plugin::BottomCenter},
         {"BottomRight", plugin::BottomRight},
         {"TopLeft", plugin::TopLeft},
         {"TopCenter", plugin::TopCenter},
         {"TopRight", plugin::TopRight},
         {"FullScreen", plugin::FullScreen}});


    auto q = positions.find(screen_position);
    if (q != positions.end()) {
        for (auto p = profile_data_store_.begin(); p != profile_data_store_.end(); ++p) {
            if (p.value()["metadata_field"].get<std::string>() == field) {
                p.value()["screen_position"] = q->second;
                break;
            }
        }
    }
    profile_changed();
}


void MediaMetadataHUD::add_new_profile(const std::string &name) {

    if (!config_data_.contains(name)) {
        config_data_[name]  = nlohmann::json::array();
        profile_data_store_ = nlohmann::json::array();
        config_->set_value(config_data_, false);

        update_profiles_list();

        current_profile_->set_value(name);
        profile_changed();
    }
}

void MediaMetadataHUD::delete_current_profile() {

    if (config_data_.contains(current_profile_->value()) && config_data_.size() > 1) {

        config_data_.erase(config_data_.find(current_profile_->value()));
        config_->set_value(config_data_, false);
        update_profiles_list();
        if (config_data_.size())
            current_profile_->set_value(config_data_.begin().key());
        profile_changed();
    }
}


void MediaMetadataHUD::update_full_metadata_set(const bool full_rebuild) {

    // lambda to get metadata from a given actor and then run new_metadata
    // with the actor type
    auto get_metadata_from_actor = [=](caf::actor a, const std::string who) {
        mail(json_store::get_json_atom_v, std::string("/metadata"))
            .request(a, infinite)
            .then(
                [=](const utility::JsonStore &source_metadata) mutable {
                    new_metadata(who, source_metadata, full_rebuild);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    new_metadata(who);
                });
    };

    auto p = on_screen_images_.find(metadata_viewer_viewport_name_);
    if (p == on_screen_images_.end() || !p->second) {
        // set metadata to nothing
        full_metadata_set_->set_value(nlohmann::json());
        return;
    }
    const media_reader::ImageBufPtr &image = p->second->hero_image();

    if (image.frame_id().key() != metadata_frame_key_ || full_rebuild) {

        metadata_frame_key_ = image.frame_id().key();
        new_metadata("Frame Metadata", image.metadata(), full_rebuild);
    }

    if (metadata_media_id_ != image.frame_id().source_uuid() || full_rebuild) {

        metadata_media_id_ = image.frame_id().source_uuid();
        auto media_source_actor =
            caf::actor_cast<caf::actor>(image.frame_id().media_source_addr());
        if (media_source_actor) {
            get_metadata_from_actor(media_source_actor, "Media Metadata");
        } else {
            new_metadata("Media Metadata");
        }
        auto media_actor = caf::actor_cast<caf::actor>(image.frame_id().media_addr());
        if (media_actor) {
            get_metadata_from_actor(media_actor, "Pipeline Metadata");
        } else {
            new_metadata("Pipeline Metadata");
        }
    }
}

void flatten(
    nlohmann::json &result,
    const nlohmann::json &obj,
    const std::string key,
    const std::string real_path,
    const std::string &search_filter_string) {
    if (obj.is_null())
        return;
    // special case
    if (obj.is_object() && !obj.is_array()) {
        for (auto p = obj.begin(); p != obj.end(); ++p) {
            if (p.key() == "streams" && p.value().is_array()) {
                for (int i = 0; i < p.value().size(); ++i) {
                    flatten(
                        result,
                        p.value()[i],
                        key + fmt::format("{}.{}", p.key(), i) + ".",
                        real_path + "/streams/" + fmt::format("{}", i),
                        search_filter_string);
                }
            } else {
                flatten(
                    result,
                    p.value(),
                    key + p.key() + ".",
                    real_path + "/" + p.key(),
                    search_filter_string);
            }
        }
    } else if (!key.empty()) {

        std::string vstring;
        if (obj.is_string()) {
            vstring = obj.get<std::string>();
        } else {
            vstring = obj.dump(2);
        }

        if (!search_filter_string.empty() &&
            utility::to_lower(vstring).find(search_filter_string) == std::string::npos &&
            utility::to_lower(real_path).find(search_filter_string) == std::string::npos)
            return;

        result[std::string(key, 0, key.size() - 1)]["value"] = vstring;
        result[std::string(key, 0, key.size() - 1)]["path"]  = real_path;
    }
}

void MediaMetadataHUD::new_metadata(
    const std::string metadata_owner, const utility::JsonStore &metadata, const bool force) {

    // This merges new metadata coming from 'metadata_owner' (which might be Image, Media, Media
    // Metadata or Layout) into the full set, and then updates the attr holding the full set to
    // drive the metadata viewer UI window
    if (!metadata_set_[metadata_owner] || *metadata_set_[metadata_owner] != metadata || force) {

        metadata_set_[metadata_owner].reset(new utility::JsonStore(metadata));
        utility::JsonStore j;
        for (const auto &p : metadata_set_) {

            std::string path = p.first;

            if (path == "Layout") {
                j[p.first] = p.second->ref();
                continue;
            }

            if (path != "Frame Metadata") {
                path += "/metadata";
            }
            try {

                nlohmann::json jj;
                flatten(jj, p.second->ref(), "", path, search_filter_string_);

                j[p.first] = jj;
            } catch (std::exception &e) {
                spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }
        full_metadata_set_->set_value(j);
    }
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<MediaMetadataHUD>>(
                utility::Uuid("f3e7c2db-2578-45d6-8ad5-743779057a63"),
                "MediaMetadataHUD",
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY |
                    plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true,
                "Ted Waine",
                "Plugin to display per-frame metadata (e.g. EXR header data)")}));
}
}
