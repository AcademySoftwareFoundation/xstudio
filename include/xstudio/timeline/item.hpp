// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>
#include <caf/all.hpp>

#include "xstudio/utility/frame_range.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/timeline/enums.hpp"
#include "xstudio/media/enums.hpp"

namespace xstudio {
namespace timeline {

    typedef enum {
        IA_NONE   = 0x0L,
        IT_ENABLE = 0x1L,
        IT_ADDR   = 0x2L,
        IT_ACTIVE = 0x3L,
        IT_AVAIL  = 0x4L,
        IT_INSERT = 0x5L,
        IT_REMOVE = 0x6L,
        IT_SPLICE = 0x7L,
        IT_NAME   = 0x8L,
        IT_FLAG   = 0x9L,
        IT_PROP   = 0x10L,

    } ItemAction;

    class Item;
    using Items = std::list<Item>;

    using ResolvedItem = std::tuple<Item, utility::FrameRate>;

    typedef std::function<void(const utility::JsonStore &event, Item &item)> ItemEventFunc;

    class Item : private Items {
      public:
        Item(
            const ItemType item_type,
            const utility::Uuid uuid                              = utility::Uuid::generate(),
            const std::optional<utility::FrameRange> active_range = {},
            const std::optional<utility::FrameRange> available_range = {})
            : Items(),
              item_type_(item_type),
              uuid_addr_(utility::UuidActorAddr(std::move(uuid), caf::actor_addr())) {
            if (active_range) {
                has_active_range_ = true;
                active_range_     = std::move(*active_range);
            }
            if (available_range) {
                has_available_range_ = true;
                available_range_     = std::move(*available_range);
            }
        }

        Item(
            const ItemType item_type = ItemType::IT_NONE,
            const utility::UuidActorAddr uuact =
                utility::UuidActorAddr(utility::Uuid::generate(), caf::actor_addr()),
            const std::optional<utility::FrameRange> active_range    = {},
            const std::optional<utility::FrameRange> available_range = {})
            : Items(), item_type_(item_type), uuid_addr_(std::move(uuact)) {
            if (active_range) {
                has_active_range_ = true;
                active_range_     = std::move(*active_range);
            }
            if (available_range) {
                has_available_range_ = true;
                available_range_     = std::move(*available_range);
            }
        }
        Item(const utility::JsonStore &jsn, caf::actor_system *system = nullptr);

        using Items::clear;
        using Items::empty;
        using Items::size;

        using Items::begin;
        using Items::cbegin;
        using Items::cend;
        using Items::end;

        using Items::crbegin;
        using Items::crend;
        using Items::rbegin;
        using Items::rend;

        using Items::back;
        using Items::front;

        // these circumvent the event handler
        using Items::emplace_back;
        using Items::emplace_front;
        using Items::pop_back;
        using Items::pop_front;
        using Items::push_back;
        using Items::push_front;
        using Items::splice;


        [[nodiscard]] const Items &children() const { return *this; }
        [[nodiscard]] Items &children() { return *this; }

        [[nodiscard]] bool valid_child(const Item &child) const;
        [[nodiscard]] bool valid() const;

        [[nodiscard]] utility::Uuid uuid() const { return uuid_addr_.first; }
        [[nodiscard]] ItemType item_type() const { return item_type_; }

        [[nodiscard]] utility::FrameRange trimmed_range() const;
        [[nodiscard]] std::optional<utility::FrameRange> active_range() const;
        [[nodiscard]] std::optional<utility::FrameRange> available_range() const;

        [[nodiscard]] utility::FrameRate rate() const { return trimmed_range().rate(); }

        [[nodiscard]] utility::FrameRate trimmed_duration() const;
        [[nodiscard]] std::optional<utility::FrameRate> available_duration() const;
        [[nodiscard]] std::optional<utility::FrameRate> active_duration() const;

        [[nodiscard]] utility::FrameRate trimmed_start() const;
        [[nodiscard]] std::optional<utility::FrameRate> available_start() const;
        [[nodiscard]] std::optional<utility::FrameRate> active_start() const;

        [[nodiscard]] utility::FrameRateDuration trimmed_frame_start() const {
            return trimmed_range().frame_start();
        }
        [[nodiscard]] utility::FrameRateDuration trimmed_frame_duration() const {
            return trimmed_range().frame_duration();
        }
        [[nodiscard]] std::optional<utility::FrameRateDuration> active_frame_duration() const;
        [[nodiscard]] std::optional<utility::FrameRateDuration>
        available_frame_duration() const;

        [[nodiscard]] std::optional<std::pair<Items::const_iterator, int>>
        item_at_frame(const int frame) const;

        [[nodiscard]] std::optional<Items::const_iterator> item_at_index(const int index) const;

        [[nodiscard]] utility::FrameRange range_at_index(const int item_index) const;
        [[nodiscard]] int frame_at_index(const int item_index) const;
        [[nodiscard]] int frame_at_index(const int item_index, const int item_frame) const;

        [[nodiscard]] caf::actor_addr actor_addr() const { return uuid_addr_.second; }
        [[nodiscard]] caf::actor actor() const {
            return caf::actor_cast<caf::actor>(uuid_addr_.second);
        }
        [[nodiscard]] utility::UuidActor uuid_actor() const {
            return utility::UuidActor(uuid(), actor());
        }
        [[nodiscard]] bool enabled() const { return enabled_; }
        [[nodiscard]] std::string name() const { return name_; }
        [[nodiscard]] std::string flag() const { return flag_; }
        [[nodiscard]] utility::JsonStore prop() const { return prop_; }
        [[nodiscard]] bool transparent() const {
            if (item_type_ == ItemType::IT_GAP)
                return true;
            return not enabled_;
        }
        [[nodiscard]] utility::UuidActorVector
        find_all_uuid_actors(const ItemType item_type) const;
        [[nodiscard]] utility::JsonStore
        serialise(const int depth = std::numeric_limits<int>::max()) const;

        bool replace_child(const Item &child);
        bool update(const utility::JsonStore &event);

        utility::JsonStore refresh(const int depth = std::numeric_limits<int>::max());

        void set_uuid(const utility::Uuid &uuid) { uuid_addr_.first = uuid; }

        utility::JsonStore set_enabled(const bool &value);
        utility::JsonStore set_name(const std::string &value);
        utility::JsonStore set_flag(const std::string &value);
        utility::JsonStore set_prop(const utility::JsonStore &value);
        void set_system(caf::actor_system *value) { the_system_ = value; }

        utility::JsonStore set_actor_addr(const caf::actor_addr &value);
        utility::JsonStore set_actor_addr(const caf::actor &value) {
            return set_actor_addr(caf::actor_cast<caf::actor_addr>(value));
        }

        utility::JsonStore set_active_range(const utility::FrameRange &value);
        utility::JsonStore set_available_range(const utility::FrameRange &value);
        utility::JsonStore insert(
            Items::iterator position,
            const Item &val,
            const utility::JsonStore &blind = utility::JsonStore());
        utility::JsonStore
        erase(Items::iterator position, const utility::JsonStore &blind = utility::JsonStore());
        utility::JsonStore splice(
            Items::const_iterator pos,
            Items &other,
            Items::const_iterator first,
            Items::const_iterator last);

        template <class Inspector> friend bool inspect(Inspector &f, Item &x) {
            return f.object(x).fields(
                f.field("type", x.item_type_),
                f.field("uuact", x.uuid_addr_),
                f.field("app_rng", x.active_range_),
                f.field("ava_rng", x.available_range_),
                f.field("enabled", x.enabled_),
                f.field("name", x.name_),
                f.field("flag", x.flag_),
                f.field("prop", x.prop_),
                f.field("has_av", x.has_available_range_),
                f.field("has_ac", x.has_active_range_),
                f.field("children", x.children()));
        }

        bool operator==(const Item &other) const {
            return item_type_ == other.item_type_ and
                   uuid_addr_.first == other.uuid_addr_.first and
                   available_range_ == other.available_range_ and
                   active_range_ == other.active_range_ and enabled_ == other.enabled_ and
                   flag_ == other.flag_ and prop_ == other.prop_ and name_ == other.name_;
        }

        [[nodiscard]] std::optional<ResolvedItem> resolve_time(
            const utility::FrameRate &time,
            const media::MediaType mt     = media::MediaType::MT_IMAGE,
            const utility::UuidSet &focus = utility::UuidSet()) const;

        void undo(const utility::JsonStore &event);
        void redo(const utility::JsonStore &event);

        void bind_item_event_func(ItemEventFunc fn, const bool recursive = false);

        [[nodiscard]] utility::JsonStore make_actor_addr_update() const;

      private:
        bool process_event(const utility::JsonStore &event);
        void splice_direct(
            Items::const_iterator pos,
            Items &other,
            Items::const_iterator first,
            Items::const_iterator last);
        Items::iterator insert_direct(Items::iterator position, const Item &val);
        Items::iterator erase_direct(Items::iterator position);
        void set_active_range_direct(const utility::FrameRange &value);
        void set_available_range_direct(const utility::FrameRange &value);
        void set_actor_addr_direct(const caf::actor_addr &value);
        void set_enabled_direct(const bool &value);
        void set_name_direct(const std::string &value);
        void set_flag_direct(const std::string &value);
        void set_prop_direct(const utility::JsonStore &value);

        [[nodiscard]] std::string actor_addr_to_string(const caf::actor_addr &addr) const;
        [[nodiscard]] caf::actor_addr string_to_actor_addr(const std::string &addr) const;

        [[nodiscard]] caf::actor_system &system() const { return *the_system_; }

      private:
        ItemType item_type_{IT_NONE};
        utility::UuidActorAddr uuid_addr_;
        utility::FrameRange available_range_;
        utility::FrameRange active_range_;
        bool has_available_range_{false};
        bool has_active_range_{false};
        bool enabled_{true};
        std::string name_{};
        std::string flag_{};
        utility::JsonStore prop_{};

        // not sure if this is safe..
        caf::actor_system *the_system_{nullptr};
        ItemEventFunc item_event_callback_{nullptr};
        bool recursive_bind_{false};
    };

    inline std::optional<Items::const_iterator>
    find_item(const Items &items, const utility::Uuid &uuid) {
        auto it = std::find_if(items.cbegin(), items.cend(), [uuid](Item const &obj) {
            return obj.uuid() == uuid;
        });

        // search children
        if (it == items.cend()) {
            for (const auto &i : items) {
                auto ii = find_item(i.children(), uuid);

                if (ii) {
                    it = *ii;
                    break;
                }
            }
        }

        if (it == items.cend())
            return {};

        return it;
    }

    inline auto find_uuid(const Items &items, const utility::Uuid &uuid) {
        return std::find_if(items.cbegin(), items.cend(), [uuid](Item const &obj) {
            return obj.uuid() == uuid;
        });
    }

    inline auto find_uuid(Items &items, const utility::Uuid &uuid) {
        return std::find_if(
            items.begin(), items.end(), [uuid](Item const &obj) { return obj.uuid() == uuid; });
    }

    inline auto find_actor(const Items &items, const caf::actor &actor) {
        return std::find_if(items.cbegin(), items.cend(), [actor](Item const &obj) {
            return obj.actor() == actor;
        });
    }

    inline auto find_actor(Items &items, const caf::actor &actor) {
        return std::find_if(items.begin(), items.end(), [actor](Item const &obj) {
            return obj.actor() == actor;
        });
    }

    inline auto find_actor_addr(const Items &items, const caf::actor_addr &actor_addr) {
        return std::find_if(items.cbegin(), items.cend(), [actor_addr](Item const &obj) {
            return obj.actor_addr() == actor_addr;
        });
    }

    inline auto find_actor_addr(Items &items, const caf::actor_addr &actor_addr) {
        return std::find_if(items.begin(), items.end(), [actor_addr](Item const &obj) {
            return obj.actor_addr() == actor_addr;
        });
    }

    inline auto sum_trimmed_duration(const Items &items) {
        auto duration = utility::FrameRate();
        for (const auto &i : items)
            duration += i.trimmed_duration();

        return duration;
    }

    inline auto max_trimmed_duration(const Items &items) {
        auto duration = utility::FrameRate();
        for (const auto &i : items)
            duration = std::max(i.trimmed_duration(), duration);

        return duration;
    }

    inline std::string to_string(const ItemType it) {
        std::string str;
        switch (it) {
        case IT_NONE:
            str = "None";
            break;
        case IT_GAP:
            str = "Gap";
            break;
        case IT_CLIP:
            str = "Clip";
            break;
        case IT_AUDIO_TRACK:
            str = "Audio Track";
            break;
        case IT_VIDEO_TRACK:
            str = "Video Track";
            break;
        case IT_STACK:
            str = "Stack";
            break;
        case IT_TIMELINE:
            str = "Timeline";
            break;
        }
        return str;
    }

} // namespace timeline
} // namespace xstudio