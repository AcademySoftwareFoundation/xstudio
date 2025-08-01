// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

#include "xstudio/playlist/playlist.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {

namespace utility {
    class JsonStore;
}

namespace playlist {

    class PlaylistActor : public caf::event_based_actor {
      public:
        PlaylistActor(
            caf::actor_config &cfg,
            const utility::JsonStore &jsn,
            const caf::actor &session = caf::actor());
        PlaylistActor(
            caf::actor_config &cfg,
            const std::string &name,
            const utility::Uuid &uuid = utility::Uuid(),
            const caf::actor &session = caf::actor());
        ~PlaylistActor() override = default;

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "PlaylistActor";

        void init();

        caf::behavior make_behavior() override { return behavior_; }

        void add_media(
            utility::UuidActor &ua,
            const utility::Uuid &uuid_before,
            caf::typed_response_promise<utility::UuidActor> rp);

        void create_container(
            caf::actor actor,
            caf::typed_response_promise<utility::UuidUuidActor> rp,
            const utility::Uuid &uuid_before,
            const bool into);

        void copy_container(
            const std::vector<utility::Uuid> &cuuids,
            caf::actor bookmarks,
            caf::typed_response_promise<utility::CopyResult> rp) const;

        utility::UuidActorVector
        copy_tree(utility::UuidTree<utility::PlaylistItem> &tree) const;
        utility::UuidActorVector
        get_containers(utility::UuidTree<utility::PlaylistItem> &tree) const;
        void duplicate_container(
            caf::typed_response_promise<utility::UuidVector> rp,
            const utility::Uuid &uuid,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        void duplicate_tree(utility::UuidTree<utility::PlaylistItem> &tree);
        void notify_tree(const utility::UuidTree<utility::PlaylistItem> &tree);
        // void load_from_path(const caf::uri &path, const bool recursive=true);
        void open_media_readers();
        void open_media_reader(caf::actor media_actor);
        void send_content_changed_event(const bool queue = true);
        void sort_alphabetically();

      private:
        caf::behavior behavior_;
        Playlist base_;
        caf::actor_addr session_;
        utility::UuidActor playhead_;
        caf::actor event_group_, change_event_group_;
        std::map<utility::Uuid, caf::actor> media_;
        std::map<utility::Uuid, caf::actor> container_;
        bool is_in_viewer_ = {false};
        caf::actor json_store_;
        bool content_changed_{false};
        caf::actor playlist_broadcast_;
        caf::actor selection_actor_;
        bool auto_gather_sources_{false};
    };
} // namespace playlist
} // namespace xstudio
