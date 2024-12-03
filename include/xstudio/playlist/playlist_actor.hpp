// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

#include "xstudio/playlist/playlist.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/notification_handler.hpp"

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

        ~PlaylistActor();

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "PlaylistActor";

        void init();

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler()
                .or_else(base_.container_message_handler(this))
                .or_else(notification_.message_handler(this, base_.event_group()));
        }

        void add_media(
            utility::UuidActor &ua,
            const utility::Uuid &uuid_before,
            const bool delayed,
            caf::typed_response_promise<utility::UuidActor> rp);

        void recursive_add_media_with_subsets(
            caf::typed_response_promise<std::vector<utility::UuidActor>> rp,
            const caf::uri &path,
            const utility::Uuid &uuid_before);

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
        void sort_by_media_display_info(const int sort_column_index, const bool ascending);

        void duplicate(
            caf::typed_response_promise<utility::UuidActor> rp,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks);

        void duplicate_containers(
            caf::typed_response_promise<utility::UuidActor> rp,
            const utility::UuidActor &new_playlist,
            const utility::UuidUuidMap &media_map);

      private:
        caf::behavior behavior_;
        Playlist base_;
        utility::NotificationHandler notification_;

        caf::actor_addr session_;
        utility::UuidActor playhead_;
        caf::actor change_event_group_;
        caf::actor global_prefs_actor_;
        std::map<utility::Uuid, caf::actor> media_;
        std::map<utility::Uuid, caf::actor> container_;
        bool is_in_viewer_ = {false};
        caf::actor json_store_;
        bool content_changed_{false};
        caf::actor playlist_broadcast_;
        caf::actor selection_actor_;
        bool auto_gather_sources_{false};

        utility::UuidActorVector delayed_add_media_;
    };
} // namespace playlist
} // namespace xstudio
