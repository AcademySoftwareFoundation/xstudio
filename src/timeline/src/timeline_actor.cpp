// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include <opentimelineio/version.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/clip.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/track.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/deserialization.h>

#include <cpp-colors/colors.h>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark_actor.hpp"
#include "xstudio/history/history_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;

namespace {

auto __sysclock_now() {
#ifdef _MSC_VER
    /*auto tp = sysclock::now();
    return
    std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();*/
    return sysclock::now();
#elif defined(__linux__)
    return sysclock::now();
#elif defined(__apple__)
    return utility::clock::now();
#endif
}

const static auto COLOUR_JPOINTER  = nlohmann::json::json_pointer("/xstudio/colour");
const static auto LOCKED_JPOINTER  = nlohmann::json::json_pointer("/xstudio/locked");
const static auto CONFORM_JPOINTER = nlohmann::json::json_pointer("/xstudio/conform_track");
const static auto ENABLED_JPOINTER = nlohmann::json::json_pointer("/xstudio/enabled");
const static auto MEDIA_COLOUR_JPOINTER = nlohmann::json::json_pointer("/xstudio/media_colour");

} // namespace

caf::actor TimelineActor::deserialise(const JsonStore &value, const bool replace_item) {

    auto actor = caf::actor();

    if (value.at("base").at("container").at("type") == "Stack") {
        auto key  = Uuid(value.at("base").at("item").at("uuid"));
        auto item = Item();
        actor     = spawn<StackActor>(static_cast<JsonStore>(value), item);
        add_item(UuidActor(key, actor));
        if (replace_item) {
            auto itemit = find_uuid(base_.item().children(), key);

            if (itemit != base_.item().end()) {
                (*itemit) = item;
            } else {
                spdlog::warn(
                    "{} Invalid item to replace {} {}",
                    __PRETTY_FUNCTION__,
                    to_string(key),
                    value.dump(2));
            }
        }
    } else if (value.at("base").at("container").at("type") == "PlayheadSelection") {

        try {

            selection_actor_ = system().spawn<playhead::PlayheadSelectionActor>(
                static_cast<JsonStore>(value), caf::actor_cast<caf::actor>(this));
            link_to(selection_actor_);

        } catch (const std::exception &e) {
            spdlog::error("{}", e.what());
        }
    }

    return actor;
}

// trigger actor creation
void TimelineActor::item_post_event_callback(const JsonStore &event, Item &item) {

    switch (static_cast<ItemAction>(event.at("action"))) {
    case IA_INSERT: {
        auto cuuid = Uuid(event.at("item").at("uuid"));
        // spdlog::warn("{} {} {} {}", find_uuid(base_.item().children(), cuuid) !=
        // base_.item().cend(), actors_.count(cuuid), not event["blind"].is_null(),
        // event.dump(2)); needs to be child..
        auto child_item_it = find_uuid(base_.item().children(), cuuid);
        if (child_item_it != base_.item().cend() and not item_actors_.count(cuuid) and
            not event.at("blind").is_null()) {
            // our child
            // spdlog::warn("RECREATE MATCH");

            auto actor = deserialise(JsonStore(event.at("blind")), false);
            add_item(UuidActor(cuuid, actor));
            child_item_it = find_uuid(base_.item().children(), cuuid);
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(actor)));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));
            child_item_it->set_actor_addr(actor);
            // change item actor addr
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));

            // item actor_addr will be wrong.. in ancestors
            // send special update..
            mail(event_atom_v, item_atom_v, child_item_it->make_actor_addr_update(), true)
                .send(base_.event_group());
        }
        // spdlog::warn("TimelineActor IT_INSERT");
        // rebuilt child.. trigger relink
    } break;

    case IA_REMOVE: {
        auto cuuid = Uuid(event.at("item_uuid"));
        // child destroyed
        if (item_actors_.count(cuuid)) {
            // spdlog::warn("destroy
            // {}",to_string(caf::actor_cast<caf::actor_addr>(actors_[cuuid])));
            if (auto mit = monitor_.find(caf::actor_cast<caf::actor_addr>(item_actors_[cuuid]));
                mit != std::end(monitor_)) {
                mit->second.dispose();
                monitor_.erase(mit);
            }

            send_exit(item_actors_[cuuid], caf::exit_reason::user_shutdown);
            item_actors_.erase(cuuid);
        }
    } break;

    case IA_PROP:
    case IA_NONE:
    case IA_ENABLE:
    case IA_ADDR:
    case IA_RANGE:
    case IA_ACTIVE:
    case IA_AVAIL:
    case IA_SPLICE:
    case IA_NAME:
    case IA_FLAG:
    case IA_LOCK:
    default:
        break;
    }
}

void TimelineActor::item_pre_event_callback(const JsonStore &event, Item &item) {
    try {

        switch (static_cast<ItemAction>(event.at("action"))) {
        case IA_REMOVE: {
            auto cuuid = Uuid(event.at("item_uuid"));
            // spdlog::warn("{}", event.dump(2));
            // child destroyed
            if (item_actors_.count(cuuid)) {
            } else {
                // watch for clip deletion events
                // we'll want to check for media cleanup required.
                auto citem = item.item_at_index(event.value("index", 0));
                if (citem and (*citem)->item_type() == IT_CLIP) {
                    auto media_uuid = (*citem)->prop().value("media_uuid", Uuid());
                    // get a count of all references to this media..
                    // more than one then nothing to do.
                    auto media_clips = find_media_clips(base_.children(), media_uuid);
                    if (media_clips.size() == 1)
                        anon_mail(playlist::remove_media_atom_v, UuidVector({media_uuid}))
                            .delay(std::chrono::milliseconds(100))
                            .send(this);
                } else {
                    // we need to find all clip items under this item that's being removed.
                    // collect a map of all media_uuids about to be removed.
                    auto mmap  = std::map<Uuid, size_t>();
                    auto clips = (*citem)->find_all_items(IT_CLIP);
                    for (const auto &i : clips) {
                        auto media_uuid = i.get().prop().value("media_uuid", Uuid());
                        if (not media_uuid.is_null()) {
                            if (not mmap.count(media_uuid))
                                mmap[media_uuid] = 0;
                            mmap[media_uuid]++;
                        }
                    }

                    for (const auto &i : mmap) {
                        auto media_clips = find_media_clips(base_.children(), i.first);
                        if (media_clips.size() == i.second) {
                            anon_mail(playlist::remove_media_atom_v, UuidVector({i.first}))
                                .delay(std::chrono::milliseconds(500))
                                .send(this);
                        }
                    }
                }
            }
        } break;
        case IA_PROP: {
            const auto item_media_uuid     = item.prop().value("media_uuid", Uuid());
            const auto new_item_media_uuid = event.at("value").value("media_uuid", Uuid());
            if (not item_media_uuid.is_null() and item_media_uuid != new_item_media_uuid) {
                auto media_clips = find_media_clips(base_.children(), item_media_uuid);
                if (media_clips.size() == 1)
                    anon_mail(playlist::remove_media_atom_v, UuidVector({item_media_uuid}))
                        .delay(std::chrono::milliseconds(100))
                        .send(this);
            }
            // spdlog::warn("item_pre_event_callback {} {} {} {}",
            // to_string(item_media_uuid),to_string(new_item_media_uuid),item.name(),
            // event.dump(2));
        } break;

        case IA_INSERT: {
            auto new_item = Item(JsonStore(event.at("item")));
            if (new_item.item_type() == IT_CLIP) {
                auto media_uuid = new_item.prop().value("media_uuid", Uuid());
                // make sure media exists in timeline.
                if (media_uuid and not media_actors_.count(media_uuid)) {
                    // request media from playlist..
                    caf::scoped_actor sys(system());
                    try {
                        // get uuid..
                        auto mactor = request_receive<caf::actor>(
                            *sys,
                            caf::actor_cast<caf::actor>(playlist_),
                            playlist::get_media_atom_v,
                            media_uuid);
                        add_media(mactor, media_uuid, Uuid());
                        mail(
                            event_atom_v,
                            playlist::add_media_atom_v,
                            UuidActorVector({UuidActor(media_uuid, mactor)}))
                            .send(base_.event_group());
                        base_.send_changed();
                        mail(event_atom_v, change_atom_v).send(base_.event_group());
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            }
        } break;


        default:
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

namespace otio = opentimelineio::OPENTIMELINEIO_VERSION;

// std::vector<Retainer<Composable>> const& children() const noexcept
// {
//     return _children;
// }

// {
//     "OTIO_SCHEMA": "Marker.2",
//     "metadata": {
//         "fcp_xml": {
//             "comment": "1001,1017-1142,1158"
//         }
//     },
//     "name": "064_BMC_0020",
//     "color": "RED",
//     "marked_range": {
//         "OTIO_SCHEMA": "TimeRange.1",
//         "duration": {
//             "OTIO_SCHEMA": "RationalTime.1",
//             "rate": 24.0,
//             "value": 70.0
//         },
//         "start_time": {
//             "OTIO_SCHEMA": "RationalTime.1",
//             "rate": 24.0,
//             "value": 0.0
//         }
//     }
// }
std::vector<Marker> process_markers(
    const std::vector<otio::SerializableObject::Retainer<otio::Marker>> &markers,
    const FrameRate &timeline_rate) {
    auto result = std::vector<Marker>();

    for (const auto &om : markers) {
        auto m = Marker(om->name());

        if (colors::wpf_named_color_converter::is_named(om->color()))
            m.set_flag(colors::to_ahex_str<char>(colors::color(
                colors::value_of(colors::wpf_named_color_converter::value(om->color())))));

        auto marker_metadata = JsonStore();
        try {
            otio::ErrorStatus err;
            auto marker_metadata = nlohmann::json::parse(om->to_json_string(&err, {}, 0));
            if (marker_metadata.count("metadata")) {
                m.set_prop(JsonStore(marker_metadata.at("metadata")));
                if (m.prop().contains(COLOUR_JPOINTER)) {
                    m.set_flag(m.prop().at(COLOUR_JPOINTER).get<std::string>());
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }


        auto marked_range = om->marked_range();
        m.set_range(FrameRange(
            FrameRateDuration(
                static_cast<int>(marked_range.start_time().value()),
                FrameRate(fps_to_flicks(marked_range.start_time().rate()))),
            FrameRateDuration(
                static_cast<int>(marked_range.duration().value()),
                FrameRate(fps_to_flicks(marked_range.duration().rate())))));

        result.emplace_back(m);
    }

    return result;
}

void process_item(
    const std::vector<otio::SerializableObject::Retainer<otio::Composable>> &items,
    blocking_actor *self,
    const caf::actor &timeline_actor,
    caf::actor &parent,
    const caf::uri &path, // otio path
    const std::map<std::string, UuidActor> &media_lookup,
    const FrameRate &timeline_rate) {

    auto fcp_locked_path     = nlohmann::json::json_pointer("/fcp_xml/locked");
    auto fcp_enabled_path    = nlohmann::json::json_pointer("/fcp_xml/enabled");
    auto fcp_track_name_path = nlohmann::json::json_pointer("/fcp_xml/@MZ.TrackName");

    // let the fun begin..
    for (auto i : items) {
        if (auto ii = dynamic_cast<otio::Track *>(&(*i))) {
            // spdlog::warn("Track");
            auto media_type = media::MediaType::MT_IMAGE;
            if (ii->kind() == otio::Track::Kind::audio)
                media_type = media::MediaType::MT_AUDIO;

            auto locked        = false;
            auto enabled       = ii->enabled();
            auto name          = ii->name();
            auto metadata      = JsonStore();
            auto flag          = std::string();
            auto conform_track = false;

            try {
                // "fcp_xml": {
                //     "@MZ.TrackName": "Cut Ref QT",
                //     "@MZ.TrackTargeted": "1",
                //     "@TL.SQTrackExpanded": "0",
                //     "@TL.SQTrackExpandedHeight": "45",
                //     "@TL.SQTrackShy": "0",
                //     "enabled": "TRUE",
                //     "locked": "FALSE"
                //   }
                otio::ErrorStatus err;
                auto track_metadata = nlohmann::json::parse(ii->to_json_string(&err, {}, 0));

                if (track_metadata.count("metadata"))
                    metadata = JsonStore(track_metadata.at("metadata"));

                if (metadata.contains(fcp_locked_path) and
                    metadata.at(fcp_locked_path).get<std::string>() == "TRUE")
                    locked = true;

                if (metadata.contains(fcp_enabled_path) and
                    metadata.at(fcp_enabled_path).get<std::string>() == "FALSE")
                    enabled = false;

                if (metadata.contains(fcp_track_name_path) and
                    metadata.at(fcp_track_name_path).get<std::string>() != "")
                    name = metadata.at(fcp_track_name_path).get<std::string>();

                if (metadata.contains(COLOUR_JPOINTER))
                    flag = metadata.at(COLOUR_JPOINTER).get<std::string>();

                if (metadata.contains(LOCKED_JPOINTER))
                    locked = metadata.at(LOCKED_JPOINTER).get<bool>();

                if (metadata.contains(CONFORM_JPOINTER))
                    conform_track = metadata.at(CONFORM_JPOINTER).get<bool>();

                if (metadata.contains(ENABLED_JPOINTER))
                    enabled = metadata.at(ENABLED_JPOINTER).get<bool>();

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            auto uuid  = Uuid::generate();
            auto actor = self->spawn<TrackActor>(name, timeline_rate, media_type, uuid);

            if (locked)
                self->mail(item_lock_atom_v, locked)
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            if (conform_track) {
                self->mail(item_prop_atom_v)
                    .request(timeline_actor, infinite)
                    .receive(
                        [=](JsonStore &prop) {
                            prop["conform_track_uuid"] = uuid;
                            self->mail(item_prop_atom_v, prop)
                                .request(timeline_actor, infinite)
                                .receive([=](const JsonStore &) {}, [=](const error &err) {});
                        },
                        [=](const error &err) {});
            }

            if (not enabled)
                self->mail(plugin_manager::enable_atom_v, enabled)
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            if (not flag.empty())
                self->mail(item_flag_atom_v, flag)
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            anon_mail(item_prop_atom_v, metadata, "").send(actor);

            auto source_range = ii->source_range();
            if (source_range)
                self->mail(
                        active_range_atom_v,
                        FrameRange(FrameRateDuration(
                            static_cast<int>(source_range->duration().value()),
                            FrameRate(fps_to_flicks(source_range->duration().rate())))))
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            self->mail(insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, actor)}))
                .request(parent, infinite)
                .receive([=](const JsonStore &) {}, [=](const error &err) {});

            process_item(
                ii->children(), self, timeline_actor, actor, path, media_lookup, timeline_rate);
        } else if (auto ii = dynamic_cast<otio::Gap *>(&(*i))) {

            auto uuid = Uuid::generate();
            auto actor =
                self->spawn<GapActor>(ii->name(), FrameRateDuration(0, timeline_rate), uuid);
            auto source_range = ii->source_range();

            try {
                auto locked   = false;
                auto enabled  = ii->enabled();
                auto metadata = JsonStore();
                auto flag     = std::string();
                otio::ErrorStatus err;
                auto ometadata = nlohmann::json::parse(ii->to_json_string(&err, {}, 0));

                if (ometadata.count("metadata"))
                    metadata = JsonStore(ometadata.at("metadata"));

                if (metadata.contains(fcp_locked_path) and
                    metadata.at(fcp_locked_path).get<std::string>() == "TRUE")
                    locked = true;

                if (metadata.contains(fcp_enabled_path) and
                    metadata.at(fcp_enabled_path).get<std::string>() == "FALSE")
                    enabled = false;

                if (metadata.contains(COLOUR_JPOINTER))
                    flag = metadata.at(COLOUR_JPOINTER).get<std::string>();

                if (metadata.contains(LOCKED_JPOINTER))
                    locked = metadata.at(LOCKED_JPOINTER).get<bool>();

                if (metadata.contains(ENABLED_JPOINTER))
                    enabled = metadata.at(ENABLED_JPOINTER).get<bool>();

                if (locked)
                    self->mail(item_lock_atom_v, locked)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not enabled)
                    self->mail(plugin_manager::enable_atom_v, enabled)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not flag.empty())
                    self->mail(item_flag_atom_v, flag)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                anon_mail(item_prop_atom_v, metadata, "").send(actor);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            if (source_range)
                self->mail(
                        active_range_atom_v,
                        FrameRange(FrameRateDuration(
                            static_cast<int>(source_range->duration().value()),
                            FrameRate(fps_to_flicks(source_range->duration().rate())))))
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            self->mail(insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, actor)}))
                .request(parent, infinite)
                .receive([=](const JsonStore &) {}, [=](const error &err) {});

        } else if (auto ii = dynamic_cast<otio::Clip *>(&(*i))) {
            // spdlog::warn("Clip");
            // what does it contain ?
            auto uuid  = Uuid::generate();
            auto actor = caf::actor();

            const auto active_key = ii->active_media_reference_key();
            auto active_path      = std::string();

            if (auto active = otio::SerializableObject::Retainer<otio::ExternalReference>(
                    dynamic_cast<otio::ExternalReference *>(ii->media_reference()))) {
                active_path = active->target_url();
            } else if (
                auto active = otio::SerializableObject::Retainer<otio::ImageSequenceReference>(
                    dynamic_cast<otio::ImageSequenceReference *>(ii->media_reference()))) {
                active_path = active->target_url_base() + active->name_prefix() + "{:0" +
                              std::to_string(active->frame_zero_padding()) + "d}" +
                              active->name_suffix();
            }

            // active_path maybe relative..
            if (not active_path.empty() and not caf::make_uri(active_path)) {
                // not uri....
                // assume relative ?
                auto tmp       = uri_to_posix_path(path);
                auto const pos = tmp.find_last_of('/');
                active_path    = "file://" + tmp.substr(0, pos + 1) + active_path;
            }

            if (active_path.empty() or not media_lookup.count(active_path)) {
                // missing media..
                actor = self->spawn<ClipActor>(UuidActor(), ii->name(), uuid);
            } else {
                actor = self->spawn<ClipActor>(media_lookup.at(active_path), ii->name(), uuid);
            }

            try {
                auto locked     = false;
                auto enabled    = ii->enabled();
                auto metadata   = JsonStore();
                auto flag       = std::string();
                auto media_flag = std::string();
                otio::ErrorStatus err;
                auto ometadata = nlohmann::json::parse(ii->to_json_string(&err, {}, 0));
                if (ometadata.count("metadata"))
                    metadata = JsonStore(ometadata.at("metadata"));

                if (metadata.contains(fcp_locked_path) and
                    metadata.at(fcp_locked_path).get<std::string>() == "TRUE")
                    locked = true;

                if (metadata.contains(fcp_enabled_path) and
                    metadata.at(fcp_enabled_path).get<std::string>() == "FALSE")
                    enabled = false;

                if (metadata.contains(COLOUR_JPOINTER))
                    flag = metadata.at(COLOUR_JPOINTER).get<std::string>();

                if (metadata.contains(MEDIA_COLOUR_JPOINTER))
                    media_flag = metadata.at(MEDIA_COLOUR_JPOINTER).get<std::string>();

                if (metadata.contains(LOCKED_JPOINTER))
                    locked = metadata.at(LOCKED_JPOINTER).get<bool>();

                if (metadata.contains(ENABLED_JPOINTER))
                    enabled = metadata.at(ENABLED_JPOINTER).get<bool>();

                if (locked)
                    self->mail(item_lock_atom_v, locked)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not enabled)
                    self->mail(plugin_manager::enable_atom_v, enabled)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not flag.empty())
                    self->mail(item_flag_atom_v, flag)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not metadata.is_null())
                    anon_mail(item_prop_atom_v, metadata, "").send(actor);

                // set media colour
                if (not media_flag.empty() and not active_path.empty() and
                    media_lookup.count(active_path)) {
                    self->mail(
                            playlist::reflag_container_atom_v,
                            std::tuple<std::optional<std::string>, std::optional<std::string>>(
                                media_flag, {}))
                        .request(media_lookup.at(active_path).actor(), infinite)
                        .receive([=](const bool) {}, [=](const error &err) {});
                }

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            auto source_range = ii->source_range();
            if (source_range) {
                self->mail(
                        active_range_atom_v,
                        FrameRange(
                            FrameRateDuration(
                                static_cast<int>(source_range->start_time().value()),
                                FrameRate(fps_to_flicks(source_range->start_time().rate()))),
                            FrameRateDuration(
                                static_cast<int>(source_range->duration().value()),
                                FrameRate(fps_to_flicks(source_range->duration().rate())))))
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});
            }

            self->mail(insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, actor)}))
                .request(parent, infinite)
                .receive([=](const JsonStore &) {}, [=](const error &err) {});

        } else if (auto ii = dynamic_cast<otio::Stack *>(&(*i))) {
            // spdlog::warn("Stack");
            // timeline where marker live..
            auto uuid = Uuid::generate();
            // spdlog::warn("SPAWN stack {}", timeline_rate.to_fps());
            auto actor = self->spawn<StackActor>(ii->name(), timeline_rate, uuid);

            if (not ii->enabled())
                self->mail(plugin_manager::enable_atom_v, false)
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            auto markers = process_markers(ii->markers(), timeline_rate);

            if (not markers.empty())
                anon_mail(item_marker_atom_v, insert_item_atom_v, markers).send(actor);

            try {
                auto locked   = false;
                auto enabled  = ii->enabled();
                auto metadata = JsonStore();
                auto flag     = std::string();
                otio::ErrorStatus err;
                auto ometadata = nlohmann::json::parse(ii->to_json_string(&err, {}, 0));
                if (ometadata.count("metadata"))
                    metadata = JsonStore(ometadata.at("metadata"));

                if (metadata.contains(fcp_locked_path) and
                    metadata.at(fcp_locked_path).get<std::string>() == "TRUE")
                    locked = true;

                if (metadata.contains(fcp_enabled_path) and
                    metadata.at(fcp_enabled_path).get<std::string>() == "FALSE")
                    enabled = false;

                if (metadata.contains(COLOUR_JPOINTER))
                    flag = metadata.at(COLOUR_JPOINTER).get<std::string>();

                if (metadata.contains(LOCKED_JPOINTER))
                    locked = metadata.at(LOCKED_JPOINTER).get<bool>();

                if (metadata.contains(ENABLED_JPOINTER))
                    enabled = metadata.at(ENABLED_JPOINTER).get<bool>();

                if (locked)
                    self->mail(item_lock_atom_v, locked)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not enabled)
                    self->mail(plugin_manager::enable_atom_v, enabled)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                if (not flag.empty())
                    self->mail(item_flag_atom_v, flag)
                        .request(actor, infinite)
                        .receive([=](const JsonStore &) {}, [=](const error &err) {});

                anon_mail(item_prop_atom_v, metadata, "").send(actor);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            auto source_range = ii->source_range();

            if (source_range)
                self->mail(
                        active_range_atom_v,
                        FrameRange(FrameRateDuration(
                            static_cast<int>(source_range->duration().value()),
                            FrameRate(fps_to_flicks(source_range->duration().rate())))))
                    .request(actor, infinite)
                    .receive([=](const JsonStore &) {}, [=](const error &err) {});

            self->mail(insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, actor)}))
                .request(parent, infinite)
                .receive([=](const JsonStore &) {}, [=](const error &err) {});

            process_item(
                ii->children(),
                self,
                timeline_actor,
                parent,
                path,
                media_lookup,
                timeline_rate);
        }
    }
}

void timeline_importer(
    blocking_actor *self,
    caf::typed_response_promise<bool> rp,
    const caf::actor &playlist,
    const UuidActor &dst,
    const caf::uri &path,
    const std::string &data) {

    otio::ErrorStatus error_status;
    otio::SerializableObject::Retainer<otio::Timeline> timeline;

    // notify processing..
    auto notify_uuid = Uuid();
    {
        auto notify = Notification::ProgressRangeNotification("Loading timeline");
        anon_mail(notification_atom_v, notify).send(dst.actor());
        notify_uuid = notify.uuid();
    }

    timeline = otio::SerializableObject::Retainer<otio::Timeline>(
        dynamic_cast<otio::Timeline *>(otio::Timeline::from_json_string(data, &error_status)));

    if (otio::is_error(error_status)) {
        auto failnotify = Notification::WarnNotification("Failed Loading timeline");
        failnotify.uuid(notify_uuid);
        anon_mail(notification_atom_v, failnotify).send(dst.actor());

        return rp.deliver(false);
    }

    // timeline loaded, convert to native timeline.
    //  iterate over media, and add to playlist.
    const std::vector<otio::SerializableObject::Retainer<otio::Clip>> clips =
        (timeline->find_clips());

    auto timeline_rate        = FrameRate();
    auto timeline_start_frame = 0;

    if (timeline->global_start_time()) {
        timeline_rate        = FrameRate(fps_to_flicks(timeline->global_start_time()->rate()));
        timeline_start_frame = static_cast<int>(timeline->global_start_time()->value());
    } else {
        // need to determine timeline rate somehow..
        // rate of first clip ?
        for (const auto &cl : clips) {
            auto source_range = cl->source_range();
            if (source_range) {
                timeline_rate = FrameRate(fps_to_flicks(source_range->start_time().rate()));
                break;
            }
        }
    }

    {
        auto notify =
            Notification::ProgressRangeNotification("Loading timeline", 0, 0, clips.size());
        notify.uuid(notify_uuid);
        anon_mail(notification_atom_v, notify).send(dst.actor());
    }


    self->mail(
            available_range_atom_v,
            FrameRange(
                FrameRateDuration(timeline_start_frame, timeline_rate),
                FrameRateDuration(0, timeline_rate)))
        .request(dst.actor(), infinite)
        .receive([=](const JsonStore &) {}, [=](const error &err) {});

    auto timeline_metadata = JsonStore(R"({})"_json);
    try {
        otio::ErrorStatus err;
        auto metadata = nlohmann::json::parse(timeline->to_json_string(&err, {}, 0));
        if (metadata.count("metadata"))
            timeline_metadata = metadata.at("metadata");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    timeline_metadata["path"] = to_string(path);

    anon_mail(item_prop_atom_v, timeline_metadata).send(dst.actor());

    // this maps Media actors to the url for the active media ref. We use this
    // to check if a clip can re-use Media that we already have, or whether we
    // need to create a new Media item.
    std::map<std::string, UuidActor> existing_media_url_map;
    std::map<std::string, UuidActor> target_url_map;

    spdlog::info("Processing {} clips", clips.size());
    // fill our map with media that's already in the parent playlist
    self->mail(playlist::get_media_atom_v)
        .request(playlist, infinite)
        .receive(
            [&](const std::vector<UuidActor> &all_media_in_playlist) mutable {
                for (auto &media : all_media_in_playlist) {
                    self->mail(media::media_reference_atom_v, Uuid())
                        .request(media.actor(), infinite)
                        .receive(
                            [&](const std::pair<Uuid, MediaReference> &ref) mutable {
                                existing_media_url_map[uri_to_posix_path(ref.second.uri())] =
                                    media;
                            },
                            [=](error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                }
            },
            [=](error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); });

    float count = 0.0;
    for (const auto &cl : clips) {
        count++;
        anon_mail(notification_atom_v, notify_uuid, count).send(dst.actor());

        const auto &name = cl->name();
        // spdlog::warn("{} {}", name, cl->active_media_reference_key());

        const auto active_key = cl->active_media_reference_key();
        auto active_path      = std::string();

        if (auto active = otio::SerializableObject::Retainer<otio::ExternalReference>(
                dynamic_cast<otio::ExternalReference *>(cl->media_reference()))) {
            active_path = active->target_url();
        } else if (
            auto active = otio::SerializableObject::Retainer<otio::ImageSequenceReference>(
                dynamic_cast<otio::ImageSequenceReference *>(cl->media_reference()))) {

            active_path = active->target_url_base() + active->name_prefix() + "{:0" +
                          std::to_string(active->frame_zero_padding()) + "d}" +
                          active->name_suffix();
        }

        // active_path maybe relative..
        if (not caf::make_uri(active_path)) {
            // not uri....
            // assume relative ?
            auto tmp       = uri_to_posix_path(path);
            auto const pos = tmp.find_last_of('/');
            active_path    = "file://" + tmp.substr(0, pos + 1) + active_path;
        }

        // WARNING this may inadvertantly skip auxiliary sources we want..
        if (active_path.empty() or target_url_map.count(active_path)) {
            // spdlog::warn("SKIP {}", active_path);
            continue;
        } else if (existing_media_url_map.count(active_path)) {
            // spdlog::warn("SKIP EXISTING {}", active_path);
            target_url_map[active_path] = existing_media_url_map[active_path];
            continue;
        }

        auto clip_metadata = JsonStore();
        try {
            otio::ErrorStatus err;
            auto clip_meta = nlohmann::json::parse(cl->to_json_string(&err, {}, 0));
            if (clip_meta.count("metadata"))
                clip_metadata = JsonStore(clip_meta.at("metadata"));

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        // check we're not adding the same media twice.
        UuidActorVector sources;

        // create media sources.
        for (const auto &mr : cl->media_references()) {
            // try and dynamic cast to
            if (auto ext = otio::SerializableObject::Retainer<otio::ExternalReference>(
                    dynamic_cast<otio::ExternalReference *>(mr.second))) {

                auto uri = caf::make_uri(ext->target_url());
                if (!uri) {
                    // not uri....
                    // assume relative ?
                    auto tmp       = uri_to_posix_path(path);
                    auto const pos = tmp.find_last_of('/');
                    uri = posix_path_to_uri(tmp.substr(0, pos + 1) + ext->target_url());
                }

                if (uri) {
                    auto extname     = ext->name();
                    auto source_uuid = Uuid::generate();
                    auto rate        = FrameRate();
                    auto ar          = ext->available_range();
                    if (ar) {
                        rate = FrameRate(fps_to_flicks(ar->start_time().rate()));
                    }

                    auto source_metadata = JsonStore();
                    try {
                        otio::ErrorStatus err;
                        auto ext_meta = nlohmann::json::parse(ext->to_json_string(&err, {}, 0));
                        if (ext_meta.count("metadata")) {
                            source_metadata = JsonStore(ext_meta.at("metadata"));
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }

                    auto source = self->spawn<media::MediaSourceActor>(
                        extname.empty() ? std::string("ExternalReference") : extname,
                        *uri,
                        rate,
                        source_uuid);

                    if (not source_metadata.is_null())
                        anon_mail(
                            json_store::set_json_atom_v, source_metadata, "/metadata/timeline")
                            .send(source);

                    sources.emplace_back(UuidActor(source_uuid, source));
                }
            } else if (
                auto ext = otio::SerializableObject::Retainer<otio::ImageSequenceReference>(
                    dynamic_cast<otio::ImageSequenceReference *>(mr.second))) {

                auto uri = caf::make_uri(
                    ext->target_url_base() + ext->name_prefix() + "{:0" +
                    std::to_string(ext->frame_zero_padding()) + "d}" + ext->name_suffix());
                if (!uri) {
                    uri = posix_path_to_uri(
                        ext->target_url_base() + ext->name_prefix() + "{:0" +
                        std::to_string(ext->frame_zero_padding()) + "d}" + ext->name_suffix());
                }
                if (uri) {
                    auto extname     = ext->name();
                    auto source_uuid = Uuid::generate();
                    auto rate        = FrameRate();
                    auto ar          = ext->available_range();
                    if (ar) {
                        rate = FrameRate(fps_to_flicks(ar->start_time().rate()));
                    }

                    auto source_metadata = JsonStore();
                    try {
                        otio::ErrorStatus err;
                        auto ext_meta = nlohmann::json::parse(ext->to_json_string(&err, {}, 0));
                        if (ext_meta.count("metadata")) {
                            source_metadata = JsonStore(ext_meta.at("metadata"));
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }

                    auto source = self->spawn<media::MediaSourceActor>(
                        extname.empty() ? std::string("ImageSequenceReference") : extname,
                        *uri,
                        FrameList(
                            std::to_string(ext->start_frame()) + "-" +
                            std::to_string(ext->end_frame())),
                        rate,
                        source_uuid);

                    if (not source_metadata.is_null())
                        anon_mail(
                            json_store::set_json_atom_v, source_metadata, "/metadata/timeline")
                            .send(source);

                    sources.emplace_back(UuidActor(source_uuid, source));
                }
            }
        }

        // //  add media.
        if (not sources.empty()) {
            // create media
            // add to map.
            auto uuid = Uuid::generate();
            target_url_map[active_path] =
                UuidActor(uuid, self->spawn<media::MediaActor>(name, uuid, sources));

            if (not clip_metadata.is_null())
                anon_mail(json_store::set_json_atom_v, clip_metadata, "/metadata/timeline")
                    .send(target_url_map[active_path].actor());

            anon_mail(media::current_media_source_atom_v, sources.front().uuid())
                .send(target_url_map[active_path].actor());
        }

        otio::RationalTime dur = cl->duration();
        // std::cout << "Name: " << cl->name() << " [";
        // std::cout << dur.value() << "/" << dur.rate() << "]" << std::endl;
        // trigger population of additional sources ? May conflict with timeline ?
    }

    // populate source
    if (not target_url_map.empty()) {
        // batch add..
        UuidActorVector new_media;

        for (const auto &i : target_url_map)
            new_media.push_back(i.second);

        // trigger additional sources.
        UuidList media_uuids;
        for (const auto &i : target_url_map) {
            media_uuids.push_back(i.second.uuid());
        }

        // add to playlist/timeline
        self->mail(playlist::add_media_atom_v, new_media, Uuid())
            .request(dst.actor(), infinite)
            .receive([=](const UuidActor &) {}, [=](const error &err) {});
    }

    // process timeline.
    caf::actor history_actor;

    self->mail(history::history_atom_v)
        .request(dst.actor(), infinite)
        .receive(
            [&](const UuidActor &ua) mutable { history_actor = ua.actor(); },
            [=](error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); });

    // disable history whilst populating
    // should we clear it ?
    if (history_actor) {
        anon_mail(plugin_manager::enable_atom_v, false).send(history_actor);
    }

    // build timeline, for fun and profit..
    // purge any current timeline..
    std::vector<otio::SerializableObject::Retainer<otio::Composable>> tracks;

    auto vtracks = timeline->video_tracks();
    for (auto it = vtracks.rbegin(); it != vtracks.rend(); ++it)
        tracks.emplace_back(otio::SerializableObject::Retainer<otio::Composable>(*it));

    auto atracks = timeline->audio_tracks();
    for (auto &atrack : atracks)
        tracks.emplace_back(otio::SerializableObject::Retainer<otio::Composable>(atrack));

    // get timeline stack..
    auto stack_actor = caf::actor();

    self->mail(item_atom_v, 0)
        .request(dst.actor(), infinite)
        .receive(
            [&](const Item &item) mutable { stack_actor = item.actor(); },
            [=](error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); });

    if (stack_actor) {
        // set stack actor rate via avaliable range
        self->mail(
                available_range_atom_v,
                FrameRange(
                    FrameRateDuration(0, timeline_rate), FrameRateDuration(0, timeline_rate)))
            .request(stack_actor, infinite)
            .receive([=](const JsonStore &) {}, [=](const error &err) {});

        // process markers on top level stack..
        auto stack = timeline->tracks();

        auto markers = process_markers(stack->markers(), timeline_rate);

        if (not markers.empty())
            anon_mail(item_marker_atom_v, insert_item_atom_v, markers).send(stack_actor);

        process_item(
            tracks, self, dst.actor(), stack_actor, path, target_url_map, timeline_rate);
    }

    // enable history, we've finished.
    if (history_actor) {
        anon_mail(plugin_manager::enable_atom_v, true).send(history_actor);
    }

    // spdlog::warn("imported");

    auto successnotify = Notification::InfoNotification("Timeline Loaded");
    successnotify.uuid(notify_uuid);
    anon_mail(notification_atom_v, successnotify).send(dst.actor());

    rp.deliver(true);
}

TimelineActor::TimelineActor(
    caf::actor_config &cfg, const JsonStore &jsn, const caf::actor &playlist)
    : caf::event_based_actor(cfg),
      base_(static_cast<JsonStore>(jsn["base"])),
      playlist_(playlist ? caf::actor_cast<caf::actor_addr>(playlist) : caf::actor_addr()) {
    base_.item().set_actor_addr(this);
    // parse and generate tracks/stacks.

    if (playlist)
        anon_mail(playhead::source_atom_v, playlist, UuidUuidMap()).send(this);

    if (jsn.contains("playhead")) {
        playhead_serialisation_ = jsn["playhead"];
    }

    jsn_handler_ = json_store::JsonStoreHandler(
        dynamic_cast<caf::event_based_actor *>(this),
        base_.event_group(),
        Uuid::generate(),
        not jsn.count("store") or jsn["store"].is_null()
            ? JsonStore()
            : static_cast<JsonStore>(jsn["store"]));

    for (const auto &[key, value] : jsn["actors"].items()) {
        try {
            deserialise(value, true);
        } catch (const std::exception &e) {
            spdlog::error("{}", e.what());
        }
    }

    base_.item().bind_item_pre_event_func(
        [this](const JsonStore &event, Item &item) { item_pre_event_callback(event, item); },
        true);
    base_.item().bind_item_post_event_func(
        [this](const JsonStore &event, Item &item) { item_post_event_callback(event, item); });


    init();
}

TimelineActor::TimelineActor(
    caf::actor_config &cfg,
    const std::string &name,
    const FrameRate &rate,
    const Uuid &uuid,
    const caf::actor &playlist,
    const bool with_tracks)
    : caf::event_based_actor(cfg),
      base_(name, rate, uuid, this),
      playlist_(playlist ? caf::actor_cast<caf::actor_addr>(playlist) : caf::actor_addr()) {

    // create default stack
    auto stack_item = Item(IT_STACK, "Stack", rate);

    if (with_tracks) {
        stack_item.insert(stack_item.end(), Item(IT_VIDEO_TRACK, "Video Track", rate));
        stack_item.insert(stack_item.end(), Item(IT_AUDIO_TRACK, "Audio Track", rate));
        auto vuuid                 = stack_item.front().uuid();
        auto pjsn                  = R"({})"_json;
        pjsn["conform_track_uuid"] = vuuid;
        base_.item().set_prop(JsonStore(pjsn));
    }

    auto stack = spawn<StackActor>(stack_item, stack_item);

    base_.item().set_name(name);

    add_item(UuidActor(stack_item.uuid(), stack));
    base_.item().insert(base_.item().begin(), stack_item);
    base_.item().refresh();

    base_.item().bind_item_pre_event_func(
        [this](const JsonStore &event, Item &item) { item_pre_event_callback(event, item); },
        true);
    base_.item().bind_item_post_event_func(
        [this](const JsonStore &event, Item &item) { item_post_event_callback(event, item); });

    jsn_handler_ = json_store::JsonStoreHandler(
        dynamic_cast<caf::event_based_actor *>(this), base_.event_group());

    init();
}

caf::message_handler TimelineActor::default_event_handler() {
    return caf::message_handler(
               {
                   [=](event_atom, item_atom, const Item &) {},
                   [=](event_atom, item_atom, const JsonStore &, const bool) {},
               })
        .or_else(NotificationHandler::default_event_handler())
        .or_else(json_store::JsonStoreHandler::default_event_handler())
        .or_else(Container::default_event_handler());
}


caf::message_handler TimelineActor::message_handler() {
    return caf::message_handler{
        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](history::history_atom) -> UuidActor { return UuidActor(history_uuid_, history_); },

        [=](link_media_atom, const bool force) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (item_actors_.empty()) {
                rp.deliver(true);
            } else {
                // pool direct children for state.
                fan_out_request<policy::select_all>(
                    map_value_to_vec(item_actors_),
                    infinite,
                    link_media_atom_v,
                    media_actors_,
                    force)
                    .await(
                        [=](std::vector<bool> items) mutable { rp.deliver(true); },
                        [=](error &err) mutable {
                            spdlog::warn(
                                "{} {} {}",
                                __PRETTY_FUNCTION__,
                                to_string(err),
                                base_.item().name());
                            rp.deliver(false);
                        });
            }

            return rp;
        },

        [=](link_media_atom, const UuidActorMap &media, const bool force) -> result<bool> {
            auto rp = make_response_promise<bool>();
            if (item_actors_.empty()) {
                rp.deliver(true);
            } else {
                // pool direct children for state.
                fan_out_request<policy::select_all>(
                    map_value_to_vec(item_actors_), infinite, link_media_atom_v, media, force)
                    .await(
                        [=](std::vector<bool> items) mutable { rp.deliver(true); },
                        [=](error &err) mutable { rp.deliver(err); });
            }

            return rp;
        },


        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null()) {
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
                anon_mail(history::log_atom_v, __sysclock_now(), jsn).send(history_);
            }
            return jsn;
        },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_lock_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_locked(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value, const bool merge) -> JsonStore {
            auto p = base_.item().prop();
            p.update(value);
            auto jsn = base_.item().set_prop(p);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value) -> JsonStore {
            auto jsn = base_.item().set_prop(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value, const std::string &path) -> JsonStore {
            auto prop = base_.item().prop();
            try {
                auto ptr = nlohmann::json::json_pointer(path);
                prop.at(ptr).update(value);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            auto jsn = base_.item().set_prop(prop);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom) -> JsonStore { return base_.item().prop(); },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null()) {
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
                anon_mail(history::log_atom_v, __sysclock_now(), jsn).send(history_);
            }
            return jsn;
        },

        [=](active_range_atom) -> std::optional<FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> FrameRange { return base_.item().trimmed_range(); },

        [=](item_atom) -> Item { return base_.item().clone(); },

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null()) {
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
                anon_mail(history::log_atom_v, __sysclock_now(), jsn).send(history_);
            }
            return jsn;
        },

        [=](item_atom, int index) -> result<Item> {
            if (static_cast<size_t>(index) >= base_.item().size()) {
                return make_error(xstudio_error::error, "Invalid index");
            }
            auto it = base_.item().cbegin();
            std::advance(it, index);
            return (*it).clone();
        },

        // search for item in children.
        [=](item_atom, const Uuid &id) -> result<Item> {
            auto item = find_item(base_.item().children(), id);
            if (item)
                return (**item).clone();

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](event_atom, change_atom, const bool) {
            content_changed_ = false;
            // mail(event_atom_v, item_atom_v, base_.item()).send(base_.event_group());
            mail(event_atom_v, change_atom_v).send(base_.event_group());
            mail(event_atom_v, change_atom_v).send(change_event_group_);


            // Ted - TODO - this WIP stuff allows comparing of video tracks within
            // a timeline.
            auto video_tracks = base_.item().find_all_uuid_actors(IT_VIDEO_TRACK, true);
            if (video_tracks != video_tracks_) {
                video_tracks_ = video_tracks;
                if (playhead_) {
                    // now set this (and its video tracks) as the source for
                    // the timeple playhead

                    anon_mail(
                        playhead::source_atom_v,
                        UuidActor(base_.uuid(), caf::actor_cast<caf::actor>(this)),
                        video_tracks_)
                        .send(playhead_.actor());
                }
            }
        },

        [=](event_atom, change_atom) {
            if (not content_changed_) {
                content_changed_ = true;
                mail(event_atom_v, change_atom_v, true)
                    .delay(std::chrono::milliseconds(50))
                    .send(this);
            }
        },

        // check events processes
        [=](item_atom, event_atom, const std::set<Uuid> &events) -> bool {
            auto result = true;
            for (const auto &i : events) {
                if (not events_processed_.contains(i)) {
                    result = false;
                    break;
                }
            }
            return result;
        },

        // handle child change events.
        [=](event_atom, item_atom, const JsonStore &update, const bool hidden) {
            auto event_ids = base_.item().update(update);
            if (not event_ids.empty()) {
                events_processed_.insert(event_ids.begin(), event_ids.end());
                auto more = base_.item().refresh();
                if (not more.is_null()) {
                    more.insert(more.begin(), update.begin(), update.end());
                    mail(event_atom_v, item_atom_v, more, hidden).send(base_.event_group());
                    if (not hidden)
                        anon_mail(history::log_atom_v, __sysclock_now(), more).send(history_);

                    mail(event_atom_v, change_atom_v).send(this);
                    return;
                }
            }

            mail(event_atom_v, item_atom_v, update, hidden).send(base_.event_group());
            if (not hidden)
                anon_mail(history::log_atom_v, __sysclock_now(), update).send(history_);

            if (base_.item().has_dirty(update))
                mail(event_atom_v, change_atom_v).send(this);
        },

        // loop with timepoint
        [=](history::undo_atom, const sys_time_point &key) -> result<bool> {
            auto rp = make_response_promise<bool>();

            mail(history::undo_atom_v, key)
                .request(history_, infinite)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::undo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        // loop with timepoint
        [=](history::redo_atom, const sys_time_point &key) -> result<bool> {
            auto rp = make_response_promise<bool>();

            mail(history::redo_atom_v, key)
                .request(history_, infinite)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::redo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::undo_atom, const sys_time_duration &duration) -> result<bool> {
            auto rp = make_response_promise<bool>();
            mail(history::undo_atom_v, duration)
                .request(history_, infinite)
                .then(
                    [=](const std::vector<JsonStore> &hist) mutable {
                        auto count = std::make_shared<size_t>(0);
                        for (const auto &h : hist) {
                            mail(history::undo_atom_v, h)
                                .request(caf::actor_cast<caf::actor>(this), infinite)
                                .then(
                                    [=](const bool) mutable {
                                        (*count)++;
                                        if (*count == hist.size()) {
                                            rp.deliver(true);
                                            mail(event_atom_v, change_atom_v).send(this);
                                        }
                                    },
                                    [=](const error &err) mutable {
                                        (*count)++;
                                        if (*count == hist.size()) {
                                            rp.deliver(true);
                                            mail(event_atom_v, change_atom_v).send(this);
                                        }
                                    });
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::redo_atom, const sys_time_duration &duration) -> result<bool> {
            auto rp = make_response_promise<bool>();
            mail(history::redo_atom_v, duration)
                .request(history_, infinite)
                .then(
                    [=](const std::vector<JsonStore> &hist) mutable {
                        auto count = std::make_shared<size_t>(0);
                        for (const auto &h : hist) {
                            mail(history::redo_atom_v, h)
                                .request(caf::actor_cast<caf::actor>(this), infinite)
                                .then(
                                    [=](const bool) mutable {
                                        (*count)++;
                                        if (*count == hist.size()) {
                                            rp.deliver(true);
                                            mail(event_atom_v, change_atom_v).send(this);
                                        }
                                    },
                                    [=](const error &err) mutable {
                                        (*count)++;
                                        if (*count == hist.size()) {
                                            rp.deliver(true);
                                            mail(event_atom_v, change_atom_v).send(this);
                                        }
                                    });
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::undo_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            mail(history::undo_atom_v)
                .request(history_, infinite)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::undo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::redo_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            mail(history::redo_atom_v)
                .request(history_, infinite)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::redo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::undo_atom, const JsonStore &hist) -> result<bool> {
            auto rp = make_response_promise<bool>();

            base_.item().undo(hist);

            auto inverted = R"([])"_json;
            for (auto it = hist.crbegin(); it != hist.crend(); ++it) {
                auto ev    = R"({})"_json;
                ev["redo"] = it->at("undo");
                ev["undo"] = it->at("redo");
                inverted.emplace_back(ev);
            }

            // mail(event_atom_v, item_atom_v, JsonStore(inverted),
            // true).send(base_.event_group());

            if (not item_actors_.empty()) {
                // push to children..
                fan_out_request<policy::select_all>(
                    map_value_to_vec(item_actors_), infinite, history::undo_atom_v, hist)
                    .await(
                        [=](std::vector<bool> updated) mutable {
                            anon_mail(link_media_atom_v, media_actors_, false).send(this);
                            mail(event_atom_v, item_atom_v, JsonStore(inverted), true)
                                .send(base_.event_group());
                            rp.deliver(true);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                mail(event_atom_v, item_atom_v, JsonStore(inverted), true)
                    .send(base_.event_group());
                rp.deliver(true);
            }
            return rp;
        },

        [=](history::redo_atom, const JsonStore &hist) -> result<bool> {
            auto rp = make_response_promise<bool>();
            base_.item().redo(hist);

            // mail(event_atom_v, item_atom_v, hist, true).send(base_.event_group());

            if (not item_actors_.empty()) {
                // push to children..
                fan_out_request<policy::select_all>(
                    map_value_to_vec(item_actors_), infinite, history::redo_atom_v, hist)
                    .await(
                        [=](std::vector<bool> updated) mutable {
                            rp.deliver(true);
                            anon_mail(link_media_atom_v, media_actors_, false).send(this);

                            mail(event_atom_v, item_atom_v, hist, true)
                                .send(base_.event_group());
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                mail(event_atom_v, item_atom_v, hist, true).send(base_.event_group());
                rp.deliver(true);
            }

            return rp;
        },

        [=](bake_atom, const UuidVector &uuids) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            bake(rp, UuidSet(uuids.begin(), uuids.end()));

            return rp;
        },

        [=](bake_atom,
            const FrameRate &time,
            const FrameRate &duration) -> result<std::vector<std::optional<ResolvedItem>>> {
            auto result = std::vector<std::optional<ResolvedItem>>();

            for (auto i = time; i <= time + duration; i += base_.item().rate()) {
                result.emplace_back(base_.item().resolve_time(
                    i, media::MediaType::MT_IMAGE, base_.focus_list()));
            }

            return result;
        },

        [=](bake_atom, const FrameRate &time) -> result<ResolvedItem> {
            auto ri =
                base_.item().resolve_time(time, media::MediaType::MT_IMAGE, base_.focus_list());
            if (ri)
                return *ri;

            return make_error(xstudio_error::error, "No clip resolved");
        },

        [=](item_selection_atom) -> UuidActorVector { return selection_; },

        [=](item_type_atom) -> ItemType { return base_.item().item_type(); },

        [=](item_selection_atom, const UuidActorVector &selection) { selection_ = selection; },

        [=](media_frame_to_timeline_frames_atom,
            const Uuid &media_uuid,
            const int mediaFrame,
            const bool skipDisabled) -> std::vector<int> {
            std::vector<int> result;
            auto media_clips = find_media_clips(base_.children(), media_uuid);
            for (const auto &clip_uuid : media_clips) {
                auto frame =
                    base_.item().frame_at_item_frame(clip_uuid, mediaFrame, skipDisabled);
                if (frame) {
                    result.push_back(*frame);
                }
            }
            return result;
        },

        [=](insert_item_atom,
            const int index,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            if (not base_.item().empty() or uav.size() > 1)
                rp.deliver(make_error(xstudio_error::error, "Only one child allowed"));
            else
                insert_items(rp, index, uav);
            return rp;
        },

        [=](insert_item_atom,
            const Uuid &before_uuid,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            if (not base_.item().empty() or uav.size() > 1)
                rp.deliver(make_error(xstudio_error::error, "Only one child allowed"));
            else {

                auto index = base_.item().size();
                // find index. for uuid
                if (not before_uuid.is_null()) {
                    auto it = find_uuid(base_.item().children(), before_uuid);
                    if (it == base_.item().end())
                        rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
                    else
                        index = std::distance(base_.item().begin(), it);
                }

                if (rp.pending())
                    insert_items(rp, index, uav);
            }

            return rp;
        },

        [=](remove_item_atom,
            const int index,
            const bool) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index);
            return rp;
        },

        [=](remove_item_atom, const int index, const int count, const bool)
            -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index, count);
            return rp;
        },

        // delegate to deep child..
        [=](remove_item_atom, const Uuid &uuid, const bool add_gap, const bool)
            -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();

            auto actor = find_parent_actor(base_.item(), uuid);

            if (not actor)
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
            else
                rp.delegate(actor, remove_item_atom_v, uuid, add_gap);

            return rp;
        },

        [=](remove_item_atom,
            const Uuid &uuid,
            const bool) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                remove_items(rp, std::distance(base_.item().begin(), it));

            return rp;
        },

        [=](erase_item_atom, const int index, const bool) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index);
            return rp;
        },

        [=](erase_item_atom, const int index, const int count, const bool)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index, count);
            return rp;
        },

        // delegate to deep child..
        [=](erase_item_atom, const Uuid &uuid, const bool add_gap, const bool)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto actor = find_parent_actor(base_.item(), uuid);

            if (not actor)
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
            else
                rp.delegate(actor, erase_item_atom_v, uuid, add_gap);

            return rp;
        },

        [=](erase_item_atom, const Uuid &uuid, const bool) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                erase_items(rp, std::distance(base_.item().begin(), it));

            return rp;
        },

        // emulate subset
        [=](playlist::sort_by_media_display_info_atom,
            const int sort_column_index,
            const bool ascending) { sort_by_media_display_info(sort_column_index, ascending); },

        [=](duration_atom, const timebase::flicks &new_duration) -> bool {
            // attempt by playhead to force trim the duration (to support compare
            // modes for sources of different lenght). Here we ignore it.
            return false;
        },

        // [=](media::get_media_pointer_atom,
        //     const media::MediaType media_type,
        //     const int logical_frame) -> result<media::AVFrameID> {
        //     // get actors attached to our media..
        //     if (not base_.empty()) {
        //         auto rp = make_response_promise<media::AVFrameID>();
        //         deliver_media_pointer(rp, logical_frame, media_type);
        //         return rp;
        //     }

        //     return result<media::AVFrameID>(make_error(xstudio_error::error, "No media"));
        // },

        [=](playlist::get_media_uuid_atom) -> UuidVector { return base_.media_vector(); },

        [=](playlist::add_media_atom atom,
            const caf::uri &path,
            const bool recursive,
            const Uuid &uuid_before) -> result<std::vector<UuidActor>> {
            auto rp = make_response_promise<std::vector<UuidActor>>();
            mail(atom, path, recursive, uuid_before)
                .request(caf::actor_cast<caf::actor>(playlist_), infinite)
                .then(
                    [=](const std::vector<UuidActor> new_media) mutable {
                        for (const auto &m : new_media) {
                            add_media(m.actor(), m.uuid(), uuid_before);
                        }
                        mail(event_atom_v, playlist::add_media_atom_v, new_media)
                            .send(base_.event_group());
                        base_.send_changed();
                        mail(event_atom_v, change_atom_v).send(base_.event_group());
                        mail(event_atom_v, change_atom_v).send(change_event_group_);
                        rp.deliver(new_media);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](playlist::add_media_atom,
            const UuidActorVector &uav,
            const Uuid &before_uuid,
            const bool duplicate) -> bool {
            // add internally
            for (const auto &i : uav) {
                add_media(i.actor(), i.uuid(), before_uuid);
            }

            return true;
        },

        [=](playlist::add_media_atom,
            const UuidActorVector &uav,
            const Uuid &before_uuid) -> result<bool> {
            auto rp = make_response_promise<bool>();

            // add internally
            for (const auto &i : uav) {
                add_media(i.actor(), i.uuid(), before_uuid);
            }

            // dispatch to playlist
            mail(playlist::add_media_atom_v, uav, Uuid())
                .request(caf::actor_cast<caf::actor>(playlist_), infinite)
                .then(
                    [=](const bool) mutable {
                        rp.deliver(true);

                        anon_mail(
                            media_hook::gather_media_sources_atom_v, vector_to_uuid_vector(uav))
                            .send(caf::actor_cast<caf::actor>(playlist_));

                        // just one vent to trigger rebuild ?
                        mail(
                            event_atom_v,
                            playlist::add_media_atom_v,
                            UuidActorVector({UuidActor(uav[0].uuid(), uav[0].actor())}))
                            .send(base_.event_group());

                        base_.send_changed();
                        mail(event_atom_v, change_atom_v).send(base_.event_group());
                        mail(event_atom_v, change_atom_v).send(change_event_group_);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](session::media_rate_atom atom) {
            return mail(atom).delegate(caf::actor_cast<caf::actor>(playlist_));
        },

        [=](playlist::add_media_atom,
            const UuidActor &ua,
            const Uuid &before_uuid) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            add_media(rp, ua, before_uuid);
            return rp;
        },

        [=](playlist::add_media_atom,
            const Uuid &uuid,
            const Uuid &before_uuid) -> result<bool> {
            // get actor from playlist..
            auto rp = make_response_promise<bool>();

            mail(playlist::get_media_atom_v, uuid)
                .request(caf::actor_cast<actor>(playlist_), infinite)
                .then(
                    [=](caf::actor actor) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this),
                            playlist::add_media_atom_v,
                            uuid,
                            actor,
                            before_uuid);
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(false);
                    });

            return rp;
        },

        [=](playlist::add_media_atom,
            const Uuid &uuid,
            const caf::actor &actor,
            const Uuid &before_uuid) -> bool {
            try {
                add_media(actor, uuid, before_uuid);
                mail(
                    event_atom_v,
                    playlist::add_media_atom_v,
                    UuidActorVector({UuidActor(uuid, actor)}))
                    .send(base_.event_group());
                base_.send_changed();
                mail(event_atom_v, change_atom_v).send(base_.event_group());
                mail(event_atom_v, change_atom_v).send(change_event_group_);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            return true;
        },

        [=](playlist::add_media_atom,
            caf::actor actor,
            const Uuid &before_uuid) -> result<bool> {
            auto rp = make_response_promise<bool>();

            caf::scoped_actor sys(system());
            try {
                // get uuid..
                Uuid uuid = request_receive<Uuid>(*sys, actor, uuid_atom_v);
                // check playlist owns it..
                request_receive<caf::actor>(
                    *sys,
                    caf::actor_cast<caf::actor>(playlist_),
                    playlist::get_media_atom_v,
                    uuid);

                rp.delegate(
                    caf::actor_cast<caf::actor>(this),
                    playlist::add_media_atom_v,
                    uuid,
                    actor,
                    before_uuid);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                rp.deliver(false);
            }

            return rp;
        },

        [=](playlist::add_media_atom atom,
            UuidActor ua, // the MediaActor
            const UuidList &final_ordered_uuid_list,
            Uuid before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            // This handler lets us build a playlist of a given order, even when
            // we add the media in an out of order way. We generate Uuids for the
            // MediaActors that will be used to build the playlist *before* the
            // actual MediaActors are built and added - we pass in the ordered
            // list of Uuids when adding each MediaActor so we can insert it
            // into the correct point as the playlist is being built.
            //
            // This is used in ShotgunDataSourceActor, for example

            const UuidList media = base_.media();

            // get an iterator to the uuid of the next MediaActor item that has
            // been (or will be) added to this playlist
            auto next = std::find(
                final_ordered_uuid_list.begin(), final_ordered_uuid_list.end(), ua.uuid());
            if (next != final_ordered_uuid_list.end())
                next++;

            while (next != final_ordered_uuid_list.end()) {

                // has 'next' already been added to this playlist?
                auto q = std::find(media.begin(), media.end(), *next);
                if (q != media.end()) {
                    // yes - we know where to insert the incoming MediaActor
                    before = *q;
                    break;
                }
                // keep looking
                next++;
            }

            // Note we can't use mail(add_media_atom_v, ua, before)
            // to enact the adding, because it might happen *after* we get
            // another of these add_media messages which would then mess up the
            // ordering algorithm
            add_media(rp, ua, before);
            return rp;
        },

        [=](playlist::filter_media_atom, const std::string &filter_string) -> result<UuidList> {
            // for each media item in the playlist, check if any fields in the
            // 'media display info' for the media matches filter_string. If
            // it does, include it in the result. We have to do some awkward
            // shenanegans thanks to async requests ... the usual stuff!!

            if (filter_string.empty())
                return base_.media();

            auto rp = make_response_promise<UuidList>();
            // share ptr to store result for each media piece (in order)
            auto vv = std::make_shared<std::vector<Uuid>>(base_.media().size());
            // keep count of responses with this
            auto ct                        = std::make_shared<int>(base_.media().size());
            const auto filter_string_lower = to_lower(filter_string);

            auto check_deliver = [=]() mutable {
                (*ct)--;
                if (*ct == 0) {
                    UuidList r;
                    for (const auto &v : *vv) {
                        if (!v.is_null()) {
                            r.push_back(v);
                        }
                    }
                    rp.deliver(r);
                }
            };

            int idx = 0;
            for (const auto &uuid : base_.media()) {
                if (!media_actors_.count(uuid)) {
                    check_deliver();
                    continue;
                }
                UuidActor media(uuid, media_actors_[uuid]);
                mail(media::media_display_info_atom_v)
                    .request(media.actor(), infinite)
                    .then(
                        [=](const JsonStore &media_display_info) mutable {
                            if (media_display_info.is_array()) {
                                for (int i = 0; i < media_display_info.size(); ++i) {
                                    std::string data = media_display_info[i].dump();
                                    if (to_lower(data).find(filter_string_lower) !=
                                        std::string::npos) {
                                        (*vv)[idx] = media.uuid();
                                        break;
                                    }
                                }
                            }
                            check_deliver();
                        },
                        [=](caf::error &err) mutable { check_deliver(); });
                idx++;
            }
            return rp;
        },

        [=](playlist::get_next_media_atom,
            const Uuid &after_this_uuid,
            int skip_by,
            const std::string &filter_string) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            mail(playlist::filter_media_atom_v, filter_string)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const UuidList &media) mutable {
                        if (skip_by > 0) {
                            auto i = std::find(media.begin(), media.end(), after_this_uuid);
                            if (i == media.end()) {
                                // not found!
                                rp.deliver(make_error(
                                    xstudio_error::error,
                                    fmt::format(
                                        "playlist::get_next_media_atom called with uuid that "
                                        "is not in "
                                        "timeline {}",
                                        to_string(after_this_uuid))));
                            }
                            while (skip_by--) {
                                i++;
                                if (i == media.end()) {
                                    i--;
                                    break;
                                }
                            }
                            if (media_actors_.count(*i))
                                rp.deliver(UuidActor(*i, media_actors_[*i]));

                        } else {
                            auto i = std::find(media.rbegin(), media.rend(), after_this_uuid);
                            if (i == media.rend()) {
                                // not found!
                                rp.deliver(make_error(
                                    xstudio_error::error,
                                    fmt::format(
                                        "playlist::get_next_media_atom called with uuid that "
                                        "is not in "
                                        "playlist",
                                        to_string(after_this_uuid))));
                            }
                            while (skip_by++) {
                                i++;
                                if (i == media.rend()) {
                                    i--;
                                    break;
                                }
                            }

                            if (media_actors_.count(*i))
                                rp.deliver(UuidActor(*i, media_actors_[*i]));
                        }
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });


            return rp;
        },

        [=](global_store::save_atom,
            const caf::uri &path,
            const std::string &type,
            const std::string &schema) -> result<bool> {
            auto rp = make_response_promise<bool>();

            mail(session::export_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const std::string &result) {
                        export_otio(rp, result, path, type, schema);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](session::export_atom) -> result<std::string> {
            // convert timeline to otio string
            auto rp = make_response_promise<std::string>();
            export_otio_as_string(rp);
            return rp;
        },

        [=](playlist::create_playhead_atom) -> UuidActor {
            if (playhead_)
                return playhead_;

            auto uuid = Uuid::generate();

            // N.B. for now we're not using the 'selection_actor_' as this
            // drives the playhead a list of selected media which the playhead
            // will play. It will ignore this timeline completely if we do that.
            // We want to play this timeline, not the media in the timeline
            // that is selected.
            auto playhead_actor = spawn<playhead::PlayheadActor>(
                std::string("Timeline Playhead"),
                playhead::GLOBAL_AUDIO,
                selection_actor_,
                uuid,
                caf::actor_cast<caf::actor_addr>(this));

            link_to(playhead_actor);

            anon_mail(playhead::playhead_rate_atom_v, base_.rate()).send(playhead_actor);

            // now make this timeline and its vide tracks the source for the playhead
            video_tracks_ = base_.item().find_all_uuid_actors(IT_VIDEO_TRACK, true);
            anon_mail(
                playhead::source_atom_v,
                UuidActor(base_.uuid(), caf::actor_cast<caf::actor>(this)),
                video_tracks_)
                .send(playhead_actor);

            playhead_ = UuidActor(uuid, playhead_actor);
            if (!playhead_serialisation_.is_null()) {
                anon_mail(module::deserialise_atom_v, playhead_serialisation_)
                    .send(playhead_.actor());
            }
            return playhead_;
        },

        [=](playlist::get_playhead_atom) {
            return mail(playlist::create_playhead_atom_v)
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](playlist::get_change_event_group_atom) -> caf::actor {
            return change_event_group_;
        },

        [=](playlist::get_media_atom, const bool) -> result<std::vector<ContainerDetail>> {
            std::vector<caf::actor> actors;
            // only media actors..
            for (const auto &i : base_.media())
                actors.push_back(media_actors_[i]);

            if (not actors.empty()) {
                auto rp = make_response_promise<std::vector<ContainerDetail>>();
                // collect media data..

                fan_out_request<policy::select_all>(actors, infinite, detail_atom_v)
                    .then(
                        [=](const std::vector<ContainerDetail> details) mutable {
                            std::vector<ContainerDetail> reordered_details;

                            for (const auto &i : base_.media()) {
                                for (const auto &ii : details) {
                                    if (ii.uuid_ == i) {
                                        reordered_details.push_back(ii);
                                        break;
                                    }
                                }
                            }

                            rp.deliver(reordered_details);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }
            std::vector<ContainerDetail> result;
            return result;
        },

        [=](playlist::get_media_atom, const Uuid &uuid) -> result<caf::actor> {
            if (base_.contains_media(uuid))
                return media_actors_[uuid];
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](playlist::get_media_atom) -> UuidActorVector {
            UuidActorVector result;

            for (const auto &i : base_.media())
                result.emplace_back(UuidActor(i, media_actors_[i]));

            return result;
        },

        [=](playlist::selection_actor_atom) -> caf::actor { return selection_actor_; },

        [=](playlist::move_media_atom atom, const Uuid &uuid, const Uuid &uuid_before) {
            return mail(atom, UuidVector({uuid}), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](playlist::move_media_atom atom,
            const UuidList &media_uuids,
            const Uuid &uuid_before) {
            return mail(atom, UuidVector(media_uuids.begin(), media_uuids.end()), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](playlist::move_media_atom,
            const UuidVector &media_uuids,
            const Uuid &uuid_before) -> bool {
            bool result = false;
            for (auto uuid : media_uuids) {
                result |= base_.move_media(uuid, uuid_before);
            }
            if (result) {
                base_.send_changed();
                mail(event_atom_v, playlist::move_media_atom_v, media_uuids, uuid_before)
                    .send(base_.event_group());
                mail(event_atom_v, change_atom_v).send(base_.event_group());
                mail(event_atom_v, change_atom_v).send(change_event_group_);
            }
            return result;
        },

        [=](playlist::remove_media_atom atom, const UuidList &uuids) {
            return mail(atom, UuidVector(uuids.begin(), uuids.end()))
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](playlist::remove_media_atom atom, const Uuid &uuid) {
            return mail(atom, UuidVector({uuid})).delegate(actor_cast<caf::actor>(this));
        },

        [=](media::current_media_source_atom)
            -> caf::result<std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>> {
            auto rp = make_response_promise<
                std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>>();
            if (not media_actors_.empty()) {
                fan_out_request<policy::select_all>(
                    map_value_to_vec(media_actors_),
                    infinite,
                    media::current_media_source_atom_v)
                    .then(
                        [=](const std::vector<
                            std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>
                                details) mutable { rp.deliver(details); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.deliver(
                    std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>());
            }
            return rp;
        },

        [=](playlist::remove_media_atom, const UuidVector &uuids) -> bool {
            // this needs to propergate to children somehow..
            UuidVector removed;

            for (const auto &uuid : uuids) {
                if (media_actors_.count(uuid) and remove_media(media_actors_[uuid], uuid)) {
                    removed.push_back(uuid);

                    for (const auto &i : find_media_clips(base_.children(), uuid)) {
                        // find parent actor of clip and remove..
                        auto pa = find_parent_actor(base_.item(), i);
                        if (pa)
                            anon_mail(erase_item_atom_v, i, true).send(pa);
                    }
                }
            }

            if (not removed.empty()) {
                mail(event_atom_v, change_atom_v).send(base_.event_group());
                mail(event_atom_v, playlist::remove_media_atom_v, removed)
                    .send(base_.event_group());
                mail(event_atom_v, change_atom_v).send(change_event_group_);
                base_.send_changed();
            }
            return not removed.empty();
        },
        [=](clear_atom) -> bool {
            base_.clear();
            for (const auto &i : item_actors_) {
                // this->leave(i.second);
                unlink_from(i.second);
                send_exit(i.second, caf::exit_reason::user_shutdown);
            }
            item_actors_.clear();
            return true;
        },

        [=](rate_atom) -> FrameRate { return base_.item().rate(); },

        [=](rate_atom atom, const media::MediaType media_type) {
            return mail(atom).delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](rate_atom,
            const utility::FrameRate &new_rate,
            const bool force_media_rate_to_match) -> bool { return true; },

        [=](duplicate_atom) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            try {
                // clone ourself..
                caf::scoped_actor sys(system());

                auto conform_track_uuid =
                    base_.item().prop().value("conform_track_uuid", Uuid());
                auto video_tracks        = base_.item().find_all_uuid_actors(IT_VIDEO_TRACK);
                auto conform_track_index = video_tracks.size() - 1;

                if (not conform_track_uuid.is_null()) {
                    for (size_t i = 0; i < video_tracks.size(); i++) {
                        if (video_tracks[i].uuid() == conform_track_uuid) {
                            conform_track_index = i;
                            break;
                        }
                    }
                }

                JsonStore jsn;
                auto dup = base_.duplicate();
                dup.item().clear();

                // reset conform track uuid
                auto tprop                  = dup.item().prop();
                tprop["conform_track_uuid"] = Uuid();
                dup.item().set_prop(tprop);

                jsn["base"]   = dup.serialise();
                jsn["actors"] = {};
                auto actor    = spawn<TimelineActor>(jsn, caf::actor());


                auto meta = request_receive<JsonStore>(
                    *sys, jsn_handler_.json_actor(), json_store::get_json_atom_v);
                request_receive<bool>(*sys, actor, json_store::set_json_atom_v, meta);

                auto hactor = request_receive<UuidActor>(*sys, actor, history::history_atom_v);
                anon_mail(plugin_manager::enable_atom_v, false).send(hactor.actor());

                for (const auto &i : base_.children()) {
                    auto ua = request_receive<UuidActor>(
                        *sys, item_actors_[i.uuid()], duplicate_atom_v);
                    request_receive<JsonStore>(
                        *sys, actor, insert_item_atom_v, -1, UuidActorVector({ua}));
                }

                // actor should be populated ?
                // find uuid of video track..
                auto new_item = request_receive<Item>(*sys, actor, item_atom_v);
                auto track = (*(new_item.item_at_index(0)))->item_at_index(conform_track_index);
                if (track) {
                    tprop["conform_track_uuid"] = (*track)->uuid();
                    anon_mail(item_prop_atom_v, tprop).send(actor);
                }

                // enable history
                anon_mail(plugin_manager::enable_atom_v, true).send(hactor.actor());

                rp.deliver(UuidActor(dup.uuid(), actor));

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                rp.deliver(make_error(xstudio_error::error, err.what()));
            }

            return rp;
        },

        [=](focus_atom) -> UuidVector {
            auto tmp = base_.focus_list();
            return UuidVector(tmp.begin(), tmp.end());
        },

        [=](focus_atom, const UuidVector &list) {
            base_.set_focus_list(list);
            // both ?
            mail(event_atom_v, change_atom_v).send(base_.event_group());
            mail(event_atom_v, change_atom_v).send(change_event_group_);
        },

        [=](playhead::source_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(playlist_);
        },

        // set source (playlist), triggers relinking
        [=](playhead::source_atom,
            caf::actor playlist,
            const UuidUuidMap &swap) -> result<bool> {
            // spdlog::warn("playhead::source_atom old {} new {}", to_string(playlist_),
            // to_string(playlist)); for(const auto &i: swap)
            //     spdlog::warn("{} {}", to_string(i.first), to_string(i.second));

            auto rp = make_response_promise<bool>();

            for (const auto &i : media_actors_) {
                if (auto mit = monitor_.find(caf::actor_cast<caf::actor_addr>(i.second));
                    mit != std::end(monitor_)) {
                    mit->second.dispose();
                    monitor_.erase(mit);
                }
            }
            media_actors_.clear();

            playlist_ = caf::actor_cast<actor_addr>(playlist);

            mail(playlist::get_media_atom_v)
                .request(playlist, infinite)
                .then(
                    [=](const std::vector<UuidActor> &media) mutable {
                        // build map
                        UuidActorMap amap;
                        for (const auto &i : media)
                            amap[i.uuid()] = i.actor();

                        bool clean = false;
                        while (not clean) {
                            clean = true;
                            for (const auto &i : base_.media()) {
                                auto ii = (swap.count(i) ? swap.at(i) : i);
                                if (not amap.count(ii)) {
                                    spdlog::error(
                                        "Failed to find media in playlist {}", to_string(ii));
                                    base_.remove_media(i);
                                    clean = false;
                                    break;
                                }
                            }
                        }
                        // link
                        for (const auto &i : base_.media()) {
                            auto ii = (swap.count(i) ? swap.at(i) : i);
                            if (ii != i) {
                                base_.swap_media(i, ii);
                            }
                            media_actors_[ii] = amap[ii];
                            monitor_media(amap[ii]);
                        }

                        // for(const auto &i: actors_)
                        //     spdlog::warn("{} {}", to_string(i.first),to_string(i.second));

                        // timeline has only one child, the stack (we hope)
                        // relink all clips..
                        fan_out_request<policy::select_all>(
                            map_value_to_vec(item_actors_),
                            infinite,
                            playhead::source_atom_v,
                            swap,
                            media_actors_)
                            .await(
                                [=](std::vector<bool> items) mutable {
                                    base_.send_changed();
                                    rp.deliver(true);
                                },
                                [=](error &err) mutable {
                                    spdlog::warn(
                                        "{} {} {}",
                                        __PRETTY_FUNCTION__,
                                        to_string(err),
                                        base_.item().name());
                                    rp.deliver(false);
                                });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](duration_atom, const int) {},

        [=](media::get_media_pointers_atom atom,
            const media::MediaType media_type,
            const TimeSourceMode tsm,
            const FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {
            // This is required by SubPlayhead actor to make the timeline
            // playable.
            return base_.item().get_all_frame_IDs(
                media_type, tsm, override_rate, base_.focus_list());
        },

        [=](serialise_atom) -> result<JsonStore> {
            std::vector actors = map_value_to_vec(item_actors_);
            actors.push_back(selection_actor_);
            auto rp = make_response_promise<JsonStore>();

            // get json.
            mail(json_store::get_json_atom_v, "")
                .request(jsn_handler_.json_actor(), infinite)
                .then(
                    [=](const JsonStore &meta) mutable {
                        fan_out_request<policy::select_all>(actors, infinite, serialise_atom_v)
                            .then(
                                [=](std::vector<JsonStore> json) mutable {
                                    JsonStore jsn;
                                    jsn["base"]   = base_.serialise();
                                    jsn["store"]  = meta;
                                    jsn["actors"] = {};
                                    for (const auto &j : json)
                                        jsn["actors"][static_cast<std::string>(
                                            j["base"]["container"]["uuid"])] = j;
                                    if (playhead_) {
                                        mail(serialise_atom_v)
                                            .request(playhead_.actor(), infinite)
                                            .then(
                                                [=](const JsonStore &playhead_state) mutable {
                                                    playhead_serialisation_ = playhead_state;
                                                    jsn["playhead"]         = playhead_state;
                                                    rp.deliver(jsn);
                                                },
                                                [=](caf::error &err) mutable {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                    rp.deliver(jsn);
                                                });

                                    } else {
                                        if (!playhead_serialisation_.is_null()) {
                                            jsn["playhead"] = playhead_serialisation_;
                                        }
                                        rp.deliver(jsn);
                                    }
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },


        // ***********************************************************
        //
        // The following message handlers mirror PlaylistSelectionActor
        // and allow this actor to mimic this actor so that it works
        // with the playhead
        //
        // ***********************************************************

        [=](playhead::delete_selection_from_playlist_atom) {},

        [=](media_hook::gather_media_sources_atom) {},

        [=](playhead::evict_selection_from_cache_atom) -> media::MediaKeyVector {
            return media::MediaKeyVector();
        },

        [=](playhead::get_selection_atom) -> UuidList { return UuidList{base_.uuid()}; },

        [=](playhead::get_selection_atom, caf::actor requester) {
            anon_mail(event_atom_v, playhead::selection_changed_atom_v, UuidList{base_.uuid()})
                .send(requester);
        },

        [=](playhead::select_next_media_atom, const int skip_by) {},

        [=](playlist::select_all_media_atom) {},

        [=](playlist::select_media_atom, const UuidList &media_uuids) {},

        [=](playlist::select_media_atom) {},

        [=](playlist::select_media_atom, Uuid media_uuid) {},

        [=](playhead::get_selected_sources_atom) -> UuidActorVector {
            return UuidActorVector();
        },

        [=](session::get_playlist_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(playlist_);
        },

        [=](session::import_atom, const caf::uri &path, const std::string &data, bool clear)
            -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (clear && base_.item().size()) {
                // send a clear atom to the stack actor.
                auto stack_item = *(base_.item().begin());
                mail(clear_atom_v)
                    .request(stack_item.actor(), infinite)
                    .then(
                        [=](bool) mutable {
                            rp.delegate(
                                caf::actor_cast<caf::actor>(this),
                                session::import_atom_v,
                                path,
                                data);
                        },
                        [=](caf::error &err) mutable { rp.deliver(err); });
            } else {
                rp.delegate(
                    caf::actor_cast<caf::actor>(this), session::import_atom_v, path, data);
            }
            return rp;
        },

        [=](session::import_atom,
            const caf::uri &path,
            const std::string &data) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // purge timeline.. ?
            spawn(
                timeline_importer,
                rp,
                caf::actor_cast<caf::actor>(playlist_),
                UuidActor(base_.uuid(), actor_cast<caf::actor>(this)),
                path,
                data);
            return rp;
        }};
}


void TimelineActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    change_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(change_event_group_);

    history_uuid_ = Uuid::generate();
    history_      = spawn<history::HistoryMapActor<sys_time_point, JsonStore>>(history_uuid_);
    link_to(history_);

    if (!selection_actor_) {
        selection_actor_ = spawn<playhead::PlayheadSelectionActor>(
            "TimelinePlayheadSelectionActor", caf::actor_cast<caf::actor>(this));
        link_to(selection_actor_);
    }

    // set_down_handler([=](down_msg &msg) {
    //     // find in playhead list..
    //     for (auto it = std::begin(media_actors_); it != std::end(media_actors_); ++it) {
    //         // if a child dies we won't have enough information to recreate it.
    //         // we still need to report it up the chain though.

    //         if (msg.source == it->second) {
    //             demonitor(it->second);

    //             // if media..
    //             if (base_.remove_media(it->first)) {
    //                 mail(event_atom_v, change_atom_v).send(base_.event_group());
    //                 mail(
    //                     event_atom_v,
    //                     playlist::remove_media_atom_v,
    //                     UuidVector({it->first}))
    //                     .send(base_.event_group());
    //                 base_.send_changed();
    //             }

    //             media_actors_.erase(it);
    //         }
    //     }
    //     auto it = find_actor_addr(base_.item().children(), msg.source);

    //     if (it != base_.item().end()) {
    //         auto jsn  = base_.item().erase(it);
    //         auto more = base_.item().refresh();
    //         if (not more.is_null())
    //             jsn.insert(jsn.begin(), more.begin(), more.end());

    //         mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
    //     }
    // });

    // update_edit_list_ = true;
}

void TimelineActor::monitor_media(const caf::actor &actor) {
    auto act_addr = caf::actor_cast<caf::actor_addr>(actor);

    if (auto it = monitor_.find(act_addr); it == std::end(monitor_)) {
        monitor_[act_addr] = monitor(actor, [this, addr = actor.address()](const error &) {
            // remove monitored
            if (auto it = monitor_.find(caf::actor_cast<caf::actor_addr>(addr));
                it != std::end(monitor_))
                monitor_.erase(it);

            // find it
            for (auto it = std::begin(media_actors_); it != std::end(media_actors_); ++it) {
                // if a child dies we won't have enough information to recreate it.
                // we still need to report it up the chain though.

                if (addr == it->second) {
                    // if media..
                    if (base_.remove_media(it->first)) {
                        mail(event_atom_v, change_atom_v).send(base_.event_group());
                        mail(
                            event_atom_v,
                            playlist::remove_media_atom_v,
                            UuidVector({it->first}))
                            .send(base_.event_group());
                        base_.send_changed();
                    }

                    media_actors_.erase(it);
                    break;
                }
            }
        });
    }
}


void TimelineActor::add_item(const UuidActor &ua) {
    // join_event_group(this, ua.second);
    scoped_actor sys{system()};

    try {
        auto grp    = request_receive<caf::actor>(*sys, ua.actor(), get_event_group_atom_v);
        auto joined = request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, this);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    auto act_addr = caf::actor_cast<caf::actor_addr>(ua.actor());

    if (auto it = monitor_.find(act_addr); it == std::end(monitor_)) {
        monitor_[act_addr] =
            monitor(ua.actor(), [this, addr = ua.actor().address()](const error &) {
                if (auto it = monitor_.find(caf::actor_cast<caf::actor_addr>(addr));
                    it != std::end(monitor_))
                    monitor_.erase(it);

                for (auto it = std::begin(item_actors_); it != std::end(item_actors_); ++it) {
                    if (addr == it->second) {
                        item_actors_.erase(it);

                        // remove from base.
                        auto it = find_actor_addr(base_.item().children(), addr);

                        if (it != base_.item().end()) {
                            auto jsn  = base_.item().erase(it);
                            auto more = base_.item().refresh();
                            if (not more.is_null())
                                jsn.insert(jsn.begin(), more.begin(), more.end());

                            mail(event_atom_v, item_atom_v, jsn, false)
                                .send(base_.event_group());
                        }

                        break;
                    }
                }
            });
    }

    item_actors_[ua.uuid()] = ua.actor();
}

void TimelineActor::add_media(caf::actor actor, const Uuid &uuid, const Uuid &before_uuid) {

    if (actor) {
        if (not base_.contains_media(uuid)) {
            base_.insert_media(uuid, before_uuid);
            media_actors_[uuid] = actor;
            monitor_media(actor);
        }
    } else {
        spdlog::warn("{} Invalid media actor", __PRETTY_FUNCTION__);
    }
}

void TimelineActor::add_media(
    caf::typed_response_promise<UuidActor> rp, const UuidActor &ua, const Uuid &before_uuid) {

    try {

        caf::scoped_actor sys(system());

        // does the media already belong to parent playlist?
        auto actor = request_receive<caf::actor>(
            *sys,
            caf::actor_cast<caf::actor>(playlist_),
            playlist::get_media_atom_v,
            ua.uuid(),
            true);

        if (!actor) {

            // no, parent playlist doesn't have this media so we must add it first
            actor = request_receive<UuidActor>(
                        *sys,
                        caf::actor_cast<caf::actor>(playlist_),
                        playlist::add_media_atom_v,
                        ua,
                        before_uuid)
                        .actor();
        }

        add_media(actor, ua.uuid(), before_uuid);
        mail(event_atom_v, playlist::add_media_atom_v, UuidActorVector({ua}))
            .send(base_.event_group());
        base_.send_changed();
        mail(event_atom_v, change_atom_v).send(base_.event_group());

        // mail(event_atom_v, change_atom_v).send(change_event_group_);

        rp.deliver(ua);

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

bool TimelineActor::remove_media(caf::actor actor, const Uuid &uuid) {
    bool result = false;

    if (base_.remove_media(uuid)) {
        if (auto mit = monitor_.find(caf::actor_cast<caf::actor_addr>(actor));
            mit != std::end(monitor_)) {
            mit->second.dispose();
            monitor_.erase(mit);
        }

        media_actors_.erase(uuid);
        result = true;
    }

    return result;
}

void TimelineActor::on_exit() {
    for (const auto &i : item_actors_)
        send_exit(i.second, caf::exit_reason::user_shutdown);
}

// void TimelineActor::deliver_media_pointer(
//     caf::typed_response_promise<media::AVFrameID> rp,
//     const int logical_frame,
//     const media::MediaType media_type) {

//     spdlog::stopwatch sw;

//     std::vector<caf::actor> actors;
//     for (const auto &i : base_.media())
//         actors.push_back(media_actors_[i]);

//     fan_out_request<policy::select_all>(actors, infinite, media::media_reference_atom_v,
//     Uuid())
//         .then(
//             [=](std::vector<std::pair<Uuid, MediaReference>> refs) mutable {
//                 // re-order vector based on playlist order
//                 std::vector<std::pair<Uuid, MediaReference>> ordered_refs;
//                 for (const auto &i : base_.media()) {
//                     for (const auto &ii : refs) {
//                         const auto &[uuid, ref] = ii;
//                         if (uuid == i) {
//                             ordered_refs.push_back(ii);
//                             break;
//                         }
//                     }
//                 }

//                 // step though list, and find the relevant ref..
//                 std::pair<Uuid, MediaReference> m;
//                 int frames             = 0;
//                 bool exceeded_duration = true;

//                 for (auto it = std::begin(ordered_refs); it != std::end(ordered_refs); ++it)
//                 {
//                     if ((logical_frame - frames) < it->second.duration().frames()) {
//                         m                 = *it;
//                         exceeded_duration = false;
//                         break;
//                     }
//                     frames += it->second.duration().frames();
//                 }

//                 try {
//                     if (exceeded_duration)
//                         throw std::runtime_error("No frames left");
//                     // send request media atom..
//                     mail(media::get_media_pointer_atom_v, media_type, logical_frame - frames)
//                         .request(media_actors_[m.first], infinite)
//                         .then(
//                             [=](const media::AVFrameID &mp) mutable {
//                                 spdlog::warn("delivered {:.3f}", sw);
//                                 rp.deliver(mp);
//                             },
//                             [=](error &err) mutable { rp.deliver(std::move(err)); });

//                 } catch (const std::exception &e) {
//                     rp.deliver(make_error(xstudio_error::error, e.what()));
//                 }
//             },
//             [=](error &err) mutable { rp.deliver(std::move(err)); });
// }


void TimelineActor::sort_by_media_display_info(
    const int sort_column_index, const bool ascending) {

    using SourceAndUuid     = std::pair<std::string, Uuid>;
    auto sort_keys_vs_uuids = std::make_shared<std::vector<std::pair<std::string, Uuid>>>();

    int idx = 0;
    for (const auto &i : base_.media()) {

        // Pro tip: because i is a reference, it's the reference that is captured in our lambda
        // below and therefore it is 'unstable' so we make a copy here and use that in the
        // lambda as this is object-copied in the capture instead.
        UuidActor media_actor(i, media_actors_[i]);
        idx++;

        mail(media::media_display_info_atom_v)
            .request(media_actor.actor(), infinite)
            .await(

                [=](const JsonStore &media_display_info) mutable {
                    // media_display_info should be an array of arrays. Each
                    // array is the data shown in the media list columns.
                    // So info_set_idx corresponds to the media list (in the UI)
                    // from which the sort was requested. And the info_item_idx
                    // corresponds to the column of that media list.

                    // default sort key keeps current sorting but should always
                    // put it after the last element that did have a sort key
                    std::string sort_key = fmt::format("ZZZZZZ{}", idx);

                    if (media_display_info.is_array() &&
                        sort_column_index < media_display_info.size()) {
                        sort_key = media_display_info[sort_column_index].dump();
                    }

                    (*sort_keys_vs_uuids)
                        .push_back(std::make_pair(sort_key, media_actor.uuid()));

                    if (sort_keys_vs_uuids->size() == base_.media().size()) {

                        std::sort(
                            sort_keys_vs_uuids->begin(),
                            sort_keys_vs_uuids->end(),
                            [ascending](
                                const SourceAndUuid &a, const SourceAndUuid &b) -> bool {
                                return ascending ? a.first < b.first : a.first > b.first;
                            });

                        UuidList ordered_uuids;
                        for (const auto &p : (*sort_keys_vs_uuids)) {
                            ordered_uuids.push_back(p.second);
                        }

                        anon_mail(playlist::move_media_atom_v, ordered_uuids, Uuid())
                            .send(caf::actor_cast<caf::actor>(this));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}

void TimelineActor::insert_items(
    caf::typed_response_promise<JsonStore> rp, const int index, const UuidActorVector &uav) {

    // this is called after setting the rate
    auto do_insertion = [=]() mutable {
        // validate items can be inserted.
        fan_out_request<policy::select_all>(
            vector_to_caf_actor_vector(uav), infinite, item_atom_v)
            .then(
                [=](std::vector<Item> items) mutable {
                    // items are valid for insertion ?
                    for (const auto &i : items) {
                        if (not base_.item().valid_child(i))
                            return rp.deliver(
                                make_error(xstudio_error::error, "Invalid child type"));
                    }

                    // take ownership
                    for (const auto &ua : uav)
                        add_item(ua);

                    // find insertion point..
                    auto it = std::next(base_.item().begin(), index);

                    // insert items..
                    // our list will be out of order..
                    auto changes = JsonStore(R"([])"_json);
                    for (const auto &ua : uav) {
                        // find item..
                        auto found = false;
                        for (const auto &i : items) {
                            if (ua.uuid() == i.uuid()) {
                                auto tmp = base_.item().insert(it, i);
                                changes.insert(changes.end(), tmp.begin(), tmp.end());
                                found = true;
                                break;
                            }
                        }

                        if (not found) {
                            spdlog::error("item not found for insertion");
                        }
                    }

                    // add changes to stack
                    auto more = base_.item().refresh();

                    if (not more.is_null())
                        changes.insert(changes.begin(), more.begin(), more.end());

                    mail(event_atom_v, item_atom_v, changes, false).send(base_.event_group());
                    anon_mail(history::log_atom_v, __sysclock_now(), changes).send(history_);
                    mail(event_atom_v, change_atom_v).send(this);

                    rp.deliver(changes);
                },
                [=](const caf::error &err) mutable { rp.deliver(err); });
    };

    // before adding clips, we must force their frame rate to match ours.
    fan_out_request<policy::select_all>(
        vector_to_caf_actor_vector(uav), infinite, rate_atom_v, base_.rate(), false)
        .then(
            [=](std::vector<bool> r) mutable { do_insertion(); },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

std::pair<JsonStore, std::vector<Item>>
TimelineActor::remove_items(const int index, const int count) {

    std::vector<Item> items;
    JsonStore changes(R"([])"_json);

    if (index < 0 or index + count - 1 >= static_cast<int>(base_.item().size()))
        throw std::runtime_error("Invalid index / count");
    else {
        scoped_actor sys{system()};

        for (int i = index + count - 1; i >= index; i--) {
            auto it = std::next(base_.item().begin(), i);
            if (it != base_.item().end()) {
                auto item = *it;

                if (auto mit = monitor_.find(caf::actor_cast<caf::actor_addr>(item.actor()));
                    mit != std::end(monitor_)) {
                    mit->second.dispose();
                    monitor_.erase(mit);
                }

                item_actors_.erase(item.uuid());
                auto blind = request_receive<JsonStore>(*sys, item.actor(), serialise_atom_v);

                auto tmp = base_.item().erase(it, blind);
                changes.insert(changes.end(), tmp.begin(), tmp.end());
                items.push_back(item.clone());
            }
        }

        auto more = base_.item().refresh();
        if (not more.is_null())
            changes.insert(changes.begin(), more.begin(), more.end());

        // why was this commented out ?
        // mail(event_atom_v, item_atom_v, changes, false).send(base_.event_group());

        anon_mail(history::log_atom_v, __sysclock_now(), changes).send(history_);

        mail(event_atom_v, change_atom_v).send(this);
    }

    return std::make_pair(changes, items);
}


void TimelineActor::remove_items(
    caf::typed_response_promise<std::pair<JsonStore, std::vector<Item>>> rp,
    const int index,
    const int count) {

    try {
        rp.deliver(remove_items(index, count));
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void TimelineActor::erase_items(
    caf::typed_response_promise<JsonStore> rp, const int index, const int count) {

    try {
        auto result = remove_items(index, count);
        for (const auto &i : result.second)
            send_exit(i.actor(), caf::exit_reason::user_shutdown);
        rp.deliver(result.first);

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

// create new track from bake list
void TimelineActor::bake(caf::typed_response_promise<UuidActor> rp, const UuidSet &uuids) {
    // audio or video ?
    //  bake for range of timeline
    if (uuids.empty())
        return rp.deliver(make_error(xstudio_error::error, "Empty uuid list"));

    auto items = std::vector<std::optional<ResolvedItem>>();
    auto range = base_.item().trimmed_range();

    auto first    = range.start();
    auto duration = range.duration();
    auto last     = range.start() + range.duration();
    auto rate     = range.rate();

    auto mtype = media::MediaType::MT_IMAGE;

    // determine if this is audio or image
    // find track container of uuid.

    auto utrack = find_track_from_item(base_.item().children(), *(uuids.begin()));
    if (utrack) {
        // spdlog::warn("{}", (*utrack)->name());
        if ((*utrack)->item_type() == IT_AUDIO_TRACK)
            mtype = media::MediaType::MT_AUDIO;
    }

    // spdlog::warn("{} {} {}", range.frame_start().frames(), range.frame_duration().frames(),
    // to_string(range.rate())); spdlog::warn("{} {} {} {}", first.to_seconds(),
    // duration.to_seconds(), last.to_seconds(), to_string(range.rate()));

    for (auto i = first; i <= last; i += range.rate()) {
        auto r = base_.item().resolve_time(i, mtype, uuids, true);
        // if(r)
        //     spdlog::warn("frame {} {} {}", i.to_seconds(), std::get<1>(*r).to_seconds(),
        //     std::get<0>(*r).name());
        // else
        //     spdlog::warn("frame {}", i.to_seconds());
        items.emplace_back(r);
    }

    // collapse into sequence of clips and gaps.
    auto track = Track("Flattened Tracks", rate, mtype);

    for (const auto &i : items) {
        if (track.children().empty() or                                  // empty track
            (track.children().back().item_type() != IT_GAP and not i) or // change to gap
            (track.children().back().item_type() == IT_GAP and i) or     // change from gap
            (track.children().back().item_type() != IT_GAP and
             track.children().back().uuid() !=
                 std::get<0>(*i).uuid()) // change to different clip
        ) {
            if (not i) {
                track.children().emplace_back(Gap("Gap", FrameRateDuration(1, rate)).item());
            } else {
                // replace item with copy of i
                track.children().emplace_back(std::get<0>(*i));
                // track.children().back().set_uuid(Uuid::generate());
                track.children().back().set_actor_addr(caf::actor_addr());
                track.children().back().set_active_range(FrameRange(
                    FrameRateDuration(
                        std::get<1>(*i).to_flicks(), track.children().back().rate()),
                    FrameRateDuration(1, track.children().back().rate())));
            }
        } else {
            if (i) {
                auto trange = track.children().back().trimmed_range();
                trange.set_duration(trange.duration() + trange.rate());
                track.children().back().set_active_range(trange);
            } else {
                auto trange = track.children().back().trimmed_range();
                trange.set_duration(trange.duration() + trange.rate());
                track.children().back().set_active_range(trange);
            }
        }
    }
    // trim trailing gap..
    if (not track.children().empty() and track.children().back().item_type() == IT_GAP)
        track.children().pop_back();

    // reset uuids
    for (auto &i : track.item().children()) {
        i.set_uuid(Uuid::generate());
    }

    track.item().refresh();

    // for(const auto &i: track.item().children()) {
    //     spdlog::warn("{} {} {}", i.name(), i.trimmed_frame_start().frames(),
    //     i.trimmed_frame_duration().frames());
    // }

    // create actors
    auto track_uuid  = track.item().uuid();
    auto track_actor = spawn<TrackActor>(track.item());

    rp.deliver(UuidActor(track_uuid, track_actor));
}

void TimelineActor::export_otio_as_string(caf::typed_response_promise<std::string> rp) {

    otio::ErrorStatus err;
    auto jany = std::any();
    auto meta = base_.item().prop();

    try {
        if (not meta.is_object())
            meta = R"({})"_json;

        auto conform_track_uuid = meta.value("conform_track_uuid", Uuid());

        meta.erase("conform_track_uuid");

        auto xstudio_meta =
            R"({"xstudio": {"colour": "", "enabled": true, "locked": false}})"_json;
        xstudio_meta[COLOUR_JPOINTER]  = base_.item().flag();
        xstudio_meta[ENABLED_JPOINTER] = base_.item().enabled();
        xstudio_meta[LOCKED_JPOINTER]  = base_.item().locked();
        meta.update(xstudio_meta, true);
        deserialize_json_from_string(meta.dump(), &jany, &err);
        // if(is_error(err)) {
        //     spdlog::warn("Failed {}", err.full_description);
        // }

        otio::SerializableObject::Retainer<otio::Timeline> otimeline(new otio::Timeline(
            base_.item().name(),
            otio::RationalTime::from_frames(
                base_.item().trimmed_frame_start().frames(), base_.item().rate().to_fps()),
            std::any_cast<otio::AnyDictionary>(jany)));

        auto to_marker = [=](const Marker &item) mutable {
            if (meta = item.prop(); not meta.is_object())
                meta = R"({})"_json;

            xstudio_meta                  = R"({"xstudio": {"colour": ""}})"_json;
            xstudio_meta[COLOUR_JPOINTER] = item.flag();
            meta.update(xstudio_meta, true);
            deserialize_json_from_string(meta.dump(), &jany, &err);

            return otio::SerializableObject::Retainer<otio::Marker>(new otio::Marker(
                item.name(),
                otio::TimeRange(
                    otio::RationalTime::from_frames(
                        item.frame_start().frames(), item.rate().to_fps()),
                    otio::RationalTime::from_frames(
                        item.frame_duration().frames(), item.rate().to_fps())),
                "GREEN",
                std::any_cast<otio::AnyDictionary>(jany)));
        };

        auto to_composition = [=](const Item &item) mutable {
            otio::Composition *result = nullptr;

            if (item.item_type() == IT_VIDEO_TRACK or item.item_type() == IT_AUDIO_TRACK) {
                if (meta = item.prop(); not meta.is_object())
                    meta = R"({})"_json;

                xstudio_meta =
                    R"({"xstudio": {"colour": "", "enabled": true, "locked": false}})"_json;
                xstudio_meta[COLOUR_JPOINTER]  = item.flag();
                xstudio_meta[ENABLED_JPOINTER] = item.enabled();
                xstudio_meta[LOCKED_JPOINTER]  = item.locked();

                if (item.uuid() == conform_track_uuid)
                    xstudio_meta["xstudio"]["conform_track"] = true;

                meta.update(xstudio_meta, true);
                deserialize_json_from_string(meta.dump(), &jany, &err);

                result = new otio::Track(
                    item.name(),
                    otio::TimeRange(
                        otio::RationalTime::from_frames(
                            item.trimmed_frame_start().frames(), item.rate().to_fps()),
                        otio::RationalTime::from_frames(
                            item.trimmed_frame_duration().frames(), item.rate().to_fps())),
                    item.item_type() == IT_VIDEO_TRACK ? otio::Track::Kind::video
                                                       : otio::Track::Kind::audio,
                    std::any_cast<otio::AnyDictionary>(jany));
                result->set_enabled(item.enabled());

                for (const auto &citem : item.children()) {
                    if (meta = citem.prop(); not meta.is_object())
                        meta = R"({})"_json;

                    meta.erase("media_uuid");

                    if (citem.item_type() == IT_GAP) {
                        xstudio_meta =
                            R"({"xstudio": {"colour": "", "enabled": true, "locked": false}})"_json;
                        xstudio_meta[COLOUR_JPOINTER]  = citem.flag();
                        xstudio_meta[ENABLED_JPOINTER] = citem.enabled();
                        xstudio_meta[LOCKED_JPOINTER]  = citem.locked();
                        meta.update(xstudio_meta, true);
                        deserialize_json_from_string(meta.dump(), &jany, &err);

                        auto gap = new otio::Gap(
                            otio::RationalTime::from_frames(
                                citem.trimmed_frame_duration().frames(), citem.rate().to_fps()),
                            citem.name(),
                            std::vector<otio::Effect *>(),
                            std::vector<otio::Marker *>(),
                            std::any_cast<otio::AnyDictionary>(jany));

                        gap->set_enabled(citem.enabled());

                        result->append_child(gap);
                    } else if (citem.item_type() == IT_CLIP) {
                        xstudio_meta =
                            R"({"xstudio": {"colour": "", "media_colour": "", "enabled": true, "locked": false}})"_json;
                        xstudio_meta[COLOUR_JPOINTER]  = citem.flag();
                        xstudio_meta[ENABLED_JPOINTER] = citem.enabled();
                        xstudio_meta[LOCKED_JPOINTER]  = citem.locked();

                        // need to get the media flag..
                        try {
                            if (citem.actor()) {
                                caf::scoped_actor sys(system());
                                auto flag = std::get<0>(
                                    request_receive<std::tuple<std::string, std::string>>(
                                        *sys,
                                        citem.actor(),
                                        playlist::reflag_container_atom_v));
                                if (flag != "#00000000" and flag != "")
                                    xstudio_meta[MEDIA_COLOUR_JPOINTER] = flag;
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }

                        meta.update(xstudio_meta, true);
                        deserialize_json_from_string(meta.dump(), &jany, &err);

                        auto clip = new otio::Clip(
                            citem.name(),
                            nullptr,
                            otio::TimeRange(
                                otio::RationalTime::from_frames(
                                    citem.trimmed_frame_start().frames(),
                                    citem.rate().to_fps()),
                                otio::RationalTime::from_frames(
                                    citem.trimmed_frame_duration().frames(),
                                    citem.rate().to_fps())),
                            std::any_cast<otio::AnyDictionary>(jany));

                        clip->set_enabled(citem.enabled());

                        try {
                            if (citem.actor()) {
                                caf::scoped_actor sys(system());

                                auto mr = MediaReference();
                                auto mt = item.item_type() == IT_VIDEO_TRACK ? media::MT_IMAGE
                                                                             : media::MT_AUDIO;

                                try {
                                    mr = request_receive<std::pair<Uuid, MediaReference>>(
                                             *sys,
                                             citem.actor(),
                                             media::media_reference_atom_v,
                                             mt)
                                             .second;
                                } catch (...) {
                                    // fallback..
                                    if (mt == media::MT_AUDIO) {
                                        mt = media::MT_IMAGE;
                                        mr = request_receive<std::pair<Uuid, MediaReference>>(
                                                 *sys,
                                                 citem.actor(),
                                                 media::media_reference_atom_v,
                                                 mt)
                                                 .second;
                                    } else {
                                        throw;
                                    }
                                }

                                auto msua = request_receive<UuidActor>(
                                    *sys,
                                    citem.actor(),
                                    media::current_media_source_atom_v,
                                    mt);

                                auto name = request_receive<std::string>(
                                    *sys, msua.actor(), name_atom_v);

                                if (mr.container()) {
                                    auto extref = new otio::ExternalReference(
                                        "file://" + uri_to_posix_path(mr.uri()));
                                    if (citem.available_range()) {
                                        extref->set_available_range(otio::TimeRange(
                                            otio::RationalTime::from_frames(
                                                citem.available_frame_start()->frames() +
                                                    mr.start_frame_offset(),
                                                citem.rate().to_fps()),
                                            otio::RationalTime::from_frames(
                                                citem.available_frame_duration()->frames() -
                                                    mr.start_frame_offset(),
                                                citem.rate().to_fps())));
                                    }
                                    extref->set_name(name);
                                    clip->set_media_reference(extref);
                                } else {
                                    auto path = uri_to_posix_path(mr.uri());
                                    const static std::regex path_re(
                                        R"(^(.+?\/)([^/]+\.)\{:(\d+)d\}(\..+?)$)");
                                    std::cmatch m;
                                    if (std::regex_match(path.c_str(), m, path_re)) {
                                        auto base   = m[1].str();
                                        auto prefix = m[2].str();
                                        auto pad    = std::stoi(m[3].str());
                                        auto suffix = m[4].str();

                                        // spdlog::warn("base {}", base);
                                        // spdlog::warn("prefix {}", prefix);
                                        // spdlog::warn("pad {}", pad);
                                        // spdlog::warn("sufix {}", suffix);

                                        auto extref = new otio::ImageSequenceReference(
                                            "file://" + base,
                                            prefix,
                                            suffix,
                                            citem.available_frame_start()->frames(),
                                            1,
                                            citem.rate().to_fps(),
                                            pad);
                                        extref->set_name(name);

                                        if (citem.available_range()) {
                                            extref->set_available_range(otio::TimeRange(
                                                otio::RationalTime::from_frames(
                                                    citem.available_frame_start()->frames(),
                                                    citem.rate().to_fps()),
                                                otio::RationalTime::from_frames(
                                                    citem.available_frame_duration()->frames(),
                                                    citem.rate().to_fps())));
                                        }
                                        clip->set_media_reference(extref);
                                    }

                                    // ImageSequenceReference(
                                    //       std::string const&       target_url_base    =
                                    //       std::string(), std::string const&       name_prefix
                                    //       = std::string(), std::string const& name_suffix =
                                    //       std::string(), int                      start_frame
                                    //       = 1, int                      frame_step         =
                                    //       1, double                   rate               = 1,
                                    //       int                      frame_zero_padding = 0,
                                    //       MissingFramePolicy const missing_frame_policy =
                                    //           MissingFramePolicy::error,
                                    //       optional<TimeRange> const&    available_range =
                                    //       nullopt, AnyDictionary const&          metadata =
                                    //       AnyDictionary(), optional<Imath::Box2d> const&
                                    //       available_image_bounds = nullopt);
                                }
                            } else {
                                spdlog::warn("Missing media actor {}", citem.name());
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }

                        result->append_child(clip);
                    }
                }
            }

            return result;
        };

        // add tracks.. / etc..
        auto ostack = otimeline->tracks();

        auto stack = base_.item().front();
        for (const auto &marker : stack.markers()) {
            ostack->markers().push_back(to_marker(marker));
        }

        // stack already created, so we can't set source range ?

        // if (stack.available_range()) {
        //     ostack->set_available_range(otio::TimeRange(
        //         otio::RationalTime::from_frames(
        //             stack.available_frame_start()->frames(),
        //             stack.rate().to_fps()),
        //         otio::RationalTime::from_frames(
        //             stack.available_frame_duration()->frames(),
        //             stack.rate().to_fps())));
        // }


        if (meta = base_.item().front().prop(); not meta.is_object())
            meta = R"({})"_json;

        xstudio_meta = R"({"xstudio": {"colour": "", "enabled": true, "locked": false}})"_json;
        xstudio_meta[COLOUR_JPOINTER]  = base_.item().front().flag();
        xstudio_meta[ENABLED_JPOINTER] = base_.item().front().enabled();
        xstudio_meta[LOCKED_JPOINTER]  = base_.item().front().locked();
        meta.update(xstudio_meta, true);
        deserialize_json_from_string(meta.dump(), &jany, &err);
        ostack->metadata() = std::any_cast<otio::AnyDictionary>(jany);

        for (const auto &track : stack.children()) {
            if (track.item_type() == IT_VIDEO_TRACK)
                ostack->insert_child(0, to_composition(track));
            else
                ostack->append_child(to_composition(track));
        }

        const auto result = otimeline->to_json_string(&err);

        if (not otio::is_error(err))
            rp.deliver(result);
        else
            rp.deliver(make_error(xstudio_error::error, "Export failed"));

        // and crash..
        otimeline->possibly_delete();
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {}", __PRETTY_FUNCTION__, err.what(), meta.dump(2), jany.type().name());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}


void TimelineActor::export_otio(
    caf::typed_response_promise<bool> rp,
    const std::string &otio_str,
    const caf::uri &path,
    const std::string &type,
    const std::string &target_schema) {
    // build timeline from model..
    // we need clips to return information on current media source..

    caf::scoped_actor sys(system());
    auto global = system().registry().template get<caf::actor>(global_registry);
    auto epa    = request_receive<caf::actor>(*sys, global, global::get_python_atom_v);
    rp.delegate(epa, session::export_atom_v, otio_str, path, type, target_schema);
}
