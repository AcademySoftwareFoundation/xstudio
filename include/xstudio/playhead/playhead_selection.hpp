// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {
    class PlayheadSelection : public utility::Container {
      public:
        PlayheadSelection(
            const std::string &name = "PlayheadSelection",
            const std::string &type = "PlayheadSelection");
        PlayheadSelection(const utility::JsonStore &jsn);

        ~PlayheadSelection() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] bool empty() const { return selected_media_uuids_.empty(); }

        [[nodiscard]] size_t size() const { return selected_media_uuids_.size(); }

        [[nodiscard]] const utility::UuidList &items() const {
            return selected_media_uuids_.uuids();
        }
        [[nodiscard]] utility::UuidVector items_vec() const;
        void insert_item(
            const utility::Uuid &uuid, const utility::Uuid &uuid_before = utility::Uuid());
        bool remove_item(const utility::Uuid &uuid);
        bool move_item(
            const utility::Uuid &uuid, const utility::Uuid &uuid_before = utility::Uuid());
        void clear();
        [[nodiscard]] bool contains(const utility::Uuid &uuid) const;

        [[nodiscard]] utility::Uuid monitored() const { return monitored_uuid_; }
        void set_monitored(const utility::Uuid &uuid) { monitored_uuid_ = uuid; }

        bool operator==(const PlayheadSelection &other) const {
            return selected_media_uuids_ == other.selected_media_uuids_ and
                   monitored_uuid_ == other.monitored_uuid_;
        }

        bool operator!=(const PlayheadSelection &other) const { return not(*this == other); }

      private:
        utility::UuidListContainer selected_media_uuids_;
        utility::Uuid monitored_uuid_;
    };
} // namespace playhead
} // namespace xstudio