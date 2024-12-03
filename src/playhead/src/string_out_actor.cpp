// SPDX-License-Identifier: Apache-2.0
#include <chrono>

#include <caf/policy/select_all.hpp>
#include <caf/unit.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/playhead/string_out_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;

StringOutActor::StringOutActor(
    caf::actor_config &cfg,
    const utility::UuidActorVector &sources) : caf::event_based_actor(cfg), source_actors_(sources)
{
    
    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    
    for (auto &source: source_actors_) {

        monitor(source.actor());
        utility::join_event_group(this, source.actor());
 
    }

    set_down_handler([=](down_msg &msg) {
        // is source down?
        auto p = source_actors_.begin();
        bool change = false;
        while (p != source_actors_.end()) {
            if ((*p).actor() == msg.source) {
                p = source_actors_.erase(p);
                change = true;
            } else {
                p++;
            }
        }
        if (change) {
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
        }
    });

    // Note - out source_actors_ will usually be MediaActors, but might be
    // A TimelineActor or a ClipActor. The event_atom handlers below do nothing
    // as we're not interested in the messages except for an event_atom, change_atom
    // message. If we get one of these we re-emit it to our  event_group_. The
    // SubPlayhead that is playing us will then respond by fetching our frames
    // from us.

    behavior_.assign(

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },

        [=](media::get_media_pointers_atom atom,
            const media::MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {

            auto rp = make_response_promise<media::FrameTimeMapPtr>();
            build_frame_map(media_type, tsm, override_rate, rp);
            return rp;

        },

        [=](utility::event_atom, playlist::add_media_atom, const utility::UuidActorVector &) {},

        [=](utility::event_atom, playlist::remove_media_atom, const utility::UuidVector &) {},

        [=](utility::event_atom,
            media_reader::get_thumbnail_atom,
            const thumbnail::ThumbnailBufferPtr &buf) {},

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &uav) {
            send(event_group_, utility::event_atom_v, media::add_media_source_atom_v, uav);
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &changes, bool) {
        },

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus) {
        },

        [=](utility::event_atom,
            media::media_display_info_atom,
            const utility::JsonStore &,
            caf::actor_addr &) {},

        [=](utility::event_atom, media::media_display_info_atom, const utility::JsonStore &) {},

        [=](utility::event_atom, utility::change_atom) {

            // one of the sources has changed -  this might affect its actual
            // frames for display. So we clear the entry in our cache, and 
            // re-emit the change event to SubPlayhead
            auto sender = caf::actor_cast<caf::actor>(current_sender());
            for (auto &s: source_actors_) {
                if (s.actor() == sender) {
                    source_frames_[s.uuid()].reset();
                }
            }
            send(event_group_, utility::event_atom_v, utility::change_atom_v); // triggers refresh of frames_time_list_
        },

        [=](utility::event_atom, utility::last_changed_atom, const utility::time_point &) {
            delegate(
                caf::actor_cast<caf::actor>(this),
                utility::event_atom_v,
                utility::change_atom_v);
        },

        [=](utility::rate_atom atom, const media::MediaType media_type) -> utility::FrameRate {
            return utility::FrameRate(timebase::k_flicks_24fps);
        },

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookmark_uuid) {
        },

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &,
            const utility::UuidList &) {},

        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {
        });


}

void StringOutActor::build_frame_map(
    const media::MediaType media_type,
    const utility::TimeSourceMode tsm,
    const utility::FrameRate &override_rate,
    caf::typed_response_promise<media::FrameTimeMapPtr> rp) {

    auto count = std::make_shared<int>(0);

    for (auto &c: source_actors_) {
        // we already have frames for this source
        if (source_frames_[c.uuid()]) continue;

        (*count)++;

        auto source_uuid = c.uuid();
        request(
            c.actor(),
            infinite,
            media::get_media_pointers_atom_v,
            media_type,
            tsm,
            override_rate)
            .then(
                [=](const media::FrameTimeMapPtr &mpts) mutable {

                    source_frames_[source_uuid] = mpts;
                    (*count)--;
                    if (!(*count)) {
                        finalise_frame_map(rp);
                    }
                },
                [=](caf::error &err) mutable {
                    (*count)--;
                    if (!(*count)) {
                        finalise_frame_map(rp);
                    }
                });
    }

    if (!(*count)) {
        finalise_frame_map(rp);
    }
}

void StringOutActor::finalise_frame_map(caf::typed_response_promise<media::FrameTimeMapPtr> rp) {

    media::FrameTimeMap * result = new media::FrameTimeMap;
    timebase::flicks d(0);
    for (auto &c: source_actors_) {

        if (!source_frames_[c.uuid()]) continue;
        const media::FrameTimeMap &map = *(source_frames_[c.uuid()]);
        timebase::flicks prev(0);
        for (const auto &p: map) {
            d += p.first - prev;
            (*result)[d] = p.second;
            prev = p.first;
        }

    }
    rp.deliver(media::FrameTimeMapPtr(result));
}
