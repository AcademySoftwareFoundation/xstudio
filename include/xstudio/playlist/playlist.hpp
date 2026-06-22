// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playlist {
    inline static const std::string PLAYLIST_FILE_VERSION = "0.1.0";

    class Playlist : public utility::Container {
      public:
        Playlist(const std::string &name = "Playlist");
        Playlist(const utility::JsonStore &jsn);

        ~Playlist() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] bool empty() const {
            return media_list_.empty() && container_tree_.empty();
        }

        [[nodiscard]] const utility::PlaylistTree &containers() const {
            return container_tree_;
        }
        std::optional<utility::Uuid> insert_group(
            const std::string &name, const utility::Uuid &uuid_before = utility::Uuid());
        std::optional<utility::Uuid> insert_divider(
            const std::string &name,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        bool rename_container(const std::string &name, const utility::Uuid &uuid);
        bool insert_container(
            utility::PlaylistTree container,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        std::optional<utility::Uuid> insert_container(
            const utility::PlaylistItem &container,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        bool remove_container(const utility::Uuid &uuid);
        [[nodiscard]] std::optional<utility::UuidVector>
        container_children(const utility::Uuid &uuid) const;
        bool move_container(
            const utility::Uuid &uuid,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        bool reflag_container(const std::string &flag, const utility::Uuid &uuid);

        [[nodiscard]] std::optional<const utility::UuidTree<utility::PlaylistItem> *>
        cfind(const utility::Uuid &uuid) const {
            return container_tree_.cfind(uuid);
        }

        [[nodiscard]] utility::UuidList media() const { return media_list_.uuids(); }
        void insert_media(
            const utility::Uuid &uuid, const utility::Uuid &uuid_before = utility::Uuid()) {
            media_list_.insert(uuid, uuid_before);
        }
        bool remove_media(const utility::Uuid &uuid) { return media_list_.remove(uuid); }
        bool move_media(
            const utility::Uuid &uuid, const utility::Uuid &uuid_before = utility::Uuid()) {
            return media_list_.move(uuid, uuid_before);
        }
        utility::Uuid next_media(const utility::Uuid &uuid) {
            return media_list_.next_uuid(uuid);
        }

        [[nodiscard]] utility::FrameRate media_rate() const { return media_rate_; }
        void set_media_rate(const utility::FrameRate &rate) { media_rate_ = rate; }

        [[nodiscard]] utility::FrameRate playhead_rate() const { return playhead_rate_; }
        void set_playhead_rate(const utility::FrameRate &rate) { playhead_rate_ = rate; }

        [[nodiscard]] bool expanded() const { return expanded_; }
        void set_expanded(const bool expanded) { expanded_ = expanded; }

      private:
        utility::UuidListContainer media_list_;
        utility::PlaylistTree container_tree_;

        utility::FrameRate media_rate_;
        utility::FrameRate playhead_rate_;
        bool expanded_ = {false};
    };
} // namespace playlist
} // namespace xstudio