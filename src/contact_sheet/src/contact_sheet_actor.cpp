// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/contact_sheet/contact_sheet_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::contact_sheet;
using namespace xstudio::playlist;
using namespace xstudio::subset;

ContactSheetActor::ContactSheetActor(
    caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn)
    : SubsetActor(cfg, playlist, jsn) {

    init();
}

ContactSheetActor::ContactSheetActor(
    caf::actor_config &cfg, caf::actor playlist, const std::string &name)
    : SubsetActor(cfg, playlist, name, "ContactSheet") {
    init();
}

void ContactSheetActor::init() {

    // here we join our own events channel. The 'change_event_group' just emits
    // change events when the underlying container (i.e. the SubSet base class)
    // changes, i.e. when media is added, removed or re-ordered
    request(caf::actor_cast<caf::actor>(this), infinite, get_change_event_group_atom_v)
        .then(
            [=](caf::actor grp) { join_broadcast(this, grp); },
            [=](caf::error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });


    override_behaviour_ = caf::message_handler{

        [=](duplicate_atom) -> result<UuidActor> {
            // clone ourself..
            auto actor =
                spawn<ContactSheetActor>(caf::actor_cast<caf::actor>(playlist_), base_.name());
            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            // get uuid from actor..
            try {
                caf::scoped_actor sys(system());
                // get uuid..
                Uuid uuid = request_receive<Uuid>(*sys, actor, utility::uuid_atom_v);

                // maybe not be safe.. as ordering isn't implicit..
                std::vector<UuidActor> media_actors;
                for (const auto &i : base_.media())
                    media_actors.emplace_back(UuidActor(i, actors_[i]));

                request_receive<bool>(
                    *sys, actor, playlist::add_media_atom_v, media_actors, Uuid());

                return UuidActor(uuid, actor);

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](playlist::create_playhead_atom) -> UuidActor {
            if (playhead_)
                return playhead_;
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<playhead::PlayheadActor>(
                std::string("Contact Sheet Playhead"),
                playhead::GLOBAL_AUDIO,
                selection_actor_,
                uuid,
                caf::actor_cast<caf::actor_addr>(this));
            link_to(actor);

            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());

            playhead_ = UuidActor(uuid, actor);

            // now get the playhead to load our media
            request(caf::actor_cast<caf::actor>(this), infinite, playlist::get_media_atom_v)
                .then(
                    [=](const UuidActorVector &media) mutable {
                        request(playhead_.actor(), infinite, playhead::source_atom_v, media)
                            .then(
                                [=](bool) mutable {
                                    // restore the playhead state, if we have serialisation data
                                    if (!playhead_serialisation_.is_null()) {
                                        anon_send(
                                            playhead_.actor(),
                                            module::deserialise_atom_v,
                                            playhead_serialisation_);
                                    } else {
                                        anon_send(
                                            playhead_.actor(),
                                            playhead::compare_mode_atom_v,
                                            "Grid");
                                    }
                                },
                                [=](caf::error &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                });
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });

            return playhead_;
        },
        [=](utility::event_atom, utility::change_atom) {
            if (!playhead_)
                return;
            request(caf::actor_cast<caf::actor>(this), infinite, playlist::get_media_atom_v)
                .then(
                    [=](const UuidActorVector &media) mutable {
                        anon_send(playhead_.actor(), playhead::source_atom_v, media);
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }};
}