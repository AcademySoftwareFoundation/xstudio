// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/item.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Item::Item(const utility::JsonStore &jsn, caf::actor_system *system)
    : Items(), the_system_(system) {
    uuid_addr_.first = jsn.at("uuid");
    item_type_       = jsn.at("type");
    enabled_         = jsn.at("enabled");
    name_            = jsn.value("name", "");

    if (jsn.count("actor_addr"))
        uuid_addr_.second = string_to_actor_addr(jsn.at("actor_addr"));

    if (jsn.at("active_range").is_null())
        active_range_ = {};
    else {
        has_active_range_ = true;
        active_range_     = jsn.at("active_range");
    }

    if (jsn.at("available_range").is_null())
        available_range_ = {};
    else {
        has_available_range_ = true;
        available_range_     = jsn.at("available_range");
    }

    for (const auto &i : jsn.at("children")) {
        emplace_back(Item(utility::JsonStore(i), the_system_));
    }
}

std::string Item::actor_addr_to_string(const caf::actor_addr &addr) const {
    std::string str;
    if (the_system_) {
        binary_serializer::container_type buf;
        binary_serializer bs{system(), buf};
        auto e = bs.apply(addr);
        if (e)
            str = utility::make_hex_string(std::begin(buf), std::end(buf));
    }
    return str;
}

caf::actor_addr Item::string_to_actor_addr(const std::string &str) const {
    caf::actor_addr addr;
    if (the_system_) {
        caf::binary_serializer::container_type buf = hex_to_bytes(str);
        caf::binary_deserializer bd{system(), buf};
        auto e = bd.apply(addr);
    }
    return addr;
}

utility::JsonStore Item::serialise(const int depth) const {
    utility::JsonStore jsn;

    jsn["uuid"]       = uuid_addr_.first;
    jsn["actor_addr"] = actor_addr_to_string(uuid_addr_.second);
    jsn["type"]       = item_type_;
    jsn["enabled"]    = enabled_;
    jsn["name"]       = name_;

    if (has_available_range_)
        jsn["available_range"] = available_range_;
    else
        jsn["available_range"] = nullptr;

    if (has_active_range_)
        jsn["active_range"] = active_range_;
    else
        jsn["active_range"] = nullptr;

    jsn["children"] = nlohmann::json::array();
    if (depth)
        for (const auto &i : *this)
            jsn["children"].emplace_back(i.serialise(depth - 1));

    return jsn;
}
std::optional<utility::FrameRange> Item::active_range() const {
    if (has_active_range_)
        return active_range_;

    return {};
}
std::optional<utility::FrameRange> Item::available_range() const {
    if (has_available_range_)
        return available_range_;
    return {};
}

utility::FrameRange Item::trimmed_range() const {
    if (has_active_range_)
        return active_range_;

    if (has_available_range_)
        return available_range_;

    return utility::FrameRange();
}

utility::FrameRate Item::trimmed_duration() const {
    if (has_active_range_)
        return active_range_.duration();

    if (has_available_range_)
        return available_range_.duration();

    return utility::FrameRate();
}

utility::FrameRate Item::trimmed_start() const {
    if (has_active_range_)
        return active_range_.start();

    if (has_available_range_)
        return available_range_.start();

    return utility::FrameRate();
}

std::optional<utility::FrameRate> Item::available_duration() const {
    if (has_available_range_)
        return available_range_.duration();
    return {};
}

std::optional<utility::FrameRate> Item::active_duration() const {
    if (has_active_range_)
        return active_range_.duration();
    return {};
}

std::optional<utility::FrameRate> Item::available_start() const {
    if (has_available_range_)
        return available_range_.start();
    return {};
}

std::optional<utility::FrameRate> Item::active_start() const {
    if (has_active_range_)
        return active_range_.start();
    return {};
}

std::optional<utility::FrameRateDuration> Item::active_frame_duration() const {
    if (has_active_range_)
        return active_range_.frame_duration();
    return {};
}

std::optional<utility::FrameRateDuration> Item::available_frame_duration() const {
    if (has_available_range_)
        return available_range_.frame_duration();
    return {};
}


// ordering might matter here..
// so watch out if things go wrong.
utility::JsonStore Item::refresh(const int depth) {
    if (not depth)
        return utility::JsonStore();

    auto result = nlohmann::json::array();

    for (auto &i : *this) {
        auto tmp = i.refresh(depth - 1);
        if (not tmp.is_null())
            result.insert(result.end(), tmp.begin(), tmp.end());
    }

    switch (item_type_) {
    case IT_TIMELINE: {
        auto rng = sum_trimmed_duration(*this);

        if (not available_range() or (*available_range()).duration() != rng) {
            auto tmp = set_available_range(FrameRange(
                available_range() ? (*available_range()).frame_start()
                                  : FrameRateDuration(0, rate()),
                FrameRateDuration(rng, rate())));
            if (not tmp.is_null())
                result.insert(result.end(), tmp.begin(), tmp.end());
        }
    } break;
    case IT_STACK: {
        auto rng = max_trimmed_duration(*this);

        if (not available_range() or (*available_range()).duration() != rng) {

            auto tmp = set_available_range(FrameRange(
                available_range() ? (*available_range()).frame_start()
                                  : FrameRateDuration(0, rate()),
                FrameRateDuration(rng, rate())));
            if (not tmp.is_null())
                result.insert(result.end(), tmp.begin(), tmp.end());
        }
    } break;

    case IT_AUDIO_TRACK:
    case IT_VIDEO_TRACK: {
        auto rng = sum_trimmed_duration(*this);
        if (not available_range() or (*available_range()).duration() != rng) {
            auto tmp = set_available_range(
                FrameRange(FrameRateDuration(0, rate()), FrameRateDuration(rng, rate())));
            if (not tmp.is_null())
                result.insert(result.begin(), tmp.begin(), tmp.end());
        }
    } break;

    case IT_GAP:
    case IT_CLIP:
    case IT_NONE:
    default:
        break;
    }

    if (result.empty())
        return utility::JsonStore();

    return utility::JsonStore(result);
}

bool Item::valid() const {
    bool valid = true;

    for (const auto &i : *this) {
        if (not valid_child(i)) {
            valid = false;
            break;
        }
    }

    if (valid) {
        for (const auto &i : *this) {
            if (not i.valid()) {
                valid = false;
                break;
            }
        }
    }

    return valid;
}

bool Item::replace_child(const Item &child) {
    auto it = find_uuid(*this, child.uuid());
    if (it != end()) {
        *it = child;
        return true;
    }
    return false;
}

bool Item::valid_child(const Item &child) const {
    bool valid = false;
    switch (item_type_) {

    case IT_TIMELINE:
        switch (child.item_type()) {
        case IT_STACK:
            valid = true;
            break;

        case IT_GAP:
        case IT_CLIP:
        case IT_NONE:
        case IT_AUDIO_TRACK:
        case IT_VIDEO_TRACK:
        case IT_TIMELINE:
        default:
            break;
        }
        break;
    case IT_STACK:
        switch (child.item_type()) {
        case IT_GAP:
        case IT_CLIP:
        case IT_STACK:
        case IT_AUDIO_TRACK:
        case IT_VIDEO_TRACK:
            valid = true;
            break;

        case IT_NONE:
        case IT_TIMELINE:
        default:
            break;
        }
        break;
    case IT_AUDIO_TRACK:
    case IT_VIDEO_TRACK:
        switch (child.item_type()) {

        case IT_GAP:
        case IT_CLIP:
        case IT_STACK:
            valid = true;
            break;

        case IT_NONE:
        case IT_AUDIO_TRACK:
        case IT_VIDEO_TRACK:
        case IT_TIMELINE:
        default:
            break;
        }
        break;

    case IT_GAP:
    case IT_CLIP:
    case IT_NONE:
    default:
        break;
    }

    return valid;
}

utility::UuidActorVector Item::find_all_uuid_actors(const ItemType item_type) const {
    utility::UuidActorVector items;

    if (item_type_ == item_type)
        items.push_back(uuid_actor());
    for (const auto &i : children()) {
        auto citems = i.find_all_uuid_actors(item_type);
        items.insert(items.end(), citems.begin(), citems.end());
    }

    return items;
}

std::optional<std::tuple<const Item &, utility::FrameRate>>
Item::resolve_time(const utility::FrameRate &time, const media::MediaType mt) const {
    if (transparent())
        return {};

    if (time >= trimmed_duration())
        return {};

    switch (item_type_) {
    case IT_TIMELINE:
        // pass to stack
        if (not empty()) {
            auto t = front().resolve_time(time + trimmed_start(), mt);
            if (t)
                return *t;
        }
        break;

    case IT_STACK:
        // if we have tracks we need to filter based off mt..
        // handle transparent..
        // needs depth first search ?
        // most of the logic lives here..
        if (mt == media::MediaType::MT_IMAGE) {
            for (const auto &it : *this) {
                // we skip audio track..
                if (it.transparent() or it.item_type() == IT_AUDIO_TRACK)
                    continue;
                auto t = it.resolve_time(time + trimmed_start(), mt);
                if (t)
                    return *t;
            }
        } else {
            for (const auto &it : *this) {
                // we skip video track
                if (it.transparent() or it.item_type() == IT_VIDEO_TRACK)
                    continue;
                auto t = it.resolve_time(time + trimmed_start(), mt);
                if (t)
                    return *t;
            }
        }
        // we shouldn't return the container..
        break;

    case IT_AUDIO_TRACK:
    case IT_VIDEO_TRACK:
        // sequentail list of items.
        {
            auto ttp = time;
            // spdlog::warn("{}", trimmed_start()/timebase::k_flicks_24fps);
            // spdlog::warn("{}", ttp/timebase::k_flicks_24fps);
            // spdlog::warn("{}", (ttp + trimmed_start())/timebase::k_flicks_24fps);
            const auto ts = trimmed_start();
            for (const auto &it : *this) {
                const auto td = it.trimmed_duration();
                if (ttp + ts >= td) {
                    ttp -= td;
                } else {
                    auto t = it.resolve_time(ttp + ts, mt);
                    if (t)
                        return *t;
                    break;
                }
            }
        }
        break;

    case IT_GAP:
    case IT_CLIP:
        return std::tuple<const Item &, utility::FrameRate>(*this, time + trimmed_start());
        break;
    case IT_NONE:
    default:
        break;
    }
    return {};
}


void Item::set_enabled_direct(const bool &value) { enabled_ = value; }

void Item::set_name_direct(const std::string &value) { name_ = value; }

utility::JsonStore Item::set_enabled(const bool &value) {
    if (enabled_ != value) {
        utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IT_ENABLE;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = enabled_;
        jsn[0]["redo"]["value"]                         = value;
        set_enabled_direct(value);
        return jsn;
    }

    return utility::JsonStore();
}

utility::JsonStore Item::set_name(const std::string &value) {
    if (name_ != value) {
        utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IT_NAME;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = name_;
        jsn[0]["redo"]["value"]                         = value;
        set_name_direct(value);
        return jsn;
    }

    return utility::JsonStore();
}

void Item::set_actor_addr_direct(const caf::actor_addr &value) { uuid_addr_.second = value; }

utility::JsonStore Item::make_actor_addr_update() const {
    utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);

    jsn[0]["redo"]["action"] = ItemAction::IT_ADDR;
    jsn[0]["redo"]["uuid"]   = uuid_addr_.first;
    jsn[0]["redo"]["value"]  = actor_addr_to_string(uuid_addr_.second);

    return jsn;
}

utility::JsonStore Item::set_actor_addr(const caf::actor_addr &value) {
    if (uuid_addr_.second != value) {
        utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IT_ADDR;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        if (uuid_addr_.second)
            jsn[0]["undo"]["value"] = actor_addr_to_string(uuid_addr_.second);
        else
            jsn[0]["undo"]["value"] = nullptr;

        if (value)
            jsn[0]["redo"]["value"] = actor_addr_to_string(value);
        else
            jsn[0]["redo"]["value"] = nullptr;

        set_actor_addr_direct(value);
        return jsn;
    }

    return utility::JsonStore();
}

void Item::set_active_range_direct(const utility::FrameRange &value) {
    has_active_range_ = true;
    active_range_     = value;
}

utility::JsonStore Item::set_active_range(const utility::FrameRange &value) {
    if (value != active_range_) {
        utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IT_ACTIVE;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        jsn[0]["undo"]["value"]  = active_range_;
        jsn[0]["undo"]["value2"] = has_active_range_;

        jsn[0]["redo"]["value"]  = value;
        jsn[0]["redo"]["value2"] = true;
        set_active_range_direct(value);
        return jsn;
    }

    return utility::JsonStore();
}

void Item::set_available_range_direct(const utility::FrameRange &value) {
    has_available_range_ = true;
    available_range_     = value;
}

utility::JsonStore Item::set_available_range(const utility::FrameRange &value) {
    if (value != available_range_) {
        utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);

        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IT_AVAIL;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        jsn[0]["undo"]["value"]  = available_range_;
        jsn[0]["undo"]["value2"] = has_available_range_;

        jsn[0]["redo"]["value"]  = value;
        jsn[0]["redo"]["value2"] = true;

        set_available_range_direct(value);
        return jsn;
    }
    return utility::JsonStore();
}

Items::iterator Item::insert_direct(Items::iterator position, const Item &val) {
    auto it = Items::insert(position, val);
    it->set_system(the_system_);
    if (recursive_bind_ and item_event_callback_)
        it->bind_item_event_func(item_event_callback_, recursive_bind_);
    return it;
}

utility::JsonStore
Item::insert(Items::iterator position, const Item &value, const utility::JsonStore &blind) {
    utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
    auto index = std::distance(begin(), position);

    jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

    jsn[0]["undo"]["action"]    = ItemAction::IT_REMOVE;
    jsn[0]["undo"]["index"]     = index;
    jsn[0]["undo"]["item_uuid"] = value.uuid();

    jsn[0]["redo"]["action"] = ItemAction::IT_INSERT;
    jsn[0]["redo"]["index"]  = index;
    jsn[0]["redo"]["item"]   = value.serialise();
    jsn[0]["redo"]["blind"]  = blind;

    insert_direct(position, value);

    return jsn;
}

Items::iterator Item::erase_direct(Items::iterator position) { return Items::erase(position); }

utility::JsonStore Item::erase(Items::iterator position) {
    utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
    auto index = std::distance(begin(), position);

    jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

    jsn[0]["undo"]["action"] = ItemAction::IT_INSERT;
    jsn[0]["undo"]["index"]  = index;
    jsn[0]["undo"]["item"]   = position->serialise();
    jsn[0]["undo"]["blind"]  = nullptr;

    jsn[0]["redo"]["action"]    = ItemAction::IT_REMOVE;
    jsn[0]["redo"]["index"]     = index;
    jsn[0]["redo"]["item_uuid"] = position->uuid();

    erase_direct(position);

    return jsn;
}

void Item::splice_direct(
    Items::const_iterator pos,
    Items &other,
    Items::const_iterator first,
    Items::const_iterator last) {
    Items::splice(pos, other, first, last);
}

utility::JsonStore Item::splice(
    Items::const_iterator pos,
    Items &other,
    Items::const_iterator first,
    Items::const_iterator last) {

    utility::JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);

    auto pos_index   = std::distance(cbegin(), pos);
    auto first_index = std::distance(cbegin(), first);
    auto last_index  = std::distance(cbegin(), last);

    // splice can't insert into range..
    // move position to end of range.
    if (pos_index >= first_index and pos_index <= last_index) {
        pos_index = last_index + 1;
        pos       = std::next(last, 1);
    }

    jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

    jsn[0]["redo"]["action"] = ItemAction::IT_SPLICE;
    jsn[0]["redo"]["dst"]    = pos_index;   // dst
    jsn[0]["redo"]["first"]  = first_index; // frst
    jsn[0]["redo"]["last"]   = last_index;  // lst

    if (pos_index > last_index) {
        pos_index -= (last_index - first_index);
    }

    jsn[0]["undo"]["action"] = ItemAction::IT_SPLICE;
    jsn[0]["undo"]["dst"]    = first_index; // dst
    jsn[0]["undo"]["first"]  = pos_index;
    jsn[0]["undo"]["last"]   = pos_index + (last_index - first_index);

    // spdlog::warn("{}", jsn.dump(2));

    splice_direct(pos, other, first, last);

    return jsn;
}

// apply update that occurred outside our tree.
bool Item::update(const utility::JsonStore &event) {
    bool changed = false;
    for (const auto &i : event)
        changed |= process_event(i.at("redo"));
    return changed;
}

void Item::undo(const utility::JsonStore &event) {
    for (const auto &i : event)
        process_event(i.at("undo"));
}

void Item::redo(const utility::JsonStore &event) {
    for (const auto &i : event)
        process_event(i.at("redo"));
}

bool Item::process_event(const utility::JsonStore &event) {
    // spdlog::warn("{} {} {}", event["uuid"], to_string(uuid_addr_.first), event["uuid"] ==
    // uuid_addr_.first);
    if (event.at("uuid") == uuid_addr_.first) {
        switch (static_cast<ItemAction>(event.at("action"))) {
        case IT_ENABLE:
            set_enabled_direct(event.at("value"));
            break;
        case IT_NAME:
            set_name_direct(event.at("value"));
            break;
        case IT_ACTIVE:
            set_active_range_direct(event.at("value"));
            has_active_range_ = event.at("value2");
            break;
        case IT_AVAIL:
            set_available_range_direct(event.at("value"));
            has_available_range_ = event.at("value2");
            break;
        case IT_INSERT: {
            auto it = begin();
            std::advance(it, event.at("index"));
            insert_direct(it, Item(JsonStore(event.at("item")), the_system_));
        } break;
        case IT_REMOVE: {
            auto it = begin();
            std::advance(it, event.at("index"));
            erase_direct(it);
        } break;
        case IT_SPLICE: {
            auto it1 = begin();
            std::advance(it1, event.at("dst"));
            auto it2 = begin();
            std::advance(it2, event.at("first"));
            auto it3 = begin();
            std::advance(it3, event.at("last"));
            splice_direct(it1, *this, it2, it3);
        } break;

        case IT_ADDR:
            if (event.at("value").is_null())
                set_actor_addr_direct(caf::actor_addr());
            else
                set_actor_addr_direct(string_to_actor_addr(event.at("value")));
            break;
        case IA_NONE:
        default:
            break;
        }
        if (item_event_callback_)
            item_event_callback_(event, *this);
    } else {
        // child ?
        for (auto &i : *this) {
            if (i.process_event(event))
                return true;
        }

        return false;
    }
    return true;
}

void Item::bind_item_event_func(ItemEventFunc fn, const bool recursive) {
    recursive_bind_      = recursive;
    item_event_callback_ = [fn](auto &&PH1, auto &&PH2) {
        return fn(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    };

    if (recursive_bind_) {
        for (auto &i : *this)
            i.bind_item_event_func(fn, recursive_bind_);
    }
}
