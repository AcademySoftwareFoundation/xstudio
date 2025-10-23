// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "xstudio/plugin_manager/hud_plugin.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        struct DisplaySettings {
            utility::ColourTriplet text_colour;
            float font_size;
            float text_opacity;
            float text_spacing;
            float bg_opacity;
        };
        typedef std::shared_ptr<const DisplaySettings> DisplaySettingsPtr;

        typedef std::vector<std::string> StringVec;
        typedef std::shared_ptr<std::string> StringPtr;

        class JsonStorePtr : public std::shared_ptr<const utility::JsonStore> {
            using Base = std::shared_ptr<const utility::JsonStore>;

          public:
            JsonStorePtr() = default;

            JsonStorePtr(const JsonStorePtr &o)
                : Base(static_cast<const Base &>(o)), last_used_(o.last_used_) {}

            JsonStorePtr &operator=(const JsonStorePtr &o) {
                Base &b    = static_cast<Base &>(*this);
                b          = static_cast<const Base &>(o);
                last_used_ = o.last_used_;
                return *this;
            }

            void reset(utility::JsonStore *p) {
                Base &b = static_cast<Base &>(*this);
                b.reset(p);
                last_used_ = utility::clock::now();
            }

            utility::time_point last_used_;
        };

        /* To carry data from xstudio backend to the UI draw routine, we need
        to make a custom class based on BlindDataObject. This allows us to
        set-up all the data we need to draw to the screen in the backen thread
        ready for the UI thread. Some careful consideration is made to optimise
        this machanism so as not to slow down playback loops and draw
        routines which are crucial to performance etc.
        */
        class MediaMetadata : public utility::BlindDataObject {
          public:
            MediaMetadata(const DisplaySettingsPtr &d, const bool grid_layout)
                : display_settings_(d),
                  last_used_(utility::clock::now()),
                  grid_layout_(grid_layout) {}

            ~MediaMetadata() = default;

            const DisplaySettingsPtr display_settings_;

            std::vector<float> &make_empty_vec_for_text_render_vtxs(Imath::V4f &pos) {
                positions_.push_back(pos);
                auto p = std::make_shared<std::vector<float>>();
                verts_.push_back(p);
                return *p;
            }

            void add_bounding_box(const Imath::Box2f &bdb) { bdbs_.push_back(bdb); }

            std::vector<std::shared_ptr<std::vector<float>>> verts_;
            std::vector<Imath::V4f> positions_;
            std::vector<Imath::Box2f> bdbs_;
            utility::time_point last_used_;
            const bool grid_layout_;
        };

        typedef std::shared_ptr<MediaMetadata> MediaMetadataPtr;

        /* We defined a separate class to take care of rendering graphics into
        the xstudio viewport. Be aware the instance(s) of this class runs in
        a separate thread to the main plugin class instance, don't share
        data directly. Rather, we use our MediaMetadata class to pass data from
        the plugin class to the renderer.
        */
        class MediaMetadataRenderer : public plugin::ViewportOverlayRenderer {

          public:
            void render_image_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const float device_pixel_ratio,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            void init_overlay_opengl();

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
            GLuint vbo_;
            GLuint vao_;
            std::unique_ptr<xstudio::ui::opengl::OpenGLTextRendererSDF> text_renderer_;
            DisplaySettingsPtr display_settings_;
        };

        class MediaMetadataHUD : public plugin::HUDPluginBase {
          public:
            MediaMetadataHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~MediaMetadataHUD();

          protected:
            utility::BlindDataObjectPtr onscreen_render_data(
                const media_reader::ImageBufPtr &,
                const std::string & /*viewport_name*/,
                const utility::Uuid &playhead_uuid,
                const bool is_hero_image,
                const bool images_are_in_grid_layout) const override;

            void images_going_on_screen(
                const media_reader::ImageBufDisplaySetPtr &image_set,
                const std::string viewport_name,
                const bool playhead_playing) override;

            plugin::ViewportOverlayRendererPtr
            make_overlay_renderer(const std::string &viewport_name) override {
                return plugin::ViewportOverlayRendererPtr(new MediaMetadataRenderer());
            }

            void
            attribute_changed(const utility::Uuid &attribute_uuid, const int role) override;

            void register_hotkeys() override;

            void hotkey_pressed(
                const utility::Uuid &hotkey_uuid,
                const std::string &context,
                const std::string &window) override;

            void media_due_on_screen_soon(const media::AVFrameIDsAndTimePoints &) override;

            void connect_to_viewport(
                const std::string &viewport_name,
                const std::string &viewport_toolbar_name,
                bool connect,
                caf::actor viewport) override;

            void menu_item_activated(
                const utility::JsonStore &menu_item_data,
                const std::string &user_data) override;

          private:
            void update_configs_data();
            void update_profiles_list();
            void show_metadata_viewer_for_viewport(const std::string viewport_name);
            void update_profile_metadata();
            void update_full_metadata_set(const bool full_rebuild = false);
            void new_metadata(
                const std::string who,
                const utility::JsonStore &metadata = utility::JsonStore(),
                const bool force                   = false);
            void update_media_metadata_for_media(
                const media::AVFrameID frame_id, const bool update_profile_attr = false);

            void update_metadata_values_for_profile(const bool force_update = false);
            void trim_cache();

            MediaMetadataPtr make_onscreen_data(
                const media_reader::ImageBufPtr &image,
                const std::string &key,
                const bool images_are_in_grid_layout);

            void profile_changed();
            void profile_toggle_field(const std::string &field);
            void
            set_profile_field_label(const std::string &field, const std::string &field_label);
            void set_field_screen_position(
                const std::string &field, const std::string &screen_position);
            void add_new_profile(const std::string &name);
            void delete_current_profile();

            module::FloatAttribute *text_size_;
            module::FloatAttribute *text_opacity_;
            module::FloatAttribute *text_spacing_;
            module::FloatAttribute *bg_opacity_;
            module::FloatAttribute *inset_;
            module::ColourAttribute *text_colour_;
            module::StringAttribute *search_string_;
            module::StringChoiceAttribute *current_profile_;
            module::BooleanAttribute *hide_missing_fields_;

            module::JsonAttribute *full_metadata_set_;
            module::JsonAttribute *profile_data_;
            module::JsonAttribute *profile_values_;
            module::JsonAttribute *config_;
            module::JsonAttribute *config_defaults_;
            module::JsonAttribute *profile_fields_;

            utility::JsonStore config_data_;
            utility::JsonStore profile_data_store_;

            module::ActionAttribute *action_attr_;

            std::string metadata_viewer_viewport_name_;
            std::string search_filter_string_;
            media::MediaKey metadata_frame_key_;
            utility::Uuid metadata_media_id_;

            StringVec media_selected_metadata_fields_;
            StringVec media_source_selected_metadata_fields_;
            StringVec image_selected_metadata_fields_;

            // Using this because it should be faster to iterate over than
            // a JsonStore
            struct ProfileFieldItem {
                bool is_from_image;
                std::string real_metadata_path;
                std::string label;
                plugin::HUDElementPosition screen_position;
            };
            std::vector<ProfileFieldItem> profile_essential_data_;

            StringPtr field_names_;

            std::map<utility::Uuid, JsonStorePtr> media_metadata_;

            std::map<std::string, MediaMetadataPtr> render_data_per_image_cache_;
            std::map<size_t, MediaMetadataPtr> render_data_per_metadata_cache_;

            std::map<std::string, media_reader::ImageBufDisplaySetPtr> on_screen_images_;
            media_reader::ImageBufPtr previous_metadata_source_image_;
            std::map<std::string, JsonStorePtr> metadata_set_;
            DisplaySettingsPtr display_settings_;

            SDFBitmapFont overlay_font_;

            // menu management
            utility::Uuid show_viewer_hotkey_;
            std::map<std::string, utility::Uuid> viewport_menus_;
            std::map<std::string, module::QmlCodeAttribute *> metadata_viewers_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
