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
namespace subset {
    class Subset : public utility::Container {
      public:
        Subset(const std::string &name = "Subset", const std::string &type = "Subset");
        Subset(const utility::JsonStore &jsn);

        ~Subset() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] bool empty() const { return media_list_.empty(); }
        void clear() { media_list_.clear(); }

        [[nodiscard]] utility::UuidList media() const { return media_list_.uuids(); }
        [[nodiscard]] utility::UuidVector media_vector() const {
            return media_list_.uuid_vector();
        }
        void insert_media(
            const utility::Uuid &uuid, const utility::Uuid &uuid_before = utility::Uuid()) {
            media_list_.insert(uuid, uuid_before);
        }
        bool remove_media(const utility::Uuid &uuid) { return media_list_.remove(uuid); }
        bool move_media(
            const utility::Uuid &uuid, const utility::Uuid &uuid_before = utility::Uuid()) {
            return media_list_.move(uuid, uuid_before);
        }
        bool swap_media(const utility::Uuid &from, const utility::Uuid &to) {
            return media_list_.swap(from, to);
        }
        [[nodiscard]] bool contains_media(const utility::Uuid &uuid) const {
            return media_list_.contains(uuid);
        }

        [[nodiscard]] utility::FrameRate playhead_rate() const { return playhead_rate_; }
        void set_playhead_rate(const utility::FrameRate &rate) { playhead_rate_ = rate; }

      private:
        utility::UuidListContainer media_list_;
        utility::FrameRate playhead_rate_;
    };
} // namespace subset
} // namespace xstudio