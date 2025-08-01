// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/data_source/data_source.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/managed_dir.hpp"
#include "xstudio/module/module.hpp"

#include "data_source_shotgun_base.hpp"

using namespace xstudio;
using namespace xstudio::data_source;

namespace xstudio::shotgun_client {
class AuthenticateShotgun;
}

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

    void
    create_tag(caf::typed_response_promise<utility::JsonStore> rp, const std::string &value);

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

    void execute_query(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &action);

    void put_action(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &action);

    void use_action(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &action);

    void use_action(
        caf::typed_response_promise<utility::UuidActor> rp,
        const utility::JsonStore &action,
        const caf::actor &session);

    void use_action(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const caf::uri &uri,
        const utility::FrameRate &media_rate);

    void get_action(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &action);

    void post_action(
        caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &action);


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

    bool disable_integration_ {false};

    std::map<long, utility::JsonStore> shot_cache_;

    utility::ManagedDir download_cache_;
};
