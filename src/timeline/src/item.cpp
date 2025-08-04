// SPDX-License-Identifier: Apache-2.0
#include <nlohmann/json.hpp>

#include "xstudio/timeline/item.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/global/xstudio_actor_system.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Item::Item(const JsonStore &jsn) : Items() {
    uuid_addr_.first = jsn.at("uuid");
    item_type_       = jsn.at("type");
    enabled_         = jsn.at("enabled");
    name_            = jsn.value("name", "");
    locked_          = jsn.value("locked", false);
    flag_            = jsn.value("flag", "");
    prop_            = jsn.value("prop", JsonStore(R"({})"_json));

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

    for (const auto &i : jsn.value("markers", R"([])"_json))
        markers_.emplace_back(Marker(JsonStore(i)));

    for (const auto &i : jsn.at("children")) {
        emplace_back(Item(JsonStore(i)));
    }
}

bool Item::transparent() const {
    auto result = not enabled_;

    if (item_type_ == ItemType::IT_GAP)
        result = true;
    else if (
        item_type_ == ItemType::IT_CLIP and
        (prop_.is_null() or prop_.value("media_uuid", Uuid()).is_null()))
        result = true;

    return result;
}


std::string Item::actor_addr_to_string(const caf::actor_addr &addr) const {
    std::string str;
    binary_serializer::container_type buf;
    binary_serializer bs{ActorSystemSingleton::actor_system_ref(), buf};
    auto e = bs.apply(addr);
    if (e)
        str = make_hex_string(std::begin(buf), std::end(buf));
    return str;
}

caf::actor_addr Item::string_to_actor_addr(const std::string &str) const {
    caf::actor_addr addr;
    caf::binary_serializer::container_type buf = hex_to_bytes(str);
    caf::binary_deserializer bd{ActorSystemSingleton::actor_system_ref(), buf};
    auto e = bd.apply(addr);
    return addr;
}

JsonStore Item::serialise(const int depth) const {
    JsonStore jsn;

    jsn["uuid"]       = uuid_addr_.first;
    jsn["actor_addr"] = actor_addr_to_string(uuid_addr_.second);
    jsn["type"]       = item_type_;
    jsn["enabled"]    = enabled_;
    jsn["locked"]     = locked_;
    jsn["name"]       = name_;
    jsn["flag"]       = flag_;
    jsn["prop"]       = prop_;
    jsn["markers"]    = serialise_markers(markers_);

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
std::optional<FrameRange> Item::active_range() const {
    if (has_active_range_)
        return active_range_;

    return {};
}
std::optional<FrameRange> Item::available_range() const {
    if (has_available_range_)
        return available_range_;
    return {};
}

FrameRange Item::trimmed_range() const {
    if (has_active_range_)
        return active_range_;

    if (has_available_range_)
        return available_range_;

    return FrameRange();
}

FrameRate Item::trimmed_duration() const {
    if (has_active_range_)
        return active_range_.duration();

    if (has_available_range_)
        return available_range_.duration();

    return FrameRate();
}

FrameRate Item::trimmed_start() const {
    if (has_active_range_)
        return active_range_.start();

    if (has_available_range_)
        return available_range_.start();

    return FrameRate();
}

std::optional<FrameRate> Item::available_duration() const {
    if (has_available_range_)
        return available_range_.duration();
    return {};
}

std::optional<FrameRate> Item::active_duration() const {
    if (has_active_range_)
        return active_range_.duration();
    return {};
}

std::optional<FrameRate> Item::available_start() const {
    if (has_available_range_)
        return available_range_.start();
    return {};
}

std::optional<FrameRate> Item::active_start() const {
    if (has_active_range_)
        return active_range_.start();
    return {};
}

std::optional<FrameRateDuration> Item::active_frame_duration() const {
    if (has_active_range_)
        return active_range_.frame_duration();
    return {};
}

std::optional<FrameRateDuration> Item::available_frame_duration() const {
    if (has_available_range_)
        return available_range_.frame_duration();
    return {};
}


// ordering might matter here..
// so watch out if things go wrong.
JsonStore Item::refresh(const int depth) {
    if (not depth)
        return JsonStore();

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
        return JsonStore();

    return JsonStore(result);
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

UuidActorVector
Item::find_all_uuid_actors(const ItemType item_type, const bool only_enabled_items) const {
    UuidActorVector items;

    if (item_type_ == item_type && (!only_enabled_items || enabled_))
        items.push_back(uuid_actor());
    for (const auto &i : children()) {
        auto citems = i.find_all_uuid_actors(item_type, only_enabled_items);
        items.insert(items.end(), citems.begin(), citems.end());
    }

    return items;
}

std::vector<std::reference_wrapper<const Item>>
Item::find_all_items(const ItemType item_type, const ItemType track_type) const {
    auto result = std::vector<std::reference_wrapper<const Item>>();

    auto is_track = (item_type_ == IT_AUDIO_TRACK or item_type_ == IT_VIDEO_TRACK);

    if (item_type_ == item_type)
        result.push_back(std::ref(*this));

    if (track_type == IT_NONE or not is_track or item_type_ == track_type) {
        for (const auto &i : children()) {
            auto citems = i.find_all_items(item_type, track_type);
            result.insert(result.end(), citems.begin(), citems.end());
        }
    }

    return result;
}

std::vector<std::reference_wrapper<Item>>
Item::find_all_items(const ItemType item_type, const ItemType track_type) {
    auto result = std::vector<std::reference_wrapper<Item>>();

    auto is_track = (item_type_ == IT_AUDIO_TRACK or item_type_ == IT_VIDEO_TRACK);

    if (item_type_ == item_type)
        result.push_back(std::ref(*this));

    if (track_type == IT_NONE or not is_track or item_type_ == track_type) {
        for (auto &i : children()) {
            auto citems = i.find_all_items(item_type, track_type);
            result.insert(result.end(), citems.begin(), citems.end());
        }
    }

    return result;
}

void Item::resolve_and_request_clip_frames(
    const timebase::flicks timeline_start,
    caf::actor &helper,
    std::vector<bool> &filled_frames,
    const timebase::flicks ref_time,
    const utility::FrameRate timeline_frame_rate,
    const media::MediaType mt,
    const UuidSet &focus,
    bool only_if_focussed) const {

    // This function recurses down through the tree of Timeline Item objects.

    // It is used to build (or 'bake') a complete map of individual FrameIDs that
    // allow a playhead to do playback and it is executed every time something
    // changes in the timeline (or any of its objects within). Therefore it is
    // crucial that this is as fast as possible.

    // When it recurses down to something that actually delivers frames of video
    // or audio (namely a Clip) we work out what frames the Clip contributes to
    // the timeline, we then send the timepoints for those frames to our
    // helper. This in turn requests the FrameIDs from the Clip and puts them
    // into the final map. This strategy means that our main loop going down
    // into the timeline doesn't get blocked on waiting for Clips to return
    // FrameIDs.

    // ref_time tells us the start position in the timeline for which we
    // evaluate an Item. So at the clip level ref_time is the timepoint in the
    // timeline where the clip's first frame will be shown.

    if (transparent())
        return;

    // if this item is focussed, and we are only interested in focussed items,
    // all children don't need to be focussed because they inherit focus from
    // the parent
    if (only_if_focussed && focus.count(uuid())) {
        only_if_focussed = false;
    }

    switch (item_type_) {

    case IT_TIMELINE:
        // pass to stack
        if (not empty()) {
            front().resolve_and_request_clip_frames(
                timeline_start,
                helper,
                filled_frames,
                ref_time,
                timeline_frame_rate,
                mt,
                focus,
                only_if_focussed);
        }
        break;

    case IT_STACK: {
        const auto track_type =
            mt == media::MediaType::MT_IMAGE ? IT_VIDEO_TRACK : IT_AUDIO_TRACK;

        for (const auto &it : *this) {

            // Skip tracks whose media type doesn't match requested media type
            if (it.transparent() or it.item_type() != track_type)
                continue;
            it.resolve_and_request_clip_frames(
                timeline_start,
                helper,
                filled_frames,
                ref_time + trimmed_start(),
                timeline_frame_rate,
                mt,
                focus,
                only_if_focussed);
        }

    } break;

    case IT_AUDIO_TRACK:
    case IT_VIDEO_TRACK: {
        // note the core logic here is that the duration of each item in the
        // video track offsets the start of the next item by the same amount.
        auto ts = ref_time;
        for (const auto &it : *this) {
            it.resolve_and_request_clip_frames(
                timeline_start, helper, filled_frames, ts, timeline_frame_rate, mt, focus, only_if_focussed);
            ts += it.trimmed_duration();
        }
    } break;

    case IT_CLIP:

        if (!only_if_focussed or focus.count(uuid())) {

            // the frame range of the clip in the timeline
            int64_t timeline_in_frame  = (ref_time - timeline_start).count() / timeline_frame_rate.count();
            int64_t timeline_out_frame = std::min(
                int64_t((ref_time - timeline_start + trimmed_duration()).count() / timeline_frame_rate.count()),
                int64_t(filled_frames.size()));

            // (clip local) timestamp of the first clip frame
            auto clip_local_frame_time = trimmed_start().to_flicks();

            /*spdlog::warn("IT_CLIP {} {} {} {} {} {} {}", 
                name(),
                timeline_in_frame,
                timeline_out_frame,
                timebase::to_seconds(timeline_frame_rate),
                timebase::to_seconds(trimmed_duration()),
                timebase::to_seconds(ref_time),
                int64_t(filled_frames.size())
            );*/

            // vector of timestamps in clip time space for which we want FrameIDs
            std::vector<timebase::flicks> clip_timepoints;

            auto frame_timeline_pos = ref_time;
            auto subrange_ref       = ref_time;

            // loop over the clip frames in the timeline
            for (int64_t tl_logical_frame = timeline_in_frame;
                 tl_logical_frame < timeline_out_frame;
                 ++tl_logical_frame) {

                // check if the current timeline logical frame has already been
                // resolved (by a clip in a track above this one)
                if (!filled_frames[tl_logical_frame]) {

                    // if we're starting a new run of frames to drop into the map for this clip
                    // we need to hold the timeline timestamp of the current frame
                    if (clip_timepoints.empty()) {
                        subrange_ref = frame_timeline_pos;
                        clip_timepoints.reserve(timeline_out_frame - timeline_in_frame);
                    }

                    clip_timepoints.push_back(clip_local_frame_time);

                    // mark the frame as fulfilled - other tracks can't overwrite this
                    // timeline logical frame
                    filled_frames[tl_logical_frame] = true;

                } else if (!clip_timepoints.empty()) {

                    // we've got a run of clip frames to put in the map and we've
                    // hit a frame that's already filled in (by another clip
                    // on a higher track). Dispatch the set of clip timepoints
                    // to our helper so it can handle the request to the clip actor
                    // and fill in the frame map
                    anon_mail(
                        media::get_media_pointer_atom_v, actor(), clip_timepoints, subrange_ref)
                        .send(helper);
                    clip_timepoints.clear();
                }
                clip_local_frame_time += timeline_frame_rate;
                frame_timeline_pos += timeline_frame_rate;
            }

            if (!clip_timepoints.empty()) {
                // Dispatch request for clip frames to our helper
                anon_mail(
                    media::get_media_pointer_atom_v, actor(), clip_timepoints, subrange_ref)
                    .send(helper);
            }
        }
    case IT_GAP:
    case IT_NONE:
    default:
        break;
    }
}


std::optional<ResolvedItem> Item::resolve_time(
    const FrameRate &time,
    const media::MediaType mt,
    const UuidSet &focus,
    const bool must_have_focus) const {
    if (transparent())
        return {};

    if (time >= trimmed_duration())
        return {};

    switch (item_type_) {
    case IT_TIMELINE:
        // pass to stack
        if (not empty()) {
            auto t = front().resolve_time(time + trimmed_start(), mt, focus, must_have_focus);
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
            std::optional<ResolvedItem> found_item = {};

            for (const auto &it : *this) {
                // we skip audio track..
                if (it.transparent() or it.item_type() == IT_AUDIO_TRACK)
                    continue;

                auto requires_focus = must_have_focus and not focus.count(it.uuid());
                auto t = it.resolve_time(time + trimmed_start(), mt, focus, requires_focus);

                if (t) {
                    // first found item has result and we're not filtering
                    if (focus.empty())
                        return *t;

                    const auto &item = t->first;

                    // we are filtering and container is focused
                    if (focus.count(it.uuid()) and item.item_type() == IT_CLIP)
                        return *t;

                    // item is focused
                    if (focus.count(item.uuid()))
                        return *t;

                    // default first match
                    if (not must_have_focus and not found_item and item.item_type() == IT_CLIP)
                        found_item = *t;
                }
            }
            if (found_item)
                return *found_item;

        } else {
            std::optional<ResolvedItem> found_item = {};
            for (const auto &it : *this) {
                // we skip video track
                if (it.transparent() or it.item_type() == IT_VIDEO_TRACK)
                    continue;
                auto requires_focus = must_have_focus and not focus.count(it.uuid());
                auto t = it.resolve_time(time + trimmed_start(), mt, focus, requires_focus);
                if (t) {
                    if (focus.empty())
                        return *t;

                    const auto &item = t->first;

                    if (focus.count(it.uuid()) and item.item_type() == IT_CLIP)
                        return *t;

                    if (focus.count(item.uuid()))
                        return *t;

                    if (not must_have_focus and not found_item and item.item_type() == IT_CLIP)
                        found_item = *t;
                }
            }
            if (found_item)
                return *found_item;
        }
        // we shouldn't return the container..
        break;

    case IT_AUDIO_TRACK:
    case IT_VIDEO_TRACK:
        // sequentail list of items.
        {
            auto requires_focus = must_have_focus and not focus.count(uuid());
            auto ttp            = time;
            // spdlog::warn("{}", trimmed_start()/timebase::k_flicks_24fps);
            // spdlog::warn("{}", ttp/timebase::k_flicks_24fps);
            // spdlog::warn("{}", (ttp + trimmed_start())/timebase::k_flicks_24fps);
            // spdlog::warn("IT_VIDEO_TRACK {} {}", name(), requires_focus);

            const auto ts = trimmed_start();
            for (const auto &it : *this) {
                const auto td = it.trimmed_duration();

                if (ttp + ts >= td) {
                    ttp -= td;
                } else {
                    auto t = it.resolve_time(ttp + ts, mt, focus, requires_focus);
                    if (t)
                        return *t;
                    break;
                }
            }
        }
        break;

    case IT_CLIP:
        if (not must_have_focus or focus.count(uuid()))
            return std::pair<const Item &, FrameRate>(*this, time + trimmed_start());
        break;
    case IT_GAP:
    case IT_NONE:
    default:
        break;
    }
    return {};
}

std::vector<ResolvedItem> Item::resolve_time_raw(
    const FrameRate &time,
    const media::MediaType mt,
    const UuidSet &focus,
    const bool ignore_disabled) const {
    auto result = std::vector<ResolvedItem>();

    if (ignore_disabled and transparent())
        return result;

    if (time >= trimmed_duration())
        return result;

    switch (item_type_) {
    case IT_TIMELINE:
        // pass to stack
        if (not empty()) {
            result =
                front().resolve_time_raw(time + trimmed_start(), mt, focus, ignore_disabled);
        }
        break;

    case IT_STACK:
        // capture all items unless focus match found..
        if (mt == media::MediaType::MT_IMAGE) {
            auto focus_found = std::vector<ResolvedItem>();

            for (const auto &it : *this) {
                // we skip audio track..
                if ((ignore_disabled and it.transparent()) or it.item_type() == IT_AUDIO_TRACK)
                    continue;

                auto t =
                    it.resolve_time_raw(time + trimmed_start(), mt, focus, ignore_disabled);

                // track matches focus
                if (not focus.empty() and focus.count(it.uuid()))
                    focus_found.insert(focus_found.end(), t.begin(), t.end());
                else
                    result.insert(result.end(), t.begin(), t.end());
            }

            // parsed stack or results.
            if (not focus.empty()) {
                // scan results for focus items
                // in which case we only return those items.
                for (const auto &i : result) {
                    if (i.first.item_type() == IT_CLIP and focus.count(i.first.uuid()))
                        focus_found.push_back(i);
                }

                if (not focus_found.empty())
                    result = focus_found;
            }
        } else {
            auto focus_found = std::vector<ResolvedItem>();

            // should this be reversed ?
            for (const auto &it : *this) {
                // we skip audio track..
                if (ignore_disabled and it.transparent() or it.item_type() == IT_VIDEO_TRACK)
                    continue;

                auto t =
                    it.resolve_time_raw(time + trimmed_start(), mt, focus, ignore_disabled);

                // track matches focus
                if (not focus.empty() and focus.count(it.uuid()))
                    focus_found.insert(focus_found.end(), t.begin(), t.end());
                else
                    result.insert(result.end(), t.begin(), t.end());
            }

            // parsed stack or results.
            if (not focus.empty()) {
                // scan results for focus items
                // in which case we only return those items.
                for (const auto &i : result) {
                    if (i.first.item_type() == IT_CLIP and focus.count(i.first.uuid()))
                        focus_found.push_back(i);
                }

                if (not focus_found.empty())
                    result = focus_found;
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
                    result = it.resolve_time_raw(ttp + ts, mt, focus, ignore_disabled);
                    break;
                }
            }
        }
        break;

    case IT_GAP:
    case IT_CLIP:
        result.emplace_back(std::pair<const Item &, FrameRate>(*this, time + trimmed_start()));
        break;
    case IT_NONE:
    default:
        break;
    }

    return result;
}

void Item::set_enabled_direct(const bool value) { enabled_ = value; }

void Item::set_locked_direct(const bool value) { locked_ = value; }

void Item::set_name_direct(const std::string &value) { name_ = value; }

void Item::set_flag_direct(const std::string &value) { flag_ = value; }

void Item::set_prop_direct(const JsonStore &value) { prop_ = value; }

void Item::set_markers_direct(const Markers &value) { markers_ = value; }

bool Item::has_dirty(const JsonStore &event) {
    auto result = false;
    for (const auto &i : event) {
        if (i.at("undo").value("action", ItemAction::IA_NONE) == ItemAction::IA_DIRTY) {
            result = true;
            break;
        }
    }

    return result;
}


JsonStore Item::set_enabled(const bool value) {
    if (enabled_ != value) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_ENABLE;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = enabled_;
        jsn[0]["redo"]["value"]                         = value;

        jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
        jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
        jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
        jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

        set_enabled_direct(value);
        return jsn;
    }

    return JsonStore();
}

JsonStore Item::set_locked(const bool value) {
    if (locked_ != value) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_LOCK;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = locked_;
        jsn[0]["redo"]["value"]                         = value;
        set_locked_direct(value);
        return jsn;
    }

    return JsonStore();
}

JsonStore Item::set_name(const std::string &value) {
    if (name_ != value) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_NAME;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = name_;
        jsn[0]["redo"]["value"]                         = value;
        set_name_direct(value);
        return jsn;
    }

    return JsonStore();
}

JsonStore Item::set_flag(const std::string &value) {
    if (flag_ != value) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_FLAG;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = flag_;
        jsn[0]["redo"]["value"]                         = value;
        set_flag_direct(value);
        return jsn;
    }

    return JsonStore();
}

JsonStore Item::set_prop(const JsonStore &value) {
    if (prop_ != value) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_PROP;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;
        jsn[0]["undo"]["value"]                         = prop_;
        jsn[0]["redo"]["value"]                         = value;

        if (item_type_ == IT_CLIP) {
            jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
            jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
            jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
            jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;
        }

        set_prop_direct(value);
        return jsn;
    }

    return JsonStore();
}

void Item::set_actor_addr_direct(const caf::actor_addr &value) { uuid_addr_.second = value; }

JsonStore Item::make_actor_addr_update() const {
    JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);

    jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
    jsn[0]["redo"]["action"]                                = ItemAction::IA_ADDR;
    jsn[0]["redo"]["uuid"]                                  = uuid_addr_.first;
    jsn[0]["redo"]["value"] = actor_addr_to_string(uuid_addr_.second);

    return jsn;
}

JsonStore Item::set_actor_addr(const caf::actor_addr &value) {
    if (uuid_addr_.second != value) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_ADDR;
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

    return JsonStore();
}

JsonStore Item::set_markers(const Markers &value) {
    if (value != markers_) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_MARKER;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        jsn[0]["undo"]["value"] = serialise_markers(markers_);
        jsn[0]["redo"]["value"] = serialise_markers(value);

        set_markers_direct(value);
        return jsn;
    }
    return JsonStore();
}

void Item::set_range_direct(const FrameRange &avail, const FrameRange &active) {
    set_available_range_direct(avail);
    set_active_range_direct(active);
}

JsonStore Item::set_range(const FrameRange &avail, const FrameRange &active) {
    if (active != active_range_ || avail != available_range_) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_RANGE;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        jsn[0]["undo"]["value"]  = available_range_;
        jsn[0]["undo"]["value2"] = has_available_range_;
        jsn[0]["undo"]["value3"] = active_range_;
        jsn[0]["undo"]["value4"] = has_active_range_;

        jsn[0]["redo"]["value"]  = avail;
        jsn[0]["redo"]["value2"] = true;
        jsn[0]["redo"]["value3"] = active;
        jsn[0]["redo"]["value4"] = true;

        jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
        jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
        jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
        jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

        set_range_direct(avail, active);
        return jsn;
    }

    return JsonStore();
}

void Item::override_frame_rate(
    const utility::FrameRate &new_rate, const bool force_media_rate_to_match) {

    if (force_media_rate_to_match) {

        // the duration (in FRAMES) is not affected. The media in the clip is effectively sped
        // up or slowed down to match the 'new_rate' So if you have media that is 30Fps, 5
        // seconds long it starts off with 150 frames. Let's say the clip frame rate (new_rate)
        // is 60fps. The clip will now be 2.5 seconds long, and it will be 60Fps and 150 frames
        // in duration. During playback, frame 1 will be played then frame 2 then frame 3 with
        // no repetition or dropped frames
        available_range_ = utility::FrameRange(
            new_rate.to_flicks() * (available_range_.start().to_flicks().count() /
                                    available_range_.rate().to_flicks().count()),
            new_rate.to_flicks() * (available_range_.duration().to_flicks().count() /
                                    available_range_.rate().to_flicks().count()),
            new_rate);
        active_range_ = utility::FrameRange(
            new_rate.to_flicks() * (active_range_.start().to_flicks().count() /
                                    active_range_.rate().to_flicks().count()),
            new_rate.to_flicks() * (active_range_.duration().to_flicks().count() /
                                    active_range_.rate().to_flicks().count()),
            new_rate);


    } else {

        // the duration (in SECONDS) is not affected. The media in the clip is effectively
        // retimed with a 'nearest frame'. So if you have media that is 30Fps, 5 seconds long it
        // starts off with 150 frames. Let's say the clip frame rate (new_rate) is 60fps. The
        // clip will still be 5 seconds long, but it will be 60Fps and 300 frames in duration.
        // During playback, frame 1 of the underlying media will be repeated twice, then frame 2
        // will be repeated twice and so-on
        available_range_ = utility::FrameRange(
            new_rate.to_flicks() *
                (available_range_.start().to_flicks().count() / new_rate.to_flicks().count()),
            new_rate.to_flicks() * (available_range_.duration().to_flicks().count() /
                                    new_rate.to_flicks().count()),
            new_rate);
        active_range_ = utility::FrameRange(
            new_rate.to_flicks() *
                (active_range_.start().to_flicks().count() / new_rate.to_flicks().count()),
            new_rate.to_flicks() *
                (active_range_.duration().to_flicks().count() / new_rate.to_flicks().count()),
            new_rate);
    }
}

void Item::set_active_range_direct(const FrameRange &value) {
    has_active_range_ = true;
    active_range_     = value;
}

JsonStore Item::set_active_range(const FrameRange &value) {
    if (value != active_range_) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();

        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_ACTIVE;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        jsn[0]["undo"]["value"]  = active_range_;
        jsn[0]["undo"]["value2"] = has_active_range_;

        jsn[0]["redo"]["value"]  = value;
        jsn[0]["redo"]["value2"] = true;

        jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
        jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
        jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
        jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

        set_active_range_direct(value);
        return jsn;
    }

    return JsonStore();
}

void Item::set_available_range_direct(const FrameRange &value) {
    has_available_range_ = true;
    available_range_     = value;
}

JsonStore Item::set_available_range(const FrameRange &value) {
    if (value != available_range_) {
        JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);

        jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
        jsn[0]["undo"]["action"] = jsn[0]["redo"]["action"] = ItemAction::IA_AVAIL;
        jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

        jsn[0]["undo"]["value"]  = available_range_;
        jsn[0]["undo"]["value2"] = has_available_range_;

        jsn[0]["redo"]["value"]  = value;
        jsn[0]["redo"]["value2"] = true;

        jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
        jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
        jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
        jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

        set_available_range_direct(value);
        return jsn;
    }
    return JsonStore();
}

Items::iterator Item::insert_direct(Items::iterator position, const Item &val) {
    auto it = Items::insert(position, val);
    if (recursive_bind_post_ and item_post_event_callback_)
        it->bind_item_post_event_func(item_post_event_callback_, recursive_bind_post_);
    if (recursive_bind_pre_ and item_pre_event_callback_)
        it->bind_item_pre_event_func(item_pre_event_callback_, recursive_bind_pre_);
    return it;
}

JsonStore Item::insert(Items::iterator position, const Item &value, const JsonStore &blind) {
    JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
    auto index = std::distance(begin(), position);

    jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
    jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

    jsn[0]["undo"]["action"] = ItemAction::IA_REMOVE;
    jsn[0]["undo"]["index"]  = index;
    jsn[0]["undo"]["uuid"]   = value.uuid();

    jsn[0]["redo"]["action"] = ItemAction::IA_INSERT;
    jsn[0]["redo"]["index"]  = index;
    jsn[0]["redo"]["item"]   = value.serialise();
    jsn[0]["redo"]["blind"]  = blind;

    jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
    jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
    jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
    jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

    insert_direct(position, value);

    return jsn;
}

Items::iterator Item::erase_direct(Items::iterator position) { return Items::erase(position); }

JsonStore Item::erase(Items::iterator position, const JsonStore &blind) {
    JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);
    auto index = std::distance(begin(), position);

    jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
    jsn[0]["undo"]["item_uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

    jsn[0]["undo"]["action"] = ItemAction::IA_INSERT;
    jsn[0]["undo"]["index"]  = index;
    jsn[0]["undo"]["item"]   = position->serialise();
    jsn[0]["undo"]["blind"]  = blind;

    jsn[0]["redo"]["action"]    = ItemAction::IA_REMOVE;
    jsn[0]["redo"]["index"]     = index;
    jsn[0]["redo"]["item_uuid"] = position->uuid();

    jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
    jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
    jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
    jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

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

JsonStore Item::splice(
    Items::const_iterator pos,
    Items &other,
    Items::const_iterator first,
    Items::const_iterator last) {

    JsonStore jsn(R"([{"undo":{}, "redo":{}}])"_json);

    auto dst_index   = std::distance(cbegin(), pos);
    auto start_index = std::distance(cbegin(), first);
    auto count       = std::distance(first, last);

    jsn[0]["undo"]["event_id"] = jsn[0]["redo"]["event_id"] = Uuid::generate();
    jsn[0]["undo"]["uuid"] = jsn[0]["redo"]["uuid"] = uuid_addr_.first;

    jsn[0]["redo"]["action"] = ItemAction::IA_SPLICE;
    jsn[0]["redo"]["dst"]    = dst_index;   // dst
    jsn[0]["redo"]["first"]  = start_index; // frst
    jsn[0]["redo"]["count"]  = count;

    int undo_first;
    auto undo_dst = start_index;

    if (dst_index > start_index) {
        undo_first = dst_index - count;
    } else {
        undo_first = dst_index;
        undo_dst += count;
    }

    jsn[0]["undo"]["action"] = ItemAction::IA_SPLICE;
    jsn[0]["undo"]["dst"]    = undo_dst;
    jsn[0]["undo"]["first"]  = undo_first;
    jsn[0]["undo"]["count"]  = count;

    jsn.push_back(R"({"undo":{}, "redo":{}})"_json);
    jsn[1]["undo"]["event_id"] = jsn[1]["redo"]["event_id"] = Uuid::generate();
    jsn[1]["undo"]["action"] = jsn[1]["redo"]["action"] = ItemAction::IA_DIRTY;
    jsn[1]["undo"]["uuid"] = jsn[1]["redo"]["uuid"] = uuid_addr_.first;

    // std::cerr << "first " << undo_first << std::endl;
    // std::cerr << "  dst " << undo_dst << std::endl;
    // std::cerr << std::endl << std::flush;

    splice_direct(pos, other, first, last);

    return jsn;
}

// apply update that occurred outside our tree.
std::set<Uuid> Item::update(const JsonStore &event) {
    auto result = std::set<Uuid>();
    for (const auto &i : event)
        if (process_event(i.at("redo")))
            result.insert(i.at("redo").at("event_id"));
    return result;
}

void Item::undo(const JsonStore &event) {
    // reverse ordering..
    for (auto it = event.crbegin(); it != event.crend(); ++it)
        process_event(it->at("undo"));
}

void Item::redo(const JsonStore &event) {
    for (const auto &i : event)
        process_event(i.at("redo"));
}

bool Item::process_event(const JsonStore &event) {


    if (Uuid(event.at("uuid")) == uuid_addr_.first) {
        if (item_pre_event_callback_)
            item_pre_event_callback_(event, *this);

        switch (static_cast<ItemAction>(event.at("action"))) {
        case IA_ENABLE:
            set_enabled_direct(event.at("value"));
            break;
        case IA_LOCK:
            set_locked_direct(event.at("value"));
            break;
        case IA_NAME:
            set_name_direct(event.at("value"));
            break;
        case IA_FLAG:
            set_flag_direct(event.at("value"));
            break;
        case IA_PROP:
            set_prop_direct(event.at("value"));
            break;
        case IA_MARKER: {
            auto tmp = Markers();
            for (const auto &i : event.at("value"))
                tmp.emplace_back(Marker(JsonStore(i)));
            set_markers_direct(tmp);
        } break;
        case IA_ACTIVE:
            set_active_range_direct(event.at("value"));
            has_active_range_ = event.at("value2");
            break;
        case IA_AVAIL:
            set_available_range_direct(event.at("value"));
            has_available_range_ = event.at("value2");
            break;
        case IA_RANGE:
            set_available_range_direct(event.at("value"));
            has_available_range_ = event.at("value2");
            set_active_range_direct(event.at("value3"));
            has_active_range_ = event.at("value4");
            break;
        case IA_INSERT: {
            auto index = event.at("index").get<size_t>();
            if (index >= 0 and index <= size()) {
                insert_direct(std::next(begin(), index), Item(JsonStore(event.at("item"))));
            } else {
                spdlog::error(
                    "IA_INSERT - INVALID INDEX uuid {} name {} type {} index {} track size {} "
                    "{}",
                    to_string(uuid()),
                    name(),
                    to_string(item_type()),
                    index,
                    size(),
                    event.dump(2));
            }
        } break;
        case IA_REMOVE: {
            // spdlog::warn("IT_REMOVE {}", event.dump(2));
            auto index = event.at("index").get<size_t>();
            if (index < size()) {
                erase_direct(std::next(begin(), index));
            } else {
                spdlog::error(
                    "IA_REMOVE - INVALID INDEX {} {} {} {} {} {}",
                    to_string(uuid()),
                    name(),
                    to_string(item_type()),
                    size(),
                    index,
                    event.dump(2));
            }
        } break;
        case IA_SPLICE: {
            // spdlog::warn("IT_SPLICE {}", event.dump(2));
            auto dst   = event.at("dst").get<size_t>();
            auto first = event.at("first").get<size_t>();
            if ((dst == 0 or dst <= size()) and (first == 0 or first < size())) {
                auto it1 = std::next(begin(), dst);
                auto it2 = std::next(begin(), first);
                auto it3 = std::next(it2, event.at("count").get<int>());

                splice_direct(it1, *this, it2, it3);
            } else {
                spdlog::error(
                    "IA_SPLICE - INVALID INDEX {} {} {} {}", size(), first, dst, event.dump(2));
            }
        } break;

        case IA_ADDR:
            if (event.at("value").is_null())
                set_actor_addr_direct(caf::actor_addr());
            else
                set_actor_addr_direct(string_to_actor_addr(event.at("value")));
            break;
        case IA_NONE:
        default:
            break;
        }
        if (item_post_event_callback_)
            item_post_event_callback_(event, *this);
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

// make copty safe.. ICK
void Item::unbind() {
    item_pre_event_callback_  = nullptr;
    item_post_event_callback_ = nullptr;
    for (auto &i : *this)
        i.unbind();
}

void Item::reset_actor(const bool recursive) {
    set_actor_addr_direct(caf::actor_addr());
    if (recursive)
        for (auto &i : *this)
            i.reset_actor(recursive);
}

void Item::bind_item_pre_event_func(ItemEventFunc fn, const bool recursive) {
    recursive_bind_pre_      = recursive;
    item_pre_event_callback_ = [fn](auto &&PH1, auto &&PH2) {
        return fn(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    };

    if (recursive_bind_pre_) {
        for (auto &i : *this)
            i.bind_item_pre_event_func(fn, recursive_bind_pre_);
    }
}

void Item::bind_item_post_event_func(ItemEventFunc fn, const bool recursive) {
    recursive_bind_post_      = recursive;
    item_post_event_callback_ = [fn](auto &&PH1, auto &&PH2) {
        return fn(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    };

    if (recursive_bind_post_) {
        for (auto &i : *this)
            i.bind_item_post_event_func(fn, recursive_bind_post_);
    }
}

std::optional<std::pair<Items::const_iterator, int>>
Item::item_at_frame(const int track_frame) const {
    auto start    = trimmed_frame_start().frames();
    auto duration = trimmed_frame_duration().frames();

    if (track_frame >= start or track_frame <= start + duration - 1) {
        // should be valid..
        // increment start til we find item.

        for (auto it = cbegin(); it != cend(); it++) {
            if (start + it->trimmed_frame_duration().frames() > track_frame) {
                return std::make_pair(
                    it, (track_frame - start) + it->trimmed_frame_start().frames());
            } else {
                start += it->trimmed_frame_duration().frames();
            }
        }
    }

    return {};
}

FrameRange Item::range_at_index(const int item_index) const {
    auto result = FrameRange();
    result.set_rate(trimmed_range().rate());

    auto start = trimmed_range().start();
    auto end =
        item_index >= static_cast<int>(size()) ? cend() : std::next(cbegin(), item_index);
    auto it = cbegin();

    for (; it != end; ++it)
        start += it->trimmed_range().duration();

    if (it != cend())
        result.set_duration(it->trimmed_range().duration());
    else
        result.set_duration(FrameRate());

    result.set_start(start);

    return result;
}


int Item::frame_at_index(const int item_index) const {
    int result = trimmed_frame_start().frames();
    auto end =
        item_index >= static_cast<int>(size()) ? cend() : std::next(cbegin(), item_index);

    for (auto it = cbegin(); it != end; ++it)
        result += it->trimmed_frame_duration().frames();

    return result;
}

int Item::frame_at_index(const int item_index, const int item_frame) const {
    auto dur = item_frame;

    if (item_index < static_cast<int>(size()) and item_index >= 0)
        dur -= std::next(cbegin(), item_index)->trimmed_frame_start().frames();

    return frame_at_index(item_index) + dur;
}

std::optional<int> Item::frame_at_item_frame(
    const Uuid &uuid, const int item_local_frame, const bool skip_disabled) const {
    std::vector<int> result;
    auto item          = find_item(children(), uuid);
    auto item_top_left = top_left(uuid);
    if (item_top_left && item && (!skip_disabled || (*item)->enabled())) {
        int f = item_local_frame - (*item)->trimmed_frame_start().frames();
        if (f >= 0 && f <= (*item)->trimmed_frame_duration().frames()) {
            return item_top_left->first + item_local_frame -
                   (*item)->trimmed_frame_start().frames();
        }
    }
    return {};
}

std::optional<Items::const_iterator> Item::item_at_index(const int index) const {
    if (index < 0 or index >= static_cast<int>(size()))
        return {};
    return std::next(begin(), index);
}


std::optional<timeline::Point> Item::top_left(const Uuid &_uuid) const {

    if (_uuid == uuid()) {
        return top_left();
    } else {
        auto b = box(_uuid);
        if (b)
            return b->first;
    }

    return {};
}

timeline::Point Item::top_left() const { return std::make_pair(0, 0); }

std::optional<timeline::Point> Item::bottom_right(const Uuid &_uuid) const {
    if (_uuid == uuid()) {
        return bottom_right();
    } else {
        auto b = box(_uuid);
        if (b)
            return b->second;
    }

    return {};
}

timeline::Point Item::bottom_right() const {
    auto y = 0;
    switch (item_type_) {
    case IT_TIMELINE:
        y = 1;
        if (not empty())
            y = front().bottom_right().second;
        break;

    case IT_STACK:
        // sum height of children.
        for (const auto &i : children())
            y += i.bottom_right().second;
        y = std::max(y, 1);
        break;

    case IT_AUDIO_TRACK:
    case IT_VIDEO_TRACK:
        // sum height of children.
        y = 1;
        for (const auto &i : children())
            y = std::max(y, i.bottom_right().second);
        break;

    default:
        y = 1;
        break;
    }

    return std::make_pair(trimmed_frame_duration().frames(), y);
}

std::optional<Box> Item::box(const Uuid &_uuid) const {
    if (_uuid == uuid()) {
        return box();
    } else {
        if (not empty()) {
            switch (item_type_) {
            case IT_TIMELINE:
                return front().box(_uuid);
                break;

            case IT_STACK:
                // we need to increment y
                {
                    int y = 0;
                    for (const auto &i : children()) {
                        auto b = i.box(_uuid);
                        if (b) {
                            b->first.second += y;
                            b->second.second += y;
                            return *b;
                            break;
                        }
                        y++;
                    }
                }
                break;

            case IT_AUDIO_TRACK:
            case IT_VIDEO_TRACK: {
                int x = 0;
                for (const auto &i : children()) {
                    auto b = i.box(_uuid);
                    if (b) {
                        b->first.first += x;
                        b->second.first += x;
                        return *b;
                        break;
                    }
                    x += i.trimmed_frame_duration().frames();
                }
            } break;

            case IT_GAP:
            case IT_CLIP:
            default:
                break;
            }
        }
    }

    return {};
}

Box Item::box() const { return std::make_pair(top_left(), bottom_right()); }

// merge gaps, prune trailing gaps, remove null clips.
void Item::merge_gaps(const bool purge_empty_clips) {
    auto previous = end();
    auto it       = begin();

    while (it != end()) {
        switch (it->item_type_) {
        case IT_GAP:
            if (previous != end() and previous->item_type_ == IT_GAP) {
                // merge..
                previous->set_available_range(FrameRange(
                    previous->trimmed_start(),
                    previous->trimmed_duration() + it->trimmed_duration(),
                    previous->rate()));
                previous->set_active_range(*(previous->available_range()));

                it = erase_direct(it);
            } else {
                previous = it;
                it++;
            }
            break;

        case IT_CLIP:
            if (it->prop().value("media_uuid", Uuid()).is_null() and purge_empty_clips) {
                if (previous != end() and previous->item_type_ == IT_GAP) {
                    previous->set_available_range(FrameRange(
                        previous->trimmed_start(),
                        previous->trimmed_duration() + it->trimmed_duration(),
                        previous->rate()));
                    previous->set_active_range(*(previous->available_range()));

                    it = erase_direct(it);
                } else {
                    auto gap = Item(IT_GAP, "Gap");
                    gap.set_available_range(
                        FrameRange(FrameRate(), it->trimmed_duration(), it->rate()));
                    gap.set_active_range(*(gap.available_range()));

                    previous = insert_direct(it, gap);
                    it       = erase_direct(it);
                }

            } else {
                previous = it;
                it++;
            }
            break;

        case IT_AUDIO_TRACK:
        case IT_VIDEO_TRACK:
        case IT_TIMELINE:
        // purge empty tracks from head and tail?
        case IT_STACK:
            it->merge_gaps(purge_empty_clips);
            previous = it;
            it++;
            break;

        default:
            previous = it;
            it++;
            break;
        }
    }

    // trim end gap
    if (not empty() and back().item_type_ == IT_GAP)
        erase_direct(std::next(begin(), size() - 1));
}

void Item::reset_uuid(const bool recursive) {
    set_uuid(Uuid::generate());
    for (auto &i : markers_)
        i.reset_uuid();

    if (recursive) {
        for (auto &i : children())
            i.reset_uuid(recursive);
    }
}

void Item::reset_media_uuid() {
    if (item_type_ == IT_CLIP) {
        if (not prop_.is_null())
            prop_["media_uuid"] = Uuid();
    } else {
        for (auto &i : children())
            i.reset_media_uuid();
    }
}

namespace xstudio::timeline {

/* Simple one-trick actor that enacts the playback loop. */
class BuildFrameIDsHelper : public caf::event_based_actor {

  public:
    BuildFrameIDsHelper(
        caf::actor_config &cfg,
        caf::typed_response_promise<media::FrameTimeMapPtr> rp,
        media::FrameTimeMap *r,
        FrameRate base_rate,
        const media::MediaType media_type)
        : caf::event_based_actor(cfg),
          rp_(rp),
          result_(r),
          base_rate_(base_rate),
          media_type_(media_type) {

        t0 = utility::clock::now();

        behavior_.assign([=](media::get_media_pointer_atom,
                             caf::actor clip_actor,
                             const std::vector<timebase::flicks> &clip_timepoints,
                             const timebase::flicks clip_timeline_position) {
            if (clip_timepoints.empty() ||
                result_->lower_bound(clip_timeline_position) == result_->end())
                return;

            mail(media::get_media_pointer_atom_v, media_type_, clip_timepoints, base_rate_)
                .request(clip_actor, infinite)
                .then(
                    [=](const media::AVFrameIDs &mps) mutable {
                        auto p = result_->lower_bound(clip_timeline_position);
                        for (const auto &mp : mps) {
                            p->second = mp;
                            p++;
                            if (p == result_->end())
                                break;
                        }
                    },
                    [=](error &err) mutable {});
        });
    }

    ~BuildFrameIDsHelper() {

        // When this actor is destroyed it means that all references to this
        // actor held in the CAF mailbox have expired. Therefore it has received
        // all messages and fulfilled all responses to the requests it has made.
        // We can deliver on the response promise now.
        rp_.deliver(media::FrameTimeMapPtr(result_));

        // spdlog::critical("BuildFrameIDsHelper milliseconds: {}",
        // std::chrono::duration_cast<std::chrono::milliseconds>(utility::clock::now()-t0).count());
    }

    caf::behavior behavior_;
    media::FrameTimeMap *result_;
    caf::typed_response_promise<media::FrameTimeMapPtr> rp_;
    FrameRate base_rate_;
    const media::MediaType media_type_;
    utility::clock::time_point t0;

    caf::behavior make_behavior() override { return behavior_; }
};
} // namespace xstudio::timeline

caf::typed_response_promise<media::FrameTimeMapPtr> Item::get_all_frame_IDs(
    const media::MediaType media_type,
    const TimeSourceMode tsm,
    const FrameRate & /*override_rate*/,
    const UuidSet &focus_list) {

    // This crucial function bakes a timeline into a 'FrameTimeMap' which is
    // a map of individual frame IDs against a zero based time point which is
    // what a playhead needs to do playback. Every time the timeline updates we
    // need to re-generate the FrameTimeMap. Therefore, to keep timeline
    // interaction snappy and interactive this algorithm must be really fast.

    auto local_actor = caf::actor_cast<caf::event_based_actor *>(actor());
    auto rp          = local_actor->make_response_promise<media::FrameTimeMapPtr>();

    if (!available_range()) {
        rp.deliver(media::FrameTimeMapPtr());
        return rp;
    }

    // First, get our frame range
    const int start_frame = available_range()->frame_start().frames(rate());
    const int end_frame   = start_frame + available_range()->frame_duration().frames(rate());

    timebase::flicks timepoint = start_frame * rate();

    // first step - we make a map of all frames in the timeline, and fill
    // with blank frames.
    media::FrameTimeMap *result = new media::FrameTimeMap;
    auto blank_frame            = media::make_blank_frame(media_type);
    timepoint                   = start_frame * rate();
    for (auto i = start_frame; i < end_frame; i++) {
        result->emplace(std::make_pair(FrameRate(timepoint), blank_frame));
        timepoint += rate();
    }

    // make a vector that tells us which frames in the frame time map have been
    // resolved. resolve_and_request_clip_frames will recurse through
    // video tracks - if a visible clip covers certain frames in the timeline
    // then it must mark the frames as resolved so that the video track that
    // is underneath doesn't try and overwrite those FrameIDs.
    std::vector<bool> resolved_frames(end_frame - start_frame, false);

    // now spawn a worker actor that will receive FrameIDs from individual clips
    // in the timeline and add them into 'result'
    auto helper = local_actor->spawn<BuildFrameIDsHelper>(rp, result, rate(), media_type);

    // recursively call down into the timeline tree (Timeline->Stack->Track->Clip)
    // When we hit clips we evaluate the frames that the clip contributes to the
    // entire timeline frames map and request the frameIDs from the clip. The
    // response is then handled by our helper

    if (!focus_list.empty()) {
        // if we have a list of focussed items, these must overwrite their frames
        // regardless whether they are 'underneath' other clips (i.e. in tracks
        // lower down in the stack). We do this by resolving frames for focussed
        // items first
        resolve_and_request_clip_frames(
            available_range()->start().to_flicks(),
            helper,
            resolved_frames,
            start_frame * rate(),
            rate(),
            media_type,
            focus_list,
            true // only print frames into map for focussed items
        );
    }

    resolve_and_request_clip_frames(
        available_range()->start().to_flicks(),
        helper,
        resolved_frames,
        start_frame * rate(),
        rate(),
        media_type,
        focus_list,
        false // print frames into map whether focussed or not
    );

    return rp;
}