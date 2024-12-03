// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>

#include <caf/all.hpp>

#include "xstudio/session/session.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace session {

    class SessionActor : public caf::event_based_actor {
      public:
        SessionActor(
            caf::actor_config &cfg,
            const utility::JsonStore &jsn,
            const caf::uri &path = caf::uri());
        SessionActor(caf::actor_config &cfg, const std::string &name);
        ~SessionActor() override;
        // ~SessionActor() override {spdlog::warn("~SessionActor");}
        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "SessionActor";

        void init();
        caf::message_handler message_handler();

        caf::behavior make_behavior() override { return behavior_; }
        void create_container(
            caf::actor actor,
            caf::typed_response_promise<std::pair<utility::Uuid, utility::UuidActor>> rp,
            const utility::Uuid &uuid_before,
            const bool into);

        void duplicate_container(
            caf::typed_response_promise<utility::UuidVector> rp,
            const utility::UuidTree<utility::PlaylistItem> &tree,
            caf::actor source_session,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false,
            const bool rename                = true,
            const bool kill_source           = false);

        void duplicate_tree(
            utility::UuidTree<utility::PlaylistItem> &tree,
            caf::actor source_session,
            const bool rename = false);

        void create_playlist(
            caf::typed_response_promise<std::pair<utility::Uuid, utility::UuidActor>> &rp,
            const std::string name,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);

        void insert_playlist(
            caf::typed_response_promise<std::pair<utility::Uuid, utility::UuidActor>> &rp,
            caf::actor actor,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);

        void copy_containers_to(
            caf::typed_response_promise<utility::UuidVector> &rp,
            const utility::Uuid &playlist,
            const utility::UuidVector &containers,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);

        void move_containers_to(
            caf::typed_response_promise<utility::UuidVector> &rp,
            const utility::Uuid &playlist,
            const utility::UuidVector &containers,
            const bool with_media            = false,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);

        void save_json_to(
            caf::typed_response_promise<size_t> &rp,
            const utility::JsonStore &js,
            const caf::uri &path,
            const bool update_path = true,
            const size_t hash      = 0);

        void associate_bookmarks(caf::typed_response_promise<int> &rp);

        void sync_to_json_store(caf::typed_response_promise<bool> &rp);


        void gather_media_sources(
            caf::typed_response_promise<utility::UuidActorVector> &rp,
            const caf::actor &media,
            const utility::FrameRate &media_rate);
        void gather_media_sources_media_hook(
            caf::typed_response_promise<utility::UuidActorVector> &rp,
            const caf::actor &media,
            const utility::FrameRate &media_rate,
            utility::UuidActorVector &sources);
        void gather_media_sources_data_source(
            caf::typed_response_promise<utility::UuidActorVector> &rp,
            const caf::actor &media,
            const utility::FrameRate &media_rate,
            utility::UuidActorVector &sources);
        void gather_media_sources_add_media(
            caf::typed_response_promise<utility::UuidActorVector> &rp,
            const caf::actor &media,
            utility::UuidActorVector &sources);

      private:
        [[nodiscard]] std::vector<caf::actor> playlists() const {
            return utility::map_value_to_vec(playlists_);
        }

        void check_save_serialise_payload(
            std::shared_ptr<std::map<std::string, utility::JsonStore>> &payload,
            caf::typed_response_promise<utility::JsonStore> &rp);

        void
        check_media_hook_plugin_version(const utility::JsonStore &jsn, const caf::uri &path);

        caf::behavior behavior_;
        Session base_;
        caf::actor json_store_;
        caf::actor bookmarks_;
        std::map<utility::Uuid, caf::actor> playlists_;
        std::map<caf::actor_addr, std::string> serialise_targets_;
        // std::map<utility::Uuid, caf::actor> players_;

        // store gui conext information
        utility::UuidActor viewedContainer_;
        utility::UuidActor inspectedContainer_;

        std::vector<utility::UuidActorAddr> selectedMedia_;
    };
} // namespace session
} // namespace xstudio
