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

    if (jsn.contains("playhead")) {
        playhead_serialisation_ = jsn["playhead"];
    }
    init();
}

ContactSheetActor::ContactSheetActor(
    caf::actor_config &cfg, caf::actor playlist, const std::string &name)
    : SubsetActor(cfg, playlist, name, "ContactSheet") 
{
    init();
}

void ContactSheetActor::init() {

    // here we join our own events channel. The 'change_event_group' just emits
    // change events when the underlying container (i.e. the SubSet base class)
    // changes, i.e. when media is added, removed or re-ordered
    request(caf::actor_cast<caf::actor>(this), infinite, get_change_event_group_atom_v).then(
        [=](caf::actor grp) {
            join_broadcast(this, grp);
        },
        [=](caf::error &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        });


    override_behaviour_ = caf::message_handler {

        [=](playlist::create_playhead_atom) -> UuidActor {

            if (playhead_)
                return playhead_;
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<playhead::PlayheadActor>(
                std::string("Contact Sheet Playhead"),
                selection_actor_,
                uuid,
                caf::actor_cast<caf::actor_addr>(this));
            link_to(actor);

            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            
            playhead_ = UuidActor(uuid, actor);

            // now get the playhead to load our media 
            request(caf::actor_cast<caf::actor>(this), infinite, playlist::get_media_atom_v).then(
                [=](const UuidActorVector &media) mutable {
                    request(playhead_.actor(), infinite, playhead::source_atom_v, media).then(
                        [=](bool) mutable {

                            // restore the playhead state, if we have serialisation data
                            if (!playhead_serialisation_.is_null()) {
                                anon_send(playhead_.actor(), module::deserialise_atom_v, playhead_serialisation_);
                            } else {
                                anon_send(playhead_.actor(), playhead::compare_mode_atom_v, "Grid");
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
            if (!playhead_) return;
            request(caf::actor_cast<caf::actor>(this), infinite, playlist::get_media_atom_v).then(
                [=](const UuidActorVector &media) mutable {
                    anon_send(playhead_.actor(), playhead::source_atom_v, media);
                },
                [=](caf::error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });

        },
        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            JsonStore j;
            j["base"] = SubsetActor::serialise();
            if (playhead_) {
                request(playhead_.actor(), infinite, utility::serialise_atom_v).then(
                    [=](const utility::JsonStore &playhead_state) mutable {
                        playhead_serialisation_ = playhead_state;
                        j["playhead"] = playhead_state;
                        rp.deliver(j);
                    },
                    [=](caf::error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(j);
                    });

            } else if (!playhead_serialisation_.is_null()) {
                j["playhead"] = playhead_serialisation_;
                rp.deliver(j);
            }
            return rp;
        }
        };
}