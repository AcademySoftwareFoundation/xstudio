// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/data_source/data_source.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/managed_dir.hpp"
#include "xstudio/module/module.hpp"

using namespace xstudio;
using namespace xstudio::data_source;

const auto UpdatePlaylistJSON =
    R"({"entity":"Playlist", "relationship": "Version", "playlist_uuid": null})"_json;
const auto CreatePlaylistJSON =
    R"({"entity":"Playlist", "playlist_uuid": null, "project_id": null, "code": null, "location": null, "playlist_type": "Dailies"})"_json;
const auto LoadPlaylistJSON = R"({"entity":"Playlist", "playlist_id": 0})"_json;
const auto GetPlaylistValidMediaJSON =
    R"({"playlist_uuid": null, "operation": "MediaCount"})"_json;
const auto GetPlaylistLinkMediaJSON =
    R"({"playlist_uuid": null, "operation": "LinkMedia"})"_json;

const auto DownloadMediaJSON = R"({"media_uuid": null, "operation": "DownloadMedia"})"_json;

const auto GetVersionIvyUuidJSON =
    R"({"job":null, "ivy_uuid": null, "operation": "VersionFromIvy"})"_json;
const auto GetShotFromIdJSON = R"({"shot_id": null, "operation": "GetShotFromId"})"_json;
const auto RefreshPlaylistJSON =
    R"({"entity":"Playlist", "relationship": "Version", "playlist_uuid": null})"_json;
const auto RefreshPlaylistNotesJSON =
    R"({"entity":"Playlist", "relationship": "Note", "playlist_uuid": null})"_json;
const auto PublishNoteTemplateJSON = R"(
{
    "bookmark_uuid": "",
    "shot": "",
    "payload": {
            "project":{ "type": "Project", "id":0 },
            "note_links": [
                { "type": "Playlist", "id":0 },
                { "type": "Sequence", "id":0 },
                { "type": "Shot", "id":0 },
                { "type": "Version", "id":0 }
            ],

            "addressings_to": [
                { "type": "HumanUser", "id": 0}
            ],

            "addressings_cc": [
            ],

            "sg_note_type": null,
            "sg_status_list":"opn",
            "subject": null,
            "content": null
    }
}
)"_json;

const auto PreparePlaylistNotesJSON = R"({
    "operation":"PrepareNotes",
    "playlist_uuid": null,
    "media_uuids": [],
    "notify_owner": false,
    "notify_group_ids": [],
    "combine": false,
    "add_time": false,
    "add_playlist_name": false,
    "add_type": false,
    "anno_requires_note": true,
    "skip_already_published": false,
    "default_type": null
})"_json;
const auto CreatePlaylistNotesJSON =
    R"({"entity":"Note", "playlist_uuid": null, "payload": []})"_json;

const auto VersionFields = std::vector<std::string>(
    {"id",
     "created_by",
     "sg_pipeline_step",
     "sg_path_to_frames",
     "sg_dneg_version",
     "sg_twig_name",
     "sg_on_disk_mum",
     "sg_on_disk_mtl",
     "sg_on_disk_van",
     "sg_on_disk_chn",
     "sg_on_disk_lon",
     "sg_on_disk_syd",
     "sg_production_status",
     "sg_status_list",
     "sg_date_submitted_to_client",
     "sg_ivy_dnuuid",
     "frame_range",
     "code",
     "sg_path_to_movie",
     "frame_count",
     "entity",
     "project",
     "created_at",
     "notes",
     "sg_twig_type_code",
     "user",
     "sg_cut_range",
     "sg_comp_range",
     "sg_project_name",
     "sg_twig_type",
     "sg_cut_order",
     "cut_order",
     "sg_cut_in",
     "sg_comp_in",
     "sg_cut_out",
     "sg_comp_out",
     "sg_frames_have_slate",
     "sg_movie_has_slate",
     "sg_submit_dailies",
     "sg_submit_dailies_chn",
     "sg_submit_dailies_mtl",
     "sg_submit_dailies_van",
     "sg_submit_dailies_mum",
     "image"});

const auto ShotFields =
    std::vector<std::string>({"id", "code", "sg_comp_range", "sg_cut_range", "project"});

const std::string shotgun_datasource_registry{"SHOTGUNDATASOURCE"};

const auto ShotgunMetadataPath = std::string("/metadata/shotgun");

namespace xstudio::shotgun_client {
class AuthenticateShotgun;
}

class ShotgunDataSource : public DataSource, public module::Module {
  public:
    ShotgunDataSource() : DataSource("Shotgun"), module::Module("ShotgunDataSource") {
        add_attributes();
    }
    ~ShotgunDataSource() override = default;

    // handled directly in actor.
    utility::JsonStore get_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }
    utility::JsonStore put_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }
    utility::JsonStore post_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }
    utility::JsonStore use_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }

    void set_authentication_method(const std::string &value);
    void set_client_id(const std::string &value);
    void set_client_secret(const std::string &value);
    void set_username(const std::string &value);
    void set_password(const std::string &value);
    void set_session_token(const std::string &value);
    void set_authenticated(const bool value);
    void set_timeout(const int value);

    utility::Uuid session_id_;

    module::StringChoiceAttribute *authentication_method_;
    module::StringAttribute *client_id_;
    module::StringAttribute *client_secret_;
    module::StringAttribute *username_;
    module::StringAttribute *password_;
    module::StringAttribute *session_token_;
    module::BooleanAttribute *authenticated_;
    module::FloatAttribute *timeout_;

    module::ActionAttribute *playlist_notes_action_;
    module::ActionAttribute *selected_notes_action_;

    shotgun_client::AuthenticateShotgun get_authentication() const;

    void
    bind_attribute_changed_callback(std::function<void(const utility::Uuid &attr_uuid)> fn) {
        attribute_changed_callback_ = [fn](auto &&PH1) {
            return fn(std::forward<decltype(PH1)>(PH1));
        };
    }
    using module::Module::connect_to_ui;

  protected:
    // void hotkey_pressed(const utility::Uuid &hotkey_uuid, const std::string &context)
    // override;

    void attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) override;


    void call_attribute_changed(const utility::Uuid &attr_uuid) {
        if (attribute_changed_callback_)
            attribute_changed_callback_(attr_uuid);
    }


  private:
    std::function<void(const utility::Uuid &attr_uuid)> attribute_changed_callback_;

    void add_attributes();
};

class BuildPlaylistMediaJob;

template <typename T> class ShotgunDataSourceActor : public caf::event_based_actor {
  public:
    ShotgunDataSourceActor(
        caf::actor_config &cfg, const utility::JsonStore & = utility::JsonStore());

    caf::behavior make_behavior() override {
        return data_source_.message_handler().or_else(behavior_);
    }
    void update_preferences(const utility::JsonStore &js);
    void on_exit() override;

  private:
    void attribute_changed(const utility::Uuid &attr_uuid);
    void update_playlist_versions(
        caf::typed_response_promise<utility::JsonStore> rp,
        const utility::Uuid &playlist_uuid,
        const int playlist_id = 0);
    void refresh_playlist_versions(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &playlist_uuid);
    // void refresh_playlist_notes(caf::typed_response_promise<utility::JsonStore> rp, const
    // utility::Uuid &playlist_uuid);
    void create_playlist(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &js);

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
    // add_media_to_playlist(caf::typed_response_promise<std::vector<utility::UuidActor>> rp,
    //     const utility::JsonStore &data,
    //     const utility::Uuid &playlist_uuid,
    //     const utility::Uuid &before
    // );
    void add_media_to_playlist(
        caf::typed_response_promise<std::vector<utility::UuidActor>> rp,
        const utility::JsonStore &data,
        utility::Uuid playlist_uuid,
        caf::actor playlist,
        const utility::Uuid &before);
    void get_valid_media_count(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid);
    void
    link_media(caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid);

    void download_media(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid);

    void find_ivy_version(
        caf::typed_response_promise<utility::JsonStore> rp,
        const std::string &uuid,
        const std::string &job);
    void find_shot(caf::typed_response_promise<utility::JsonStore> rp, const int shot_id);

    std::shared_ptr<BuildPlaylistMediaJob> get_next_build_task(bool &is_ivy_build_task);
    void do_add_media_sources_from_shotgun(std::shared_ptr<BuildPlaylistMediaJob>);
    void do_add_media_sources_from_ivy(std::shared_ptr<BuildPlaylistMediaJob>);

  private:
    caf::behavior behavior_;
    T data_source_;
    caf::actor shotgun_;
    caf::actor pool_;
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
};
