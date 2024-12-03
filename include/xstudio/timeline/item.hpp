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
#include "xstudio/timeline/marker.hpp"
#include "xstudio/media/media.hpp"

namespace xstudio {
namespace timeline {

    typedef enum {
        IA_NONE   = 0x0L,
        IA_ENABLE = 0x1L,
        IA_ADDR   = 0x2L,
        IA_ACTIVE = 0x3L,
        IA_AVAIL  = 0x4L,
        IA_RANGE  = 0x5L,
        IA_INSERT = 0x6L,
        IA_REMOVE = 0x7L,
        IA_SPLICE = 0x8L,
        IA_NAME   = 0x9L,
        IA_FLAG   = 0x10L,
        IA_PROP   = 0x11L,
        IA_LOCK   = 0x12L,
        IA_MARKER = 0x13L,
        IA_DIRTY  = 0x14L,

    } ItemAction;

    class Item;
    using Items = std::list<Item>;

    using Point = std::pair<int, int>;
    using Box   = std::pair<Point, Point>;

    using ResolvedItem = std::pair<Item, utility::FrameRate>;

    typedef std::function<void(const utility::JsonStore &event, Item &item)> ItemEventFunc;

    class Item : private Items {
      public:
        Item(const ItemType item_type, const std::string name)
            : Items(),
              item_type_(item_type),
              name_(std::move(name)),
              uuid_addr_(utility::UuidActorAddr(utility::Uuid::generate(), caf::actor_addr())) {
        }

        Item(const ItemType item_type, const std::string name, const utility::FrameRate &rate)
            : Items(),
              item_type_(item_type),
              name_(std::move(name)),
              uuid_addr_(utility::UuidActorAddr(utility::Uuid::generate(), caf::actor_addr())) {
            has_available_range_ = true;
            active_range_        = utility::FrameRange(utility::FrameRateDuration(0, rate));
        }

        Item(
            const ItemType item_type,
            const utility::Uuid uuid,
            const std::optional<utility::FrameRange> active_range    = {},
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


        // Item(const Item& a);
        // Item& operator=(const Item&);

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


        [[nodiscard]] Item clone() const {
            Item item = *this;
            item.unbind();
            return item;
        }

        [[nodiscard]] const Items &children() const { return *this; }
        [[nodiscard]] Items &children() { return *this; }

        [[nodiscard]] const Markers &markers() const { return markers_; }
        [[nodiscard]] Markers &markers() { return markers_; }

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

        [[nodiscard]] std::optional<utility::FrameRateDuration> available_frame_start() const {
            if (has_available_range_)
                return available_range()->frame_start();
            return {};
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

        [[nodiscard]] std::optional<int> frame_at_item_frame(
            const utility::Uuid &item_uuid,
            const int item_local_frame,
            const bool skip_disabled) const;

        [[nodiscard]] std::optional<Point> top_left(const utility::Uuid &uuid) const;
        [[nodiscard]] std::optional<Point> bottom_right(const utility::Uuid &uuid) const;
        [[nodiscard]] std::optional<Box> box(const utility::Uuid &uuid) const;
        [[nodiscard]] Point top_left() const;
        [[nodiscard]] Point bottom_right() const;
        [[nodiscard]] Box box() const;

        [[nodiscard]] caf::actor_addr actor_addr() const { return uuid_addr_.second; }
        [[nodiscard]] caf::actor actor() const {
            return caf::actor_cast<caf::actor>(uuid_addr_.second);
        }
        [[nodiscard]] utility::UuidActor uuid_actor() const {
            return utility::UuidActor(uuid(), actor());
        }
        [[nodiscard]] bool enabled() const { return enabled_; }
        [[nodiscard]] bool locked() const { return locked_; }
        [[nodiscard]] std::string name() const { return name_; }
        [[nodiscard]] std::string flag() const { return flag_; }
        [[nodiscard]] utility::JsonStore prop() const { return prop_; }
        [[nodiscard]] bool transparent() const;
        [[nodiscard]] utility::UuidActorVector
        find_all_uuid_actors(const ItemType item_type, const bool only_enabled_items = false) const;

        [[nodiscard]] std::vector<std::reference_wrapper<const Item>>
        find_all_items(const ItemType item_type, const ItemType track_type = IT_NONE) const;

        [[nodiscard]] std::vector<std::reference_wrapper<Item>>
        find_all_items(const ItemType item_type, const ItemType track_type = IT_NONE);

        [[nodiscard]] utility::JsonStore
        serialise(const int depth = std::numeric_limits<int>::max()) const;

        bool replace_child(const Item &child);
        std::set<utility::Uuid> update(const utility::JsonStore &event);

        void clean(const bool purge_clips = false);
        utility::JsonStore refresh(const int depth = std::numeric_limits<int>::max());

        void set_uuid(const utility::Uuid &uuid) { uuid_addr_.first = uuid; }

        void reset_uuid(const bool recursive = false);
        void reset_media_uuid();
        void reset_actor(const bool recursive = false);

        utility::JsonStore set_enabled(const bool value);
        utility::JsonStore set_locked(const bool value);
        utility::JsonStore set_name(const std::string &value);
        utility::JsonStore set_flag(const std::string &value);
        utility::JsonStore set_prop(const utility::JsonStore &value);
        utility::JsonStore set_markers(const Markers &value);
        utility::JsonStore set_markers(const std::vector<Marker> &value) {
            return set_markers(Markers(value.begin(), value.end()));
        }

        void set_system(caf::actor_system *value) { the_system_ = value; }

        utility::JsonStore set_actor_addr(const caf::actor_addr &value);
        utility::JsonStore set_actor_addr(const caf::actor &value) {
            return set_actor_addr(caf::actor_cast<caf::actor_addr>(value));
        }

        utility::JsonStore set_active_range(const utility::FrameRange &value);
        utility::JsonStore set_available_range(const utility::FrameRange &value);
        utility::JsonStore
        set_range(const utility::FrameRange &avail, const utility::FrameRange &active);
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
                f.field("locked", x.locked_),
                f.field("name", x.name_),
                f.field("flag", x.flag_),
                f.field("prop", x.prop_),
                f.field("markers", x.markers_),
                f.field("has_av", x.has_available_range_),
                f.field("has_ac", x.has_active_range_),
                f.field("children", x.children()));
        }

        bool operator==(const Item &other) const {
            return item_type_ == other.item_type_ and
                   uuid_addr_.first == other.uuid_addr_.first and
                   available_range_ == other.available_range_ and
                   active_range_ == other.active_range_ and enabled_ == other.enabled_ and
                   locked_ == other.locked_ and flag_ == other.flag_ and
                   prop_ == other.prop_ and name_ == other.name_;
        }

        [[nodiscard]] std::optional<ResolvedItem> resolve_time(
            const utility::FrameRate &time,
            const media::MediaType mt     = media::MediaType::MT_IMAGE,
            const utility::UuidSet &focus = utility::UuidSet(),
            const bool must_have_focus    = false) const;

        // doesn't bake tracks
        [[nodiscard]] std::vector<ResolvedItem> resolve_time_raw(
            const utility::FrameRate &time,
            const media::MediaType mt     = media::MediaType::MT_IMAGE,
            const utility::UuidSet &focus = utility::UuidSet(),
            const bool ignore_disabled    = true) const;

        void undo(const utility::JsonStore &event);
        void redo(const utility::JsonStore &event);

        void bind_item_pre_event_func(ItemEventFunc fn, const bool recursive = false);
        void bind_item_post_event_func(ItemEventFunc fn, const bool recursive = false);

        // rest binds, required for copies..
        void unbind();

        [[nodiscard]] utility::JsonStore make_actor_addr_update() const;

        bool has_dirty(const utility::JsonStore &event);

        // This method allows an Item to produce a 'FrameTimeMap' which is the
        // data a playhead needs to do playback
        caf::typed_response_promise<media::FrameTimeMapPtr> get_all_frame_IDs(
            const media::MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate,
            const utility::UuidSet &focus_list = utility::UuidSet()
            );

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
        void
        set_range_direct(const utility::FrameRange &avail, const utility::FrameRange &active);
        void set_actor_addr_direct(const caf::actor_addr &value);
        void set_enabled_direct(const bool value);
        void set_locked_direct(const bool value);
        void set_name_direct(const std::string &value);
        void set_flag_direct(const std::string &value);
        void set_prop_direct(const utility::JsonStore &value);
        void set_markers_direct(const Markers &value);

        [[nodiscard]] std::string actor_addr_to_string(const caf::actor_addr &addr) const;
        [[nodiscard]] caf::actor_addr string_to_actor_addr(const std::string &addr) const;

        [[nodiscard]] caf::actor_system &system() const { return *the_system_; }

      private:

        friend class BuildFrameIDsHelper;

        ItemType item_type_{IT_NONE};
        utility::UuidActorAddr uuid_addr_;
        utility::FrameRange available_range_;
        utility::FrameRange active_range_;
        bool has_available_range_{false};
        bool has_active_range_{false};
        bool enabled_{true};
        bool locked_{false};
        std::string name_{};
        std::string flag_{};
        utility::JsonStore prop_{R"({})"_json};
        Markers markers_;

        // not sure if this is safe..
        caf::actor_system *the_system_{nullptr};
        ItemEventFunc item_pre_event_callback_{nullptr};
        ItemEventFunc item_post_event_callback_{nullptr};
        bool recursive_bind_pre_{false};
        bool recursive_bind_post_{false};
    };

    inline utility::UuidVector get_event_id(const utility::JsonStore &event) {
        auto result = utility::UuidVector();

        for (const auto &i : event)
            result.push_back(i.at("redo").at("event_id"));

        return result;
    }

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

    inline std::optional<Items::const_iterator>
    find_track_from_item(const Items &items, const utility::Uuid &uuid, const bool top = true) {
        auto it = std::find_if(items.cbegin(), items.cend(), [uuid](Item const &obj) {
            return obj.uuid() == uuid;
        });

        // search children
        if (it == items.cend()) {
            for (auto i = items.cbegin(); i != items.cend(); ++i) {
                auto ii = find_track_from_item(i->children(), uuid, false);

                if (ii) {
                    if (i->item_type() == IT_AUDIO_TRACK or i->item_type() == IT_VIDEO_TRACK)
                        it = i;
                    else
                        it = *ii;
                    break;
                }
            }
        }

        if (it == items.cend())
            return {};

        if (top and (it->item_type() != IT_AUDIO_TRACK and it->item_type() != IT_VIDEO_TRACK))
            return {};

        return it;
    }

    inline utility::UuidVector
    find_media_clips(const Items &items, const utility::Uuid &media_uuid) {
        auto result = utility::UuidVector();

        for (const auto &i : items) {
            switch (i.item_type()) {
            case ItemType::IT_CLIP:
                if (i.prop().value("media_uuid", utility::Uuid()) == media_uuid)
                    result.emplace_back(i.uuid());
                break;

            case ItemType::IT_TIMELINE:
            case ItemType::IT_AUDIO_TRACK:
            case ItemType::IT_VIDEO_TRACK:
            case ItemType::IT_STACK: {
                const auto more = find_media_clips(i.children(), media_uuid);
                if (not more.empty())
                    result.insert(result.end(), more.begin(), more.end());
            }

            break;

            default:
                break;
            }
        }

        return result;
    }

    inline std::set<utility::Uuid> get_event_ids(const utility::JsonStore &events) {
        auto result = std::set<utility::Uuid>();

        for (const auto &i : events)
            result.insert(i.at("undo").at("event_id"));

        return result;
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

    // Sure there's a better way of doing all this stuff..
    inline caf::actor find_parent_actor(const Item &item, const utility::Uuid &uuid) {
        auto result = caf::actor();

        auto item_it = find_uuid(item.children(), uuid);
        if (item_it != std::end(item.children())) {
            result = item.actor();
        } else {
            for (const auto &i : item.children()) {
                result = find_parent_actor(i, uuid);
                if (result)
                    break;
            }
        }

        return result;
    }

    inline auto
    find_indexes(const Items &items, const ItemType type, const bool ignore_disabled = false) {
        auto result = std::vector<int>();
        auto c      = 0;
        for (const auto &i : items) {
            if (i.item_type() == type and (i.enabled() or not ignore_disabled))
                result.push_back(c);
            c++;
        }

        return result;
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