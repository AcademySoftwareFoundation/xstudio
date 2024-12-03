// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>
#include <filesystem>


#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/lock_file.hpp"
#include "xstudio/utility/frame_rate.hpp"

namespace xstudio {
namespace session {

    namespace fs = std::filesystem;


    class Session : public utility::Container {
      public:
        Session(const std::string &name = "Session");
        Session(const utility::JsonStore &jsn, caf::uri filepath = caf::uri());

        ~Session() override = default;
        // ~Session() override {spdlog::warn("~Session");}

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] bool empty() const { return playlists_.size() <= 1; }

        [[nodiscard]] const utility::PlaylistTree &containers() const { return playlists_; }
        std::optional<utility::Uuid> insert_group(
            const std::string &name, const utility::Uuid &uuid_before = utility::Uuid());
        std::optional<utility::Uuid> insert_divider(
            const std::string &name,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);

        bool rename_container(const std::string &name, const utility::Uuid &uuid);
        bool reflag_container(const std::string &flag, const utility::Uuid &uuid);

        // bool insert_container(const utility::Uuid &uuid, const utility::Uuid
        // &uuid_before=utility::Uuid(), const bool into=false);
        std::optional<utility::Uuid> insert_container(
            const utility::PlaylistItem &container,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        bool insert_container(
            utility::PlaylistTree container,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        bool remove_container(const utility::Uuid &uuid);
        [[nodiscard]] std::optional<utility::UuidVector>
        container_children(const utility::Uuid &uuid) const;
        bool move_container(
            const utility::Uuid &uuid,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);

        [[nodiscard]] std::optional<const utility::UuidTree<utility::PlaylistItem> *>
        cfind(const utility::Uuid &uuid) const {
            return playlists_.cfind(uuid);
        }
        [[nodiscard]] utility::FrameRate media_rate() const { return media_rate_; }
        [[nodiscard]] utility::FrameRate playhead_rate() const { return playhead_rate_; }

        [[nodiscard]] utility::Uuid current_playlist_uuid() const {
            return current_playlist_uuid_;
        }
        void set_current_playlist_uuid(const utility::Uuid &uuid) {
            current_playlist_uuid_ = uuid;
        }

        [[nodiscard]] utility::Uuid viewed_playlist_uuid() const {
            return viewed_playlist_uuid_;
        }
        void set_viewed_playlist_uuid(const utility::Uuid &uuid) {
            viewed_playlist_uuid_ = uuid;
        }

        void set_media_rate(const utility::FrameRate &rate) { media_rate_ = rate; }
        void set_playhead_rate(const utility::FrameRate &rate) { playhead_rate_ = rate; }
        void set_filepath(const caf::uri &path);

        [[nodiscard]] caf::uri filepath() const { return filepath_; }
        [[nodiscard]] fs::file_time_type session_file_mtime() const {
            return session_file_mtime_;
        }

        void set_container(const utility::PlaylistTree &pl) { playlists_ = pl; }

      private:
        utility::PlaylistTree playlists_;
        utility::FrameRate media_rate_;
        utility::FrameRate playhead_rate_;
        utility::Uuid current_playlist_uuid_;
        utility::Uuid viewed_playlist_uuid_;
        caf::uri filepath_;
        fs::file_time_type session_file_mtime_{fs::file_time_type::min()};
    };
} // namespace session
} // namespace xstudio