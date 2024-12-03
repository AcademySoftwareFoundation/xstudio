// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/managed_dir.hpp"

#include "query_engine.hpp"
#include "worker.hpp"

namespace xstudio {
namespace shotbrowser {

    class ShotBrowser : public plugin::StandardPlugin {
      public:
        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("8c0f06a8-cf43-44f3-94e2-0428bf7d150c");

        ShotBrowser(caf::actor_config &cfg, const utility::JsonStore &init_settings);
        virtual ~ShotBrowser() = default;

        QueryEngine &engine() { return engine_; }

        void update_preferences(const utility::JsonStore &js);
        void on_exit() override;

      protected:
        caf::message_handler message_handler_extensions() override;

        // void attribute_changed(
        //     const utility::Uuid &attribute_uuid, const int /*role*/
        //     ) override;

        // void register_hotkeys() override;
        // void hotkey_pressed(const utility::Uuid &uuid, const std::string &context) override;
        // void hotkey_released(const utility::Uuid &uuid, const std::string &context) override;

        // void images_going_on_screen(
        //     const std::vector<media_reader::ImageBufPtr> &images,
        //     const std::string viewport_name,
        //     const bool playhead_playing) override;

        // void menu_item_activated(const utility::Uuid &menu_item_uuid) override;

        shotgun_client::AuthenticateShotgun get_authentication() const;

        void set_authentication_method(const std::string &value);
        void set_client_id(const std::string &value);
        void set_client_secret(const std::string &value);
        void set_username(const std::string &value);
        void set_password(const std::string &value);
        void set_session_token(const std::string &value);
        void set_authenticated(const bool value);
        void set_timeout(const int value);

        void bind_attribute_changed_callback(
            std::function<void(const utility::Uuid &attr_uuid)> fn) {
            attribute_changed_callback_ = [fn](auto &&PH1) {
                return fn(std::forward<decltype(PH1)>(PH1));
            };
        }

        void attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) override;

        void call_attribute_changed(const utility::Uuid &attr_uuid) {
            if (attribute_changed_callback_)
                attribute_changed_callback_(attr_uuid);
        }

      private:
        void add_attributes();
        void attribute_changed(const utility::Uuid &attr_uuid);

        void update_playlist_versions(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::Uuid &playlist_uuid,
            const int playlist_id            = 0,
            const utility::Uuid &notify_uuid = utility::Uuid());
        void refresh_playlist_versions(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::Uuid &playlist_uuid,
            const bool match_order = false);
        // void refresh_playlist_notes(caf::typed_response_promise<utility::JsonStore> rp, const
        // utility::Uuid &playlist_uuid);
        void create_playlist(
            caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &js);

        void create_tag(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &value);

        void rename_tag(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int tag_id,
            const std::string &value);

        void add_entity_tag(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &entity,
            const int entity_id,
            const int tag_id);

        void remove_entity_tag(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &entity,
            const int entity_id,
            const int tag_id);

        void prepare_playlist_notes(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::Uuid &playlist_uuid,
            const utility::UuidVector &media_uuids  = {},
            const bool notify_owner                 = false,
            const std::vector<int> notify_group_ids = {},
            const bool combine                      = false,
            const bool add_time                     = false,
            const bool add_playlist_name            = false,
            const bool add_type                     = false,
            const bool anno_requires_note           = true,
            const bool skip_already_pubished        = false,
            const std::string &default_type         = "");

        void create_playlist_notes(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::JsonStore &notes,
            const utility::Uuid &playlist_uuid);
        void load_playlist(
            caf::typed_response_promise<utility::UuidActor> rp,
            const int playlist_id,
            const caf::actor &session);

        // void update_playlist_notes(caf::typed_response_promise<utility::JsonStore> rp, const
        // utility::Uuid &playlist_uuid, const utility::JsonStore &js); void
        // add_media_to_playlist(caf::typed_response_promise<std::vector<utility::UuidActor>>
        // rp,
        //     const utility::JsonStore &data,
        //     const utility::Uuid &playlist_uuid,
        //     const utility::Uuid &before
        // );

        void reorder_playlist(
            caf::typed_response_promise<bool> rp,
            const caf::actor &playlist,
            const utility::JsonStore &sg_playlist);

        void add_media_to_playlist(
            caf::typed_response_promise<std::vector<utility::UuidActor>> rp,
            const utility::JsonStore &data,
            const bool create_playlist,
            utility::Uuid playlist_uuid,
            caf::actor playlist,
            const utility::Uuid &before,
            const utility::FrameRate &media_rate);
        void get_valid_media_count(
            caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid);
        void link_media(
            caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid);

        void download_media(
            caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid);

        void get_data(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id);

        void
        get_precache(caf::typed_response_promise<utility::JsonStore> rp, const int project_id);

        void get_data_department(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_project(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_location(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_review_location(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_playlist_type(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_shot_status(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_note_type(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_production_status(
            caf::typed_response_promise<utility::JsonStore> rp, const std::string &type);

        void get_data_pipeline_status(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const std::string &entity,
            const std::string &cache_name);

        void get_version_artist(
            caf::typed_response_promise<utility::JsonStore> rp, const int version_id);

        void get_pipe_step(caf::typed_response_promise<utility::JsonStore> rp);

        void get_data_unit(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id);

        void get_data_stage(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id);

        void get_data_group(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id);

        void get_data_user(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id                = -1,
            const int page                      = 1,
            const utility::JsonStore &prev_data = utility::JsonStore(R"([])"_json));

        void get_data_shot(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id,
            const int page                      = 1,
            const utility::JsonStore &prev_data = utility::JsonStore(R"([])"_json));

        void get_data_shot_for_sequence(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id,
            const int page                      = 1,
            const utility::JsonStore &prev_data = utility::JsonStore(R"([])"_json));

        void get_data_sequence(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id,
            const int page                      = 1,
            const utility::JsonStore &prev_data = utility::JsonStore(R"([])"_json));

        void get_data_playlist(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &type,
            const int project_id,
            const int page                      = 1,
            const utility::JsonStore &prev_data = utility::JsonStore(R"([])"_json));

        void find_ivy_version(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::string &uuid,
            const std::string &job);
        void find_shot(caf::typed_response_promise<utility::JsonStore> rp, const int shot_id);

        void get_fields(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int id,
            const std::string &entity,
            const std::vector<std::string> &fields);

        std::shared_ptr<BuildPlaylistMediaJob> get_next_build_task(bool &is_ivy_build_task);
        void do_add_media_sources_from_shotgun(std::shared_ptr<BuildPlaylistMediaJob>);
        void do_add_media_sources_from_ivy(std::shared_ptr<BuildPlaylistMediaJob>);

        void publish_note_annotations(
            caf::typed_response_promise<utility::JsonStore> rp,
            const caf::actor &session,
            const int note_id,
            const utility::JsonStore &annotations);

        void execute_query(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::JsonStore &action);

        void put_action(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::JsonStore &action);

        void use_action(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::JsonStore &action);

        void use_action(
            caf::typed_response_promise<utility::UuidActor> rp,
            const utility::JsonStore &action,
            const caf::actor &session);

        void use_action(
            caf::typed_response_promise<utility::UuidActorVector> rp,
            const caf::uri &uri,
            const utility::FrameRate &media_rate,
            const bool create_playlist);

        void get_action(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::JsonStore &action);

        void post_action(
            caf::typed_response_promise<utility::JsonStore> rp,
            const utility::JsonStore &action);

        void execute_preset(
            caf::typed_response_promise<utility::JsonStore> rp,
            const std::vector<std::string> &preset_paths,
            const int project_id,
            const utility::JsonStore &context,
            const utility::JsonStore &metadata,
            const utility::JsonStore &env,
            const utility::JsonStore &custom_terms);

        std::vector<std::string>
        extend_fields(const std::string &entity, const std::vector<std::string> &fields) const;


      private:
        module::StringChoiceAttribute *authentication_method_;
        module::StringAttribute *client_id_;
        module::StringAttribute *client_secret_;
        module::StringAttribute *username_;
        module::StringAttribute *password_;
        module::StringAttribute *session_token_;
        module::BooleanAttribute *authenticated_;
        module::FloatAttribute *timeout_;

        bool disable_integration_{false};

        // module::ActionAttribute *playlist_notes_action_;
        // module::ActionAttribute *selected_notes_action_;

        // void setup_menus();

        // Example attributes .. you might not need to use these
        // module::StringChoiceAttribute *some_multichoice_attribute_{nullptr};
        // module::IntegerAttribute *some_integer_attribute_{nullptr};
        // module::BooleanAttribute *some_bool_attribute_{nullptr};
        // module::ColourAttribute *some_colour_attribute_{nullptr};

        // utility::Uuid demo_hotkey_;
        // utility::Uuid my_menu_item_;

        utility::Uuid session_id_;
        std::function<void(const utility::Uuid &attr_uuid)> attribute_changed_callback_;

        QueryEngine engine_;

        caf::actor shotgun_;
        caf::actor pool_;

        utility::Uuid history_uuid_;
        caf::actor history_;

        caf::actor event_group_;

        size_t changed_hash_{0};
        caf::actor_addr secret_source_;
        std::vector<caf::typed_response_promise<shotgun_client::AuthenticateShotgun>> waiting_;
        utility::Uuid uuid_ = {utility::Uuid::generate()};
        std::map<std::string, std::string> category_colours_;

        std::deque<std::shared_ptr<BuildPlaylistMediaJob>> build_playlist_media_tasks_;
        std::deque<std::shared_ptr<BuildPlaylistMediaJob>> extend_media_with_ivy_tasks_;
        int build_tasks_in_flight_ = {0};
        int worker_count_          = {8};

        std::map<long, utility::JsonStore> shot_cache_;

        utility::ManagedDir download_cache_;

        bool pending_preference_update_ = {false};

        std::vector<std::string> version_fields_;
        std::vector<std::string> shot_fields_;
        std::vector<std::string> note_fields_;
        std::vector<std::string> playlist_fields_;
    };

} // namespace shotbrowser
} // namespace xstudio