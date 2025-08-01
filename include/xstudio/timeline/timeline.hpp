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
    static const std::set<std::string> TIMELINE_TYPES(
        {"Clip",
         "Track",
         "Video Track",
         "Audio Track",
         "Gap",
         "Stack",
         "TimelineItem",
         "Timeline"});


    class Timeline : public utility::Container {
      public:
        Timeline(
            const std::string &name   = "Timeline",
            const utility::Uuid &uuid = utility::Uuid::generate(),
            const caf::actor &actor   = caf::actor());
        Timeline(const utility::JsonStore &jsn);

        ~Timeline() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;
        [[nodiscard]] Timeline duplicate() const;

        [[nodiscard]] const Item &item() const { return item_; }
        [[nodiscard]] Item &item() { return item_; }

        [[nodiscard]] const Items &children() const { return item_.children(); }
        [[nodiscard]] Items &children() { return item_.children(); }

        [[nodiscard]] bool valid_child(const Item &child) const {
            return item_.valid_child(child);
        }

        utility::JsonStore refresh_item() { return item_.refresh(); }
        [[nodiscard]] utility::FrameRate rate() const { return item_.rate(); }


        // media list operations.
        // so we can behave like a subset.


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

        [[nodiscard]] utility::UuidSet &focus_list() { return focus_list_; }
        [[nodiscard]] utility::UuidSet focus_list() const { return focus_list_; }

        void set_focus_list(const utility::UuidSet &list) { focus_list_ = list; }
        void set_focus_list(const utility::UuidVector &list) {
            focus_list_ = utility::UuidSet(list.begin(), list.end());
        }

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
        utility::UuidListContainer media_list_;
        utility::UuidSet focus_list_;

        // utility::UuidListContainer tracks_;
        // utility::FrameRateDuration start_time_;
    };
} // namespace timeline
} // namespace xstudio