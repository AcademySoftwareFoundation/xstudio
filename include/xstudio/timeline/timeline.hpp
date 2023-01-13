// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/frame_range.hpp"
#include "xstudio/timeline/item.hpp"

namespace xstudio {
namespace timeline {
    class Timeline : public utility::Container {
      public:
        Timeline(
            const std::string &name   = "Timeline",
            const utility::Uuid &uuid = utility::Uuid::generate(),
            const caf::actor &actor   = caf::actor());
        Timeline(const utility::JsonStore &jsn);

        ~Timeline() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] const Item &item() const { return item_; }
        [[nodiscard]] Item &item() { return item_; }

        [[nodiscard]] const Items &children() const { return item_.children(); }
        [[nodiscard]] Items &children() { return item_.children(); }

        [[nodiscard]] bool valid_child(const Item &child) const {
            return item_.valid_child(child);
        }

        utility::JsonStore refresh_item() { return item_.refresh(); }

        [[nodiscard]] utility::FrameRate rate() const { return item_.rate(); }


        // [[nodiscard]] utility::UuidList tracks() const { return tracks_.uuids(); }
        // void insert_track(
        //     const utility::Uuid &uuid, const utility::Uuid &uuid_before =
        //     utility::Uuid()) { tracks_.insert(uuid, uuid_before);
        // }
        // bool remove_track(const utility::Uuid &uuid) { return tracks_.remove(uuid); }
        // bool
        // move_track(const utility::Uuid &uuid, const utility::Uuid &uuid_before =
        // utility::Uuid())
        // {
        //     return tracks_.move(uuid, uuid_before);
        // }

        // void clear() { tracks_.clear(); }

        // [[nodiscard]] utility::FrameRate rate() const { return start_time_.rate(); }
        // [[nodiscard]] utility::FrameRateDuration start_time() const { return start_time_;
        // } void set_rate(const utility::FrameRate &rate) { start_time_.set_rate(rate,
        // true); }

        // [[nodiscard]] bool contains(const utility::Uuid &uuid) const {
        //     return tracks_.contains(uuid);
        // }

      private:
        Item item_;
        // utility::UuidListContainer tracks_;
        // utility::FrameRateDuration start_time_;
    };
} // namespace timeline
} // namespace xstudio