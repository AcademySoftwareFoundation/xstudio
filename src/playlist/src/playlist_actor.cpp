// SPDX-License-Identifier: Apache-2.0

#include <caf/policy/select_all.hpp>
#include <filesystem>

#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/contact_sheet/contact_sheet_actor.hpp"
#include "xstudio/event/event.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/subset/subset_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/string_helpers.hpp"

using namespace xstudio;
using namespace xstudio::global_store;
using namespace xstudio::playlist;
using namespace xstudio::utility;

namespace fs = std::filesystem;


using namespace nlohmann;

void blocking_loader(
    blocking_actor *self,
    caf::response_promise rp,
    UuidActor dst,
    const caf::uri &path,
    const bool recursive,
    const FrameRate &default_rate,
    const bool auto_gather,
    const caf::actor &session,
    const utility::Uuid &before) {
    std::vector<UuidActor> result;
    std::vector<UuidActor> batched_media_to_add;
    // spdlog::error("blocking_loader uri {}", to_string(path));
    // spdlog::error("blocking_loader posix {}", uri_to_posix_path(path));

    self->anon_send(dst.actor(), playlist::loading_media_atom_v, true);

    auto event_msg = event::Event("Loading Playlist Media {}", 0, 0, 1, {dst.uuid()});

    event::send_event(self, event_msg);

    auto items = utility::scan_posix_path(uri_to_posix_path(path), recursive ? -1 : 0);
    std::sort(
        std::begin(items),
        std::end(items),
        [](std::pair<caf::uri, FrameList> a, std::pair<caf::uri, FrameList> b) {
            return a.first < b.first;
        });

    event_msg.set_progress_maximum(items.size());
    event::send_event(self, event_msg);

    for (const auto &i : items) {
        event_msg.set_progress(event_msg.progress() + 1);
        event::send_event(self, event_msg);

        try {
            if (is_file_supported(i.first)) {

                const caf::uri &uri         = i.first;
                const FrameList &frame_list = i.second;

                const auto uuid = Uuid::generate();
#ifdef _WIN32
                std::string ext =
                    ltrim_char(to_upper_path(fs::path(uri_to_posix_path(uri)).extension()), '.');
#else
                std::string ext =
                    ltrim_char(to_upper(fs::path(uri_to_posix_path(uri)).extension()), '.');
#endif
                const auto source_uuid = Uuid::generate();

                auto source =
                    frame_list.empty()
                        ? self->spawn<media::MediaSourceActor>(
                              (ext.empty() ? "UNKNOWN" : ext), uri, default_rate, source_uuid)
                        : self->spawn<media::MediaSourceActor>(
                              (ext.empty() ? "UNKNOWN" : ext),
                              uri,
                              frame_list,
                              default_rate,
                              source_uuid);

                auto media = self->spawn<media::MediaActor>(
                    "New Media", uuid, UuidActorVector({UuidActor(source_uuid, source)}));

                if (auto_gather)
                    anon_send(
                        session, media_hook::gather_media_sources_atom_v, media, default_rate);

                UuidActor ua(uuid, media);

                result.emplace_back(ua);

                // this has to be done in the source creation...
                // fetching metadata can be done 'lazily' as we don't need it prior to
                // building the playlist
                // self->request(media, infinite, media_metadata::get_metadata_atom_v).receive(
                //     [=](bool){},
                //     [=](error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__,
                //     to_string(err)); }
                // );
                // self->anon_send(media, media_metadata::get_metadata_atom_v);

                batched_media_to_add.emplace_back(ua);

                // we batch to try and cut down thrashing..
                if (batched_media_to_add.size() == 1) {
                    self->anon_send(dst.actor(), playlist::loading_media_atom_v, true);
                    self->request(
                            dst.actor(),
                            infinite,
                            playlist::add_media_atom_v,
                            batched_media_to_add,
                            before)
                        .receive(
                            [=](const bool) mutable {},
                            [=](error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                    batched_media_to_add.clear();
                }
            } else {
                spdlog::warn("Unsupported file type {}.", to_string(i.first));
            }

        } catch (const std::exception &e) {
            spdlog::error("Failed to create media {} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    if (not batched_media_to_add.empty()) {
        self->request(
                dst.actor(), infinite, playlist::add_media_atom_v, batched_media_to_add, Uuid())
            .receive(
                [=](const bool) mutable {},
                [=](error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
    event_msg.set_complete();
    event::send_event(self, event_msg);

    // this message will update the 'loading_media_atom' status of the playlist in the UI
    self->anon_send(dst.actor(), playlist::loading_media_atom_v, false);
    // self->delayed_anon_send(dst.actor(), std::chrono::seconds(1),
    // media_content_changed_atom_v);
    rp.deliver(result);
}


PlaylistActor::PlaylistActor(
    caf::actor_config &cfg, const utility::JsonStore &jsn, const caf::actor &session)
    : caf::event_based_actor(cfg),
      base_(JsonStore(jsn.at("base"))),
      session_(caf::actor_cast<caf::actor_addr>(session)) {
    // deserialize actors..
    // and inject into maps..

    if (not jsn.count("store") or jsn.at("store").is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(),
            JsonStore(jsn.at("store")),
            std::chrono::milliseconds(50));
    }

    link_to(json_store_);

    // media needs to exist before we can deserialise containers.
    // spdlog::stopwatch sw;

    for (const auto &[key, value] : jsn.at("actors").items()) {
        if (value.at("base").at("container").at("type") == "Media") {
            try {
                auto actor =
                    spawn<media::MediaActor>(static_cast<utility::JsonStore>(value), true);
                media_[key] = actor;
                link_to(actor);
                join_event_group(this, actor);
            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        }
    }
    // spdlog::info("media loaded in {:.3} seconds.", sw);
    // deserialise containers
    for (const auto &[key, value] : jsn.at("actors").items()) {
        if (value.at("base").at("container").at("type") == "Subset") {
            try {
                auto actor = system().spawn<subset::SubsetActor>(
                    this, static_cast<utility::JsonStore>(value));
                container_[key] = actor;
                link_to(actor);
                join_event_group(this, actor);
            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        } else if (value.at("base").at("container").at("type") == "ContactSheet") {
            try {
                auto actor = system().spawn<contact_sheet::ContactSheetActor>(
                    this, static_cast<utility::JsonStore>(value));
                container_[key] = actor;
                link_to(actor);
                join_event_group(this, actor);
            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        } else if (value.at("base").at("container").at("type") == "Timeline") {
            try {
                auto actor = system().spawn<timeline::TimelineActor>(
                    static_cast<utility::JsonStore>(value), caf::actor_cast<caf::actor>(this));

                container_[key] = actor;
                link_to(actor);
                join_event_group(this, actor);
                // link media to clips.
                anon_send(actor, timeline::link_media_atom_v, media_);
            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        }
    }

    selection_actor_ = spawn<playhead::PlayheadSelectionActor>(
        "PlaylistPlayheadSelectionActor", caf::actor_cast<caf::actor>(this));
    link_to(selection_actor_);

    init();
}

PlaylistActor::PlaylistActor(
    caf::actor_config &cfg,
    const std::string &name,
    const utility::Uuid &uuid,
    const caf::actor &session)
    : caf::event_based_actor(cfg),
      base_(name),
      session_(caf::actor_cast<caf::actor_addr>(session)) {

    if (not uuid.is_null())
        base_.set_uuid(uuid);

    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    link_to(json_store_);

    init();
}


caf::message_handler PlaylistActor::default_event_handler() {
    return {
        [=](utility::event_atom, change_atom) {},
        [=](utility::event_atom, add_media_atom, const utility::UuidActor &) {},
        [=](utility::event_atom, loading_media_atom, const bool) {},
        [=](utility::event_atom, create_divider_atom, const utility::Uuid &) {},
        [=](utility::event_atom, create_group_atom, const utility::Uuid &) {},
        [=](utility::event_atom,
            reflag_container_atom,
            const utility::Uuid &,
            const std::string &) {},
        [=](utility::event_atom, remove_container_atom, const utility::Uuid &) {},
        [=](utility::event_atom,
            rename_container_atom,
            const utility::Uuid &,
            const std::string &) {},
        [=](utility::event_atom, create_subset_atom, const utility::UuidActor &) {},
        [=](utility::event_atom, create_contact_sheet_atom, const utility::UuidActor &) {},
        [=](utility::event_atom, create_timeline_atom, const utility::UuidActor &) {},
        [=](utility::event_atom, media_content_changed_atom, const utility::UuidActorVector &) {
        },
        [=](utility::event_atom, move_container_atom, const Uuid &, const Uuid &, const bool) {
        }};
}


void PlaylistActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    // event_group_ =
    // system().groups().get_local(to_string(actor_cast<caf::actor>(this)));//system().groups().anonymous();
    event_group_        = spawn<broadcast::BroadcastActor>(this);
    change_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    link_to(change_event_group_);

    playlist_broadcast_ = spawn<broadcast::BroadcastActor>(this);

    if (!selection_actor_) {
        selection_actor_ = spawn<playhead::PlayheadSelectionActor>(
            "PlaylistPlayheadSelectionActor", caf::actor_cast<caf::actor>(this));
        link_to(selection_actor_);
    }

    join_broadcast(
        caf::actor_cast<caf::event_based_actor *>(selection_actor_), playlist_broadcast_);

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        auto_gather_sources_ =
            preference_value<bool>(j, "/core/media_reader/auto_gather_sources");
    } catch (...) {
    }

    // side step problem with anon_send..
    // set_default_handler(skip);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        if (msg.source == playhead_.actor()) {
            demonitor(playhead_.actor());
            send(event_group_, utility::event_atom_v, change_atom_v);
            base_.send_changed(event_group_, this);
        }
    });

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        base_.make_ignore_error_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),
        base_.make_last_changed_event_handler(event_group_, this),

        [=](broadcast::join_broadcast_atom) -> caf::actor { return playlist_broadcast_; },

        [=](utility::event_atom,
            timeline::item_atom,
            const utility::JsonStore &update,
            const bool hidden) {},

        [=](add_media_atom,
            const std::string &name,
            const caf::uri &uri,
            const FrameList &frame_list,
            const utility::FrameRate &rate,
            const utility::UuidActor &uuid_before) {
            const auto uuid = Uuid::generate();
#ifdef _WIN32
            std::string ext =
                ltrim_char(to_upper_path(fs::path(uri_to_posix_path(uri)).extension()), '.');
#else
	    std::string ext =
                ltrim_char(to_upper(fs::path(uri_to_posix_path(uri)).extension()), '.');
#endif
            const auto source_uuid = Uuid::generate();

            auto source =
                frame_list.empty()
                    ? spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, rate, source_uuid)
                    : spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, frame_list, rate, source_uuid);

            auto media = spawn<media::MediaActor>(
                name, uuid, UuidActorVector({UuidActor(source_uuid, source)}));

            if (auto_gather_sources_)
                anon_send(
                    actor_cast<caf::actor>(session_),
                    media_hook::gather_media_sources_atom_v,
                    media,
                    rate);

            delegate(
                actor_cast<caf::actor>(this),
                add_media_atom_v,
                UuidActor(uuid, media),
                uuid_before);
        },

        [=](add_media_atom,
            const std::string &name,
            const caf::uri &uri,
            const utility::Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this),
                add_media_atom_v,
                name,
                uri,
                FrameList(),
                uuid_before);
        },


        // this should not be required... we should be able to delegate, but it doesn't work..
        [=](add_media_atom atom,
            const std::string &name,
            const caf::uri &uri,
            const FrameList &frame_list,
            const utility::Uuid &uuid_before) {
            // delegate(
            //     actor_cast<caf::actor>(this),
            //     atom,
            //     name,
            //     uri,
            //     frame_list,
            //     base_.media_rate(),
            //     uuid_before);

            const auto uuid = Uuid::generate();
#ifdef _WIN32
            std::string ext =
                ltrim_char(to_upper_path(fs::path(uri_to_posix_path(uri)).extension()), '.');
#else
	    std::string ext =
                ltrim_char(to_upper(fs::path(uri_to_posix_path(uri)).extension()), '.');
#endif
            const auto source_uuid = Uuid::generate();

            auto source =
                frame_list.empty()
                    ? spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, base_.media_rate(), source_uuid)
                    : spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext),
                          uri,
                          frame_list,
                          base_.media_rate(),
                          source_uuid);

            auto media = spawn<media::MediaActor>(
                name, uuid, UuidActorVector({UuidActor(source_uuid, source)}));

            if (auto_gather_sources_)
                anon_send(
                    actor_cast<caf::actor>(session_),
                    media_hook::gather_media_sources_atom_v,
                    media,
                    base_.media_rate());

            delegate(
                actor_cast<caf::actor>(this),
                add_media_atom_v,
                UuidActor(uuid, media),
                uuid_before);
        },

        [=](add_media_atom atom,
            const caf::uri &path,
            const bool recursive,
            const utility::Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                path,
                recursive,
                base_.media_rate(),
                uuid_before);
        },

        [=](add_media_atom atom,
            const std::string &name,
            const utility::UuidActor &uuid_before) -> UuidActor {
            const auto uuid   = Uuid::generate();
            auto media        = spawn<media::MediaActor>(name, uuid, UuidActorVector());
            auto ua           = UuidActor(uuid, media);
            media_[ua.uuid()] = media;
            join_event_group(this, media);
            link_to(media);
            base_.insert_media(uuid, uuid_before);
            send(event_group_, utility::event_atom_v, add_media_atom_v, ua);
            send(playlist_broadcast_, utility::event_atom_v, add_media_atom_v, ua);
            send_content_changed_event();
            base_.send_changed(event_group_, this);
            return ua;
        },


        [=](add_media_atom atom,
            const std::vector<std::string> &names,
            const utility::UuidActor &uuid_before) -> UuidActorVector {
            UuidActorVector result;
            for (const auto &name : names) {
                const auto uuid = Uuid::generate();
                auto media      = spawn<media::MediaActor>(name, uuid, UuidActorVector());
                auto ua         = UuidActor(uuid, media);
                result.emplace_back(ua);
                media_[ua.uuid()] = media;
                join_event_group(this, media);
                link_to(media);
                base_.insert_media(uuid, uuid_before);
                send(event_group_, utility::event_atom_v, add_media_atom_v, ua);
                send(playlist_broadcast_, utility::event_atom_v, add_media_atom_v, ua);
            }
            send_content_changed_event();
            base_.send_changed(event_group_, this);
            return result;
        },

        [=](add_media_atom atom,
            UuidActor ua, // the MediaActor
            const utility::UuidList &final_ordered_uuid_list,
            utility::Uuid before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            // This handler lets us build a playlist of a given order, even when
            // we add the media in an out of order way. We generate Uuids for the
            // MediaActors that will be used to build the playlist *before* the
            // actual MediaActors are built and added - we pass in the ordered
            // list of Uuids when adding each MediaActor so we can insert it
            // into the correct point as the playlist is being built.
            //
            // This is used in ShotgunDataSourceActor, for example

            const utility::UuidList media = base_.media();

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

            // Note we can't use delegate(this, add_media_atom_v, ua, before)
            // to enact the adding, because it might happen *after* we get
            // another of these add_media messages which would then mess up the
            // ordering algorithm
            add_media(ua, before, rp);

            return rp;
        },

        [=](add_media_atom,
            UuidActor ua,
            const utility::Uuid &uuid_before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            add_media(ua, uuid_before, rp);
            return rp;
        },

        [=](add_media_atom,
            caf::actor actor,
            const utility::Uuid &uuid_before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            // we block other requests, as we don't wat the order beening messed up.
            request(actor, infinite, uuid_atom_v)
                .await(
                    [=](const utility::Uuid &uuid) mutable {
                        // hmm this is going to knacker ordering..
                        request(
                            actor_cast<caf::actor>(this),
                            infinite,
                            add_media_atom_v,
                            UuidActor(uuid, actor),
                            uuid_before)
                            .then(
                                [=](const utility::UuidActor &ua) mutable { rp.deliver(ua); },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](add_media_atom,
            const caf::uri &path,
            const bool recursive,
            const utility::FrameRate &rate,
            const utility::Uuid &uuid_before) -> result<std::vector<UuidActor>> {
            auto rp = make_response_promise<std::vector<UuidActor>>();
            spawn(
                blocking_loader,
                rp,
                UuidActor(base_.uuid(), actor_cast<caf::actor>(this)),
                path,
                recursive,
                rate,
                auto_gather_sources_,
                actor_cast<caf::actor>(session_),
                uuid_before);

            send(event_group_, utility::event_atom_v, loading_media_atom_v, true);
            return rp;
        },

        [=](add_media_atom,
            std::vector<UuidActor> ma,
            const utility::Uuid &uuid_before) -> result<bool> {
            // before we can add media actors, we have to make sure the detail has been acquired
            // so that the duration of the media is known. This is because the playhead will
            // update and build a timeline as soon as the playlist notifies of change, so the
            // duration and frame rate must be known up-front

            std::vector<UuidActor> media_actors = ma;
            auto source_count = std::make_shared<int>();
            (*source_count)   = media_actors.size();
            auto rp           = make_response_promise<bool>();

            // add to lis first, then lazy update..

            for (auto i : media_actors) {
                media_[i.uuid()] = i.actor();
                link_to(i.actor());
                base_.insert_media(i.uuid(), uuid_before);
                join_event_group(this, i.actor());
            }

            // async caf system means we reverse iterate through our list and use 'await' so
            // that we are guaranteed to add them (in the innermost request response) in the
            // correct order
            for (auto i : media_actors) {
                request(
                    i.actor(),
                    infinite,
                    media::acquire_media_detail_atom_v,
                    base_.playhead_rate())
                    .then(

                        [=](const bool) mutable {
                            request(i.actor(), infinite, media_hook::get_media_hook_atom_v)
                                .then(
                                    [=](bool) mutable {
                                        // media_[media_actor.first] = media_actor.second;
                                        // link_to(media_actor.second);
                                        // base_.insert_media(media_actor.first, uuid_before);
                                        (*source_count)--;
                                        if (!(*source_count)) {
                                            // we're done!
                                            // send_content_changed_event();
                                            if (is_in_viewer_)
                                                open_media_reader(media_actors[0].actor());
                                            rp.deliver(true);
                                            for (auto i : media_actors) {
                                                send(
                                                    playlist_broadcast_,
                                                    utility::event_atom_v,
                                                    add_media_atom_v,
                                                    i);
                                            }
                                            send_content_changed_event();
                                        }
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        (*source_count)--;
                                        if (!(*source_count)) {
                                            // we're done!
                                            send_content_changed_event();
                                            if (is_in_viewer_)
                                                open_media_reader(media_actors[0].actor());
                                            rp.deliver(true);
                                        }
                                    });
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            (*source_count)--;
                            if (!(*source_count)) {
                                // we're done!
                                // send_content_changed_event();
                                if (is_in_viewer_)
                                    open_media_reader(media_actors[0].actor());
                                rp.deliver(true);
                            }
                            // rp.deliver(std::move(err));
                        });
            }
            return rp;
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &) {},

        [=](convert_to_contact_sheet_atom,
            utility::Uuid uuid,
            const std::string &name,
            const utility::Uuid &uuid_before) -> result<utility::UuidUuidActor> {
            auto rp = make_response_promise<utility::UuidUuidActor>();

            request(
                actor_cast<caf::actor>(this),
                infinite,
                create_contact_sheet_atom_v,
                name,
                uuid_before,
                false)
                .then(
                    [=](const utility::UuidUuidActor &result) mutable {
                        rp.deliver(result);
                        // clone data from target and inject into new actor
                        auto src_container = base_.containers().cfind_any(uuid);

                        if (src_container) {
                            request(
                                container_[(*src_container)->value().uuid()],
                                infinite,
                                get_media_atom_v)
                                .then(
                                    [=](const std::vector<UuidActor> &media) mutable {
                                        anon_send(
                                            result.second.actor(),
                                            add_media_atom_v,
                                            media,
                                            Uuid());
                                        anon_send(
                                            actor_cast<caf::actor>(this),
                                            reflag_container_atom_v,
                                            (*src_container)->value().flag(),
                                            result.first);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    },

                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](convert_to_timeline_atom,
            const utility::Uuid &uuid,
            const std::string &name,
            const utility::Uuid &uuid_before) -> result<utility::UuidUuidActor> {
            auto rp = make_response_promise<utility::UuidUuidActor>();

            request(
                actor_cast<caf::actor>(this),
                infinite,
                create_timeline_atom_v,
                name,
                uuid_before,
                false)
                .then(
                    [=](const utility::UuidUuidActor &result) mutable {
                        rp.deliver(result);
                        // clone data from target and inject into new actor
                        auto src_container = base_.containers().cfind_any(uuid);

                        if (src_container) {
                            request(
                                container_[(*src_container)->value().uuid()],
                                infinite,
                                get_media_atom_v)
                                .then(
                                    [=](const std::vector<UuidActor> &media) mutable {
                                        anon_send(
                                            result.second.actor(),
                                            add_media_atom_v,
                                            media,
                                            Uuid());
                                        anon_send(
                                            actor_cast<caf::actor>(this),
                                            reflag_container_atom_v,
                                            (*src_container)->value().flag(),
                                            result.first);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    },

                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](convert_to_subset_atom,
            const utility::Uuid &uuid,
            const std::string &name,
            const utility::Uuid &uuid_before) -> result<utility::UuidUuidActor> {
            auto rp = make_response_promise<utility::UuidUuidActor>();

            request(
                actor_cast<caf::actor>(this),
                infinite,
                create_subset_atom_v,
                name,
                uuid_before,
                false)
                .then(
                    [=](const utility::UuidUuidActor &result) mutable {
                        rp.deliver(result);
                        // clone data from target and inject into new actor
                        auto src_container = base_.containers().cfind_any(uuid);

                        if (src_container) {
                            request(
                                container_[(*src_container)->value().uuid()],
                                infinite,
                                get_media_atom_v)
                                .then(
                                    [=](const std::vector<UuidActor> &media) mutable {
                                        anon_send(
                                            result.second.actor(),
                                            add_media_atom_v,
                                            media,
                                            Uuid());
                                        anon_send(
                                            actor_cast<caf::actor>(this),
                                            reflag_container_atom_v,
                                            (*src_container)->value().flag(),
                                            result.first);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    },

                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](copy_container_atom atom, caf::actor bookmarks) {
            delegate(
                actor_cast<caf::actor>(this), atom, std::vector<utility::Uuid>(), bookmarks);
        },

        [=](copy_container_atom,
            const std::vector<utility::Uuid> &cuuids,
            caf::actor bookmarks) -> result<utility::CopyResult> {
            auto rp = make_response_promise<utility::CopyResult>();

            copy_container(cuuids, bookmarks, rp);

            return rp;
        },

        [=](create_contact_sheet_atom,
            const std::string &name,
            const utility::Uuid &uuid_before,
            const bool into) -> result<utility::UuidUuidActor> {
            // try insert as requested, but add to end if it fails.
            auto rp    = make_response_promise<utility::UuidUuidActor>();
            auto actor = spawn<contact_sheet::ContactSheetActor>(this, name);
            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            create_container(actor, rp, uuid_before, into);
            return rp;
        },

        [=](create_container_atom,
            caf::actor actor,
            const utility::Uuid &uuid_before,
            const bool into) -> result<utility::UuidUuidActor> {
            // try insert as requested, but add to end if it fails.
            auto rp = make_response_promise<utility::UuidUuidActor>();
            create_container(actor, rp, uuid_before, into);
            return rp;
        },

        [=](create_divider_atom,
            const std::string &name,
            const utility::Uuid &uuid_before,
            const bool into) -> result<Uuid> {
            auto i = base_.insert_divider(name, uuid_before, into);
            if (i) {
                send(event_group_, utility::event_atom_v, create_divider_atom_v, *i);
                base_.send_changed(event_group_, this);
                return *i;
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](create_group_atom,
            const std::string &name,
            const utility::Uuid &uuid_before) -> result<Uuid> {
            auto i = base_.insert_group(name, uuid_before);
            if (i) {
                send(event_group_, utility::event_atom_v, create_group_atom_v, *i);
                base_.send_changed(event_group_, this);
                return *i;
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](create_timeline_atom,
            const std::string &name,
            const utility::Uuid &uuid_before,
            const bool into) -> result<utility::UuidUuidActor> {
            // try insert as requested, but add to end if it fails.
            auto rp    = make_response_promise<utility::UuidUuidActor>();
            auto actor = spawn<timeline::TimelineActor>(
                name, utility::Uuid::generate(), actor_cast<caf::actor>(this));
            // anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            create_container(actor, rp, uuid_before, into);
            return rp;
        },

        [=](create_subset_atom,
            const std::string &name,
            const utility::Uuid &uuid_before,
            const bool into) -> result<utility::UuidUuidActor> {
            // try insert as requested, but add to end if it fails.
            auto rp    = make_response_promise<utility::UuidUuidActor>();
            auto actor = spawn<subset::SubsetActor>(this, name);
            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            create_container(actor, rp, uuid_before, into);
            return rp;
        },

        [=](json_store::get_json_atom atom, const std::string &path) {
            delegate(json_store_, atom, path);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            delegate(json_store_, atom, json, path);
        },

        [=](duplicate_atom,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks) -> result<UuidActor> {
            auto uuid = utility::Uuid::generate();
            auto actor =
                spawn<PlaylistActor>(base_.name(), uuid, caf::actor_cast<caf::actor>(session_));
            anon_send(actor, session::media_rate_atom_v, base_.media_rate());
            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());

            caf::scoped_actor sys(system());

            auto json =
                request_receive<JsonStore>(*sys, json_store_, json_store::get_json_atom_v);
            anon_send(actor, json_store::set_json_atom_v, json, "");

            // clone media into new playlist
            // store mapping from old to new uuids
            UuidUuidMap media_map;
            for (const auto &i : base_.media()) {
                try {
                    // duplicate media
                    auto ua = request_receive<UuidUuidActor>(
                        *sys,
                        media_.at(i),
                        utility::duplicate_atom_v,
                        src_bookmarks,
                        dst_bookmarks);

                    // store new media uuid
                    media_map[i] = ua.second.uuid();

                    // add new media to new playlist
                    auto mua = request_receive<UuidActor>(
                        *sys, actor, add_media_atom_v, ua.second.actor(), Uuid());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }

            // clone subgroups / dividers

            try {
                // need to clone container tree..
                // if we process in order....
                for (const auto &i : base_.containers().uuids(true)) {
                    auto ii = base_.containers().cfind(i);
                    if (ii) {
                        // create in new playlist..
                        //  is actor ?
                        if (container_.count((*ii)->value().uuid())) {
                            auto ua = request_receive<UuidActor>(
                                *sys,
                                container_[(*ii)->value().uuid()],
                                utility::duplicate_atom_v);

                            // add to new playlist
                            auto nua = request_receive<UuidUuidActor>(
                                *sys,
                                actor,
                                create_container_atom_v,
                                ua.actor(),
                                Uuid(),
                                false);

                            // reassign media / reparent

                            request_receive<bool>(
                                *sys,
                                nua.second.actor(),
                                playhead::source_atom_v,
                                actor,
                                media_map);

                            request_receive<bool>(
                                *sys,
                                actor,
                                reflag_container_atom_v,
                                (*ii)->value().flag(),
                                nua.first);
                        } else {
                            // add to new playlist
                            auto u = request_receive<utility::Uuid>(
                                *sys,
                                actor,
                                create_divider_atom_v,
                                (*ii)->value().name(),
                                Uuid(),
                                false);
                            request_receive<bool>(
                                *sys, actor, reflag_container_atom_v, (*ii)->value().flag(), u);
                        }
                    }
                }

                // have we really changed ?
                base_.send_changed(event_group_, this);
                return utility::UuidActor(uuid, actor);

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        // is this actually used ?
        [=](duplicate_atom,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks,
            const bool /*include_self*/) -> result<std::pair<UuidActor, UuidActor>> {
            auto rp = make_response_promise<std::pair<UuidActor, UuidActor>>();
            request(
                actor_cast<caf::actor>(this),
                infinite,
                duplicate_atom_v,
                src_bookmarks,
                dst_bookmarks)
                .then(
                    [=](const UuidActor &result) mutable {
                        rp.deliver(std::make_pair(
                            UuidActor(base_.uuid(), actor_cast<caf::actor>(this)), result));
                    },

                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](duplicate_container_atom,
            const utility::Uuid &uuid,
            const utility::Uuid &uuid_before,
            const bool into) -> result<UuidVector> {
            // may just be cosmetic, or possible involve actors.
            auto rp = make_response_promise<UuidVector>();
            if (base_.containers().count(uuid)) {
                duplicate_container(rp, uuid, uuid_before, into);
                return rp;
            }
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid & /*bookmark_uuid*/) {},
        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &,
            const media::MediaType) {
            base_.send_changed(event_group_, this);
            send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
        },

        [=](get_change_event_group_atom) -> caf::actor { return change_event_group_; },

        [=](get_container_atom) -> PlaylistTree { return base_.containers(); },

        // unused..// [=](get_container_atom, const utility::Uuid &uuid) -> result<caf::actor>
        // {// if(container_.count(uuid))// 		return container_[uuid];// 	return
        // make_error(xstudio_error::error, "Invalid uuid");// },
        [=](get_container_atom, const Uuid &uuid) -> caf::result<PlaylistTree> {
            auto found = base_.containers().cfind(uuid);
            if (not found) {
                return make_error(xstudio_error::error, "Container not found");
            }

            return PlaylistTree(**found);
        },

        [=](get_container_atom, const bool) -> std::vector<UuidActor> {
            std::vector<UuidActor> actors;
            for (const auto &i : container_)
                actors.emplace_back(i.first, i.second);
            return actors;
        },

        [=](get_media_atom) -> std::vector<UuidActor> {
            std::vector<UuidActor> actors;
            for (const auto &i : base_.media())
                actors.emplace_back(i, media_.at(i));
            return actors;
        },

        [=](get_media_atom,
            const utility::UuidList &selection) -> result<std::vector<UuidActor>> {
            std::vector<UuidActor> actors;
            for (const auto &i : selection) {
                if (media_.find(i) == media_.end()) {
                    return make_error(
                        xstudio_error::error,
                        "Request for media with given uuid not found in this playlist.");
                }
                actors.emplace_back(i, media_.at(i));
            }
            return actors;
        },

        [=](get_media_atom, const bool) -> result<std::vector<ContainerDetail>> {
            if (not media_.empty()) {
                auto rp = make_response_promise<std::vector<ContainerDetail>>();
                // collect media data..
                std::vector<caf::actor> actors;
                for (const auto &i : media_)
                    actors.push_back(i.second);

                fan_out_request<policy::select_all>(actors, infinite, utility::detail_atom_v)
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

        [=](get_media_atom, const utility::Uuid &uuid) -> result<caf::actor> {
            if (media_.count(uuid))
                return media_.at(uuid);
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](get_media_atom, const utility::Uuid &uuid, bool /*return_null*/) -> caf::actor {
            if (media_.count(uuid))
                return media_.at(uuid);
            return caf::actor();
        },

        [=](get_next_media_atom,
            const utility::Uuid &after_this_uuid,
            int skip_by) -> result<UuidActor> {
            const utility::UuidList media = base_.media();
            if (skip_by > 0) {
                auto i = std::find(media.begin(), media.end(), after_this_uuid);
                if (i == media.end()) {
                    // not found!
                    return make_error(
                        xstudio_error::error,
                        "playlist::get_next_media_atom called with uuid that is not in "
                        "playlist");
                }
                while (skip_by--) {
                    i++;
                    if (i == media.end()) {
                        i--;
                        break;
                    }
                }
                if (media_.count(*i))
                    return UuidActor(*i, media_.at(*i));

            } else {
                auto i = std::find(media.rbegin(), media.rend(), after_this_uuid);
                if (i == media.rend()) {
                    // not found!
                    return make_error(
                        xstudio_error::error,
                        "playlist::get_next_media_atom called with uuid that is not in "
                        "playlist");
                }
                while (skip_by++) {
                    i++;
                    if (i == media.rend()) {
                        i--;
                        break;
                    }
                }

                if (media_.count(*i))
                    return UuidActor(*i, media_.at(*i));
            }

            return make_error(
                xstudio_error::error,
                "playlist::get_next_media_atom called with uuid for which no media actor "
                "exists");
        },

        [=](insert_container_atom,
            const utility::CopyResult &cr,
            const utility::Uuid &before,
            const bool into) -> result<UuidVector> {
            auto rp = make_response_promise<UuidVector>();

            base_.insert_container(cr.tree_, before, into);

            // for(const auto &m : cr.media_)
            //     spdlog::warn("add new media {} {}", to_string(m.first), to_string(m.second));

            request(
                actor_cast<caf::actor>(this),
                infinite,
                playlist::add_media_atom_v,
                cr.media_,
                utility::Uuid())
                .then(
                    [=](const bool) mutable {
                        // now add containers actors and remap sources.
                        for (const auto &i : cr.containers_) {
                            container_[i.uuid()] = i.actor();
                            link_to(i.actor());
                            join_event_group(this, i.actor());
                            anon_send(
                                i.actor(),
                                playhead::source_atom_v,
                                actor_cast<caf::actor>(this),
                                cr.media_map_);
                        }
                        notify_tree(cr.tree_);
                        base_.send_changed(event_group_, this);
                        rp.deliver(cr.tree_.children_uuid(true));
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
            return rp;
        },

        [=](loading_media_atom atom, bool is_loading) {
            send(event_group_, utility::event_atom_v, atom, is_loading);
        },

        [=](media::get_edit_list_atom, const utility::Uuid &uuid) -> result<utility::EditList> {
            std::vector<caf::actor> actors;
            for (const auto &i : base_.media())
                actors.push_back(media_.at(i));

            if (not actors.empty()) {
                auto rp = make_response_promise<utility::EditList>();

                fan_out_request<policy::select_all>(
                    actors, infinite, media::get_edit_list_atom_v, Uuid())
                    .then(
                        [=](std::vector<utility::EditList> sections) mutable {
                            utility::EditList ordered_sections;
                            for (const auto &i : base_.media()) {
                                for (const auto &ii : sections) {
                                    const auto &[ud, rt, tc] = ii.section_list()[0];
                                    if (ud == i) {
                                        if (uuid.is_null())
                                            ordered_sections.push_back(ii.section_list()[0]);
                                        else
                                            ordered_sections.push_back({uuid, rt, tc});
                                        break;
                                    }
                                }
                            }
                            rp.deliver(ordered_sections);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }

            return result<utility::EditList>(utility::EditList());
        },

        [=](media::media_reference_atom) -> result<std::vector<MediaReference>> {
            std::vector<caf::actor> actors;
            for (const auto &i : base_.media())
                actors.push_back(media_.at(i));

            if (not actors.empty()) {
                auto rp = make_response_promise<std::vector<MediaReference>>();
                fan_out_request<policy::select_all>(
                    actors, infinite, media::media_reference_atom_v)
                    .then(
                        [=](std::vector<std::vector<MediaReference>> refs) mutable {
                            // flatten...
                            std::vector<MediaReference> flat_refs;
                            for (const auto &r : refs)
                                flat_refs.insert(flat_refs.end(), r.begin(), r.end());
                            rp.deliver(flat_refs);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }
            return result<std::vector<MediaReference>>(std::vector<MediaReference>());
        },

        [=](media_content_changed_atom) {
            // send_content_changed_event();
            send_content_changed_event(false);
        },

        [=](move_container_atom,
            const utility::Uuid &uuid,
            const utility::Uuid &uuid_before,
            const bool into) -> bool {
            bool result = base_.move_container(uuid, uuid_before, into);
            if (result) {
                send(
                    event_group_,
                    utility::event_atom_v,
                    move_container_atom_v,
                    uuid,
                    uuid_before,
                    into);
                base_.send_changed(event_group_, this);
            }
            return result;
        },

        [=](playlist::move_media_atom atom, const Uuid &uuid, const Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this), atom, utility::UuidVector({uuid}), uuid_before);
        },

        [=](playlist::move_media_atom atom,
            const UuidList &media_uuids,
            const Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                utility::UuidVector(media_uuids.begin(), media_uuids.end()),
                uuid_before);
        },

        [=](playlist::move_media_atom,
            const UuidVector &media_uuids,
            const Uuid &uuid_before) -> bool {
            bool result = false;
            for (auto uuid : media_uuids) {
                result |= base_.move_media(uuid, uuid_before);
            }
            if (result) {
                base_.send_changed(event_group_, this);
                send_content_changed_event();
            }
            return result;
        },

        [=](playhead::playhead_rate_atom) -> FrameRate { return base_.playhead_rate(); },

        [=](playhead::playhead_rate_atom, const FrameRate &rate) {
            base_.set_playhead_rate(rate);
            base_.send_changed(event_group_, this);
        },

        // create a new timeline, attach it to new playhead.
        [=](playlist::create_playhead_atom) -> result<utility::UuidActor> {
            if (playhead_)
                return playhead_;

            std::stringstream ss;
            ss << base_.name() << " Playhead";
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<playhead::PlayheadActor>(ss.str(), selection_actor_, uuid);
            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            playhead_ = UuidActor(uuid, actor);
            monitor(actor);
            base_.send_changed(event_group_, this);
            return playhead_;
        },

        [=](playlist::get_playhead_atom) {
            delegate(caf::actor_cast<caf::actor>(this), playlist::create_playhead_atom_v);
        },

        [=](playlist::selection_actor_atom) -> caf::actor { return selection_actor_; },

        // Get global reader to open readers for all media in this playlist - this// should mean
        // user can jump through media more quickly as ffmpeg handles// will already be open.
        [=](playlist::set_playlist_in_viewer_atom, const bool is_in_viewer) {
            is_in_viewer_ = is_in_viewer;
            if (is_in_viewer)
                open_media_readers();
        },

        [=](reflag_container_atom, const std::string &flag, const utility::Uuid &uuid) -> bool {
            bool result = base_.reflag_container(flag, uuid);
            if (result) {
                send(event_group_, utility::event_atom_v, reflag_container_atom_v, uuid, flag);
                base_.send_changed(event_group_, this);
            }
            return result;
        },

        [=](remove_container_atom atom, const utility::Uuid &uuid) {
            delegate(actor_cast<caf::actor>(this), atom, uuid, false);
        },

        [=](remove_container_atom,
            const utility::UuidVector &cuuids,
            const bool remove_orphans) -> bool {
            bool something_removed = false;

            for (const auto &i : cuuids) {
                if (base_.cfind(i)) {
                    something_removed = true;
                    anon_send(
                        actor_cast<caf::actor>(this),
                        remove_container_atom_v,
                        i,
                        remove_orphans);
                }
            }

            return something_removed;
        },

        [=](remove_container_atom,
            const utility::Uuid &cuuid,
            const bool remove_orphans) -> bool {
            bool result = false;
            UuidVector check_media;
            // if group, remove children first..
            // should wait for those to succeed...
            caf::scoped_actor sys(system());

            // uuid of actor
            auto children = base_.container_children(cuuid);
            if (children) {
                for (const auto &i : *children) {
                    if (container_.count(i)) {
                        auto a = container_[i];
                        if (remove_orphans) {
                            try {
                                auto uv =
                                    request_receive<UuidVector>(*sys, a, get_media_uuid_atom_v);
                                check_media.insert(check_media.end(), uv.begin(), uv.end());
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        unlink_from(a);
                        container_.erase(container_.find(i));
                        send_exit(a, caf::exit_reason::user_shutdown);
                    }
                }
            }

            if (base_.cfind(cuuid)) {
                auto auuid = (*base_.cfind(cuuid))->value().uuid();
                if (base_.remove_container(cuuid)) {
                    result = true;
                    if (container_.count(auuid)) {
                        auto a = container_[auuid];
                        if (remove_orphans) {
                            try {
                                auto uv =
                                    request_receive<UuidVector>(*sys, a, get_media_uuid_atom_v);
                                check_media.insert(check_media.end(), uv.begin(), uv.end());
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        unlink_from(a);
                        container_.erase(container_.find(auuid));
                        send_exit(a, caf::exit_reason::user_shutdown);
                    }
                }
            }

            if (result) {
                if (remove_orphans && !check_media.empty()) {
                    // for (const auto &i : check_media)
                    //     spdlog::info("candiate {}", to_string(i));
                    anon_send(actor_cast<caf::actor>(this), remove_orphans_atom_v, check_media);
                }
                send(event_group_, utility::event_atom_v, remove_container_atom_v, cuuid);
                base_.send_changed(event_group_, this);
            }

            return result;
        },

        [=](media_hook::gather_media_sources_atom, const utility::UuidVector &uuids) {
            for (const auto &uuid : uuids) {
                if (media_.count(uuid)) {
                    anon_send(
                        caf::actor_cast<caf::actor>(session_),
                        media_hook::gather_media_sources_atom_v,
                        media_.at(uuid),
                        base_.media_rate());
                }
            }
        },

        [=](media_hook::gather_media_sources_atom, const utility::UuidList &uuids) {
            for (const auto &uuid : uuids) {
                if (media_.count(uuid)) {
                    anon_send(
                        caf::actor_cast<caf::actor>(session_),
                        media_hook::gather_media_sources_atom_v,
                        media_.at(uuid),
                        base_.media_rate());
                }
            }
        },

        [=](remove_media_atom, const utility::Uuid &uuid) -> bool {
            // this needs to propergate to children somehow..
            if (base_.remove_media(uuid)) {
                auto a = media_.at(uuid);
                media_.erase(media_.find(uuid));
                unlink_from(a);
                send_exit(a, caf::exit_reason::user_shutdown);
                send_content_changed_event();
                base_.send_changed(event_group_, this);

                // send remove to all children ?
                // std::vector<caf::actor> actors;
                // for(const auto &i : container_)
                // 	actors.push_back(i.second);

                // if(actors.empty()) {
                // 	send_exit(a, caf::exit_reason::user_shutdown);
                // 	return result<bool>(true);
                // }
                // auto rp = make_response_promise<bool>();

                // fan_out_request<policy::select_all>(actors, infinite, remove_media_atom_v,
                // uuid).await(
                // 	[=](std::vector<bool>) mutable {
                // 		send_exit(a, caf::exit_reason::user_shutdown);
                //        	rp.deliver(true);
                // 	},
                //        [=](error&) mutable {
                // 		send_exit(a, caf::exit_reason::user_shutdown);
                //        	rp.deliver(true);
                //       	}
                // );

                return true;
            }

            return false;
        },

        [=](media::rescan_atom atom,
            const utility::Uuid &media_uuid) -> result<MediaReference> {
            if (not media_.count(media_uuid))
                return make_error(xstudio_error::error, "Invalid Uuid.");
            auto rp = make_response_promise<MediaReference>();
            rp.delegate(media_.at(media_uuid), atom);
            return rp;
        },

        [=](media::decompose_atom, const utility::Uuid &media_uuid) -> result<UuidActorVector> {
            if (not media_.count(media_uuid))
                return make_error(xstudio_error::error, "Invalid Uuid.");
            auto rp = make_response_promise<UuidActorVector>();

            request(media_.at(media_uuid), infinite, media::decompose_atom_v)
                .then(
                    [=](const UuidActorVector &result) mutable {
                        if (not result.empty())
                            anon_send(
                                caf::actor_cast<caf::actor>(this),
                                add_media_atom_v,
                                result,
                                base_.next_media(media_uuid));
                        rp.deliver(result);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](remove_media_atom atom, const utility::UuidList &uuids) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                utility::UuidVector(uuids.begin(), uuids.end()));
        },

        [=](remove_media_atom, const utility::UuidVector &uuids) -> bool {
            // this needs to propergate to children somehow..
            bool changed = false;
            for (const auto &uuid : uuids) {
                if (base_.remove_media(uuid)) {
                    changed = true;
                    auto a  = media_.at(uuid);
                    media_.erase(media_.find(uuid));
                    unlink_from(a);
                    send_exit(a, caf::exit_reason::user_shutdown);
                }
            }
            if (changed) {
                send_content_changed_event();
                base_.send_changed(event_group_, this);
            }
            return changed;
        },

        [=](remove_orphans_atom, const utility::UuidVector &uuids) {
            // scan all our children to compile list of media that are in use.
            std::vector<caf::actor> actors;
            for (const auto &i : container_)
                actors.push_back(i.second);
            if (!actors.empty()) {
                // turn into set..
                std::set<utility::Uuid> candidates;
                std::copy(
                    uuids.begin(), uuids.end(), std::inserter(candidates, candidates.end()));

                // for (const auto &i : candidates)
                //     spdlog::info("candidates {}", to_string(i));

                // request all container media..
                fan_out_request<policy::select_all>(actors, infinite, get_media_uuid_atom_v)
                    .then(
                        [=](const std::vector<UuidVector> media_uuids) mutable {
                            std::set<utility::Uuid> not_candidates;
                            for (const auto &i : media_uuids)
                                std::copy(
                                    i.begin(),
                                    i.end(),
                                    std::inserter(not_candidates, not_candidates.end()));

                            // for (const auto &i : not_candidates)
                            //     spdlog::info("used {}", to_string(i));

                            std::set<utility::Uuid> result;
                            std::set_difference(
                                candidates.begin(),
                                candidates.end(),
                                not_candidates.begin(),
                                not_candidates.end(),
                                std::inserter(result, result.end()));

                            // for (const auto &i : result)
                            //     spdlog::info("to_remove {}", to_string(i));

                            // list of media to remove
                            for (const auto &i : result)
                                anon_send(actor_cast<caf::actor>(this), remove_media_atom_v, i);
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            } else {
                for (const auto &i : uuids)
                    anon_send(actor_cast<caf::actor>(this), remove_media_atom_v, i);
            }
        },

        [=](rename_container_atom, const std::string &name, const utility::Uuid &uuid) -> bool {
            bool result = base_.rename_container(name, uuid);
            if (result) {
                send(event_group_, utility::event_atom_v, rename_container_atom_v, uuid, name);
                base_.send_changed(event_group_, this);
            }
            return result;
        },

        [=](session::media_rate_atom) -> FrameRate { return base_.media_rate(); },
        [=](session::session_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(session_);
        },

        // ignore child events..
        [=](session::media_rate_atom, const FrameRate &rate) {
            base_.set_media_rate(rate);
            base_.send_changed(event_group_, this);
        },

        [=](sort_alphabetically_atom) { sort_alphabetically(); },

        [=](utility::event_atom, playlist::remove_media_atom, const UuidVector &) {},
        [=](utility::event_atom, playlist::add_media_atom, const utility::UuidActor &ua) {
            send(event_group_, utility::event_atom_v, add_media_atom_v, ua);
        },
        [=](utility::event_atom, playlist::move_media_atom, const UuidVector &, const Uuid &) {
        },

        [=](utility::event_atom, utility::change_atom) {
            // one of our media changed..
            // this gets spammy...
            // buffer it ?
            send_content_changed_event();
        },

        // watch for events...
        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                auto_gather_sources_ =
                    preference_value<bool>(j, "/core/media_reader/auto_gather_sources");
            } catch (...) {
            }
        },

        [=](session::import_atom,
            const caf::uri &path,
            const Uuid &uuid_before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            try {
                caf::scoped_actor sys(system());
                auto global = system().registry().template get<caf::actor>(global_registry);
                auto epa = request_receive<caf::actor>(*sys, global, global::get_python_atom_v);

                // request otio xml from embedded_python.
                request(epa, infinite, session::import_atom_v, path)
                    .then(
                        [=](const std::string &data) mutable {
                            // got data create timeline and load it..
                            const auto name = fs::path(uri_to_posix_path(path)).stem().string();

                            request(
                                actor_cast<caf::actor>(this),
                                infinite,
                                create_timeline_atom_v,
                                name,
                                uuid_before,
                                false)
                                .then(
                                    [=](const utility::UuidUuidActor &uua) mutable {
                                        // request loading of timeline
                                        anon_send(
                                            uua.second.actor(), session::import_atom_v, data);
                                        rp.deliver(uua.second);
                                    },
                                    [=](error &err) mutable { rp.deliver(std::move(err)); });
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, err.what()));
            }

            return rp;
        },

        // handle child change events.
        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            request(json_store_, infinite, json_store::get_json_atom_v, "")
                .then(
                    [=](const JsonStore &meta) mutable {
                        std::vector<caf::actor> clients;
                        for (const auto &i : media_)
                            clients.push_back(i.second);
                        for (const auto &i : container_)
                            clients.push_back(i.second);
                        if (playhead_)
                            clients.push_back(playhead_);

                        if (not clients.empty()) {

                            fan_out_request<policy::select_all>(
                                clients, infinite, serialise_atom_v)
                                .then(
                                    [=](std::vector<JsonStore> json) mutable {
                                        JsonStore jsn;
                                        jsn["store"]     = meta;
                                        jsn["base"]      = base_.serialise();
                                        jsn["playheads"] = {};
                                        jsn["actors"]    = {};
                                        for (const auto &j : json) {
                                            if (j["base"]["container"]["type"]
                                                    .get<std::string>() == "Playhead") {
                                                jsn["playheads"][static_cast<std::string>(
                                                    j["base"]["container"]["uuid"])] = j;

                                            } else {
                                                jsn["actors"][static_cast<std::string>(
                                                    j["base"]["container"]["uuid"])] = j;
                                            }
                                        }
                                        rp.deliver(jsn);
                                    },
                                    [=](error &err) mutable { rp.deliver(std::move(err)); });

                        } else {
                            JsonStore jsn;
                            jsn["store"]  = meta;
                            jsn["base"]   = base_.serialise();
                            jsn["actors"] = {};
                            rp.deliver(jsn);
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        }

    );
}

void PlaylistActor::on_exit() {
    // shutdown playhead
    send_exit(playhead_.actor(), caf::exit_reason::user_shutdown);
}

void PlaylistActor::create_container(
    caf::actor actor,
    caf::typed_response_promise<UuidUuidActor> rp,
    const utility::Uuid &uuid_before,
    const bool into) {
    request(actor, infinite, detail_atom_v)
        .await(
            [=](const ContainerDetail &detail) mutable {
                container_[detail.uuid_] = actor;
                link_to(actor);
                join_event_group(this, actor);
                PlaylistItem tmp(detail.name_, detail.type_, detail.uuid_);

                auto cuuid = base_.insert_container(tmp, uuid_before, into);
                if (not cuuid) {
                    cuuid = base_.insert_container(tmp);
                }

                if (detail.type_ == "Subset")
                    send(
                        event_group_,
                        utility::event_atom_v,
                        create_subset_atom_v,
                        UuidActor(detail.uuid_, actor));
                else if (detail.type_ == "ContactSheet")
                    send(
                        event_group_,
                        utility::event_atom_v,
                        create_contact_sheet_atom_v,
                        UuidActor(detail.uuid_, actor));
                else if (detail.type_ == "Timeline")
                    send(
                        event_group_,
                        utility::event_atom_v,
                        create_timeline_atom_v,
                        UuidActor(detail.uuid_, actor));

                base_.send_changed(event_group_, this);
                rp.deliver(std::make_pair(*cuuid, UuidActor(detail.uuid_, actor)));
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void PlaylistActor::add_media(
    UuidActor &ua,
    const utility::Uuid &uuid_before,
    caf::typed_response_promise<UuidActor> rp) {

    media_[ua.uuid()] = ua.actor();
    join_event_group(this, ua.actor());
    link_to(ua.actor());
    base_.insert_media(ua.uuid(), uuid_before);
    send(event_group_, utility::event_atom_v, add_media_atom_v, ua);
    send(playlist_broadcast_, utility::event_atom_v, add_media_atom_v, ua);

    request(ua.actor(), infinite, media::acquire_media_detail_atom_v, base_.playhead_rate())
        .then(
            [=](const bool) mutable {
                request(ua.actor(), infinite, media_hook::get_media_hook_atom_v)
                    .then(
                        [=](bool) {},
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });

                // send(event_group_, utility::event_atom_v, add_media_atom_v, ua);
                send_content_changed_event();
                base_.send_changed(event_group_, this);
                rp.deliver(ua);
                if (is_in_viewer_)
                    open_media_reader(ua.actor());
            },
            [=](error &err) mutable {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, to_string(err), to_string(ua.actor()));
                send_content_changed_event();
                base_.send_changed(event_group_, this);
                rp.deliver(ua);
            });
}

// this is gonna be fun...
// we need to serialise all items under and including uuid
// and also change uuids, and update linkage..
// not sure how we deal with hidden children
// I think the actors themselves need to support a clone atom..

void PlaylistActor::duplicate_container(
    caf::typed_response_promise<UuidVector> rp,
    const utility::Uuid &uuid,
    const utility::Uuid &uuid_before,
    const bool into) {
    // find all actor children.. (Timeline/SUBSET/CONTACTSHEET)
    // quick clone of divider..

    // uuid is valid..
    auto old = base_.containers().cfind(uuid);

    // clone structure
    PlaylistTree new_tree(**old);
    // regenerate uuids..
    new_tree.reset_uuid(true);

    duplicate_tree(new_tree);
    notify_tree(new_tree);
    base_.insert_container(new_tree, uuid_before, into);

    base_.send_changed(event_group_, this);
    std::function<void(const PlaylistTree &, std::vector<Uuid> &)> flatten_tree;
    flatten_tree = [&flatten_tree](const PlaylistTree &tree, std::vector<Uuid> &rt) {
        rt.push_back(tree.uuid());
        for (auto i : tree.children_ref()) {
            flatten_tree(i, rt);
        }
    };

    std::vector<Uuid> ordered_uuids;
    flatten_tree(new_tree, ordered_uuids);

    rp.deliver(ordered_uuids);
}

// session actors container groups/dividers and playlists..
void PlaylistActor::notify_tree(const utility::UuidTree<utility::PlaylistItem> &tree) {
    if (tree.value().type() == "ContainerDivider") {
        send(event_group_, utility::event_atom_v, playlist::create_divider_atom_v, tree.uuid());
    } else if (tree.value().type() == "ContainerGroup") {
        send(event_group_, utility::event_atom_v, playlist::create_group_atom_v, tree.uuid());
    } else if (
        tree.value().type() == "Subset" || tree.value().type() == "ContactSheet" ||
        tree.value().type() == "Timeline") {

        // auto uua = UuidUuidActor({tree.uuid(),{tree.value().uuid(),
        // container_[tree.value().uuid()]}});
        auto ua = UuidActor(tree.value().uuid(), container_[tree.value().uuid()]);

        if (tree.value().type() == "Subset") {
            send(event_group_, utility::event_atom_v, create_subset_atom_v, ua);
        } else if (tree.value().type() == "ContactSheet") {
            send(event_group_, utility::event_atom_v, create_contact_sheet_atom_v, ua);
        } else if (tree.value().type() == "Timeline") {
            send(event_group_, utility::event_atom_v, create_timeline_atom_v, ua);
        }
    }

    for (const auto &i : tree.children_) {
        notify_tree(i);
    }
}


// session actors container groups/dividers and playlists..
void PlaylistActor::duplicate_tree(utility::UuidTree<utility::PlaylistItem> &tree) {
    tree.value().set_name(tree.value().name() + " - copy");

    auto type = tree.value().type();

    if (type == "ContainerDivider") {
        tree.value().set_uuid(Uuid::generate());
    } else if (type == "ContainerGroup") {
        tree.value().set_uuid(Uuid::generate());
    } else if (type == "Subset" || type == "ContactSheet" || type == "Timeline") {
        // need to issue a duplicate action, as we actors are blackboxes..
        // try not to confuse this with duplicating a container, as opposed to the actor..
        // we need to insert the new playlist in to the session and update the UUID
        try {
            caf::scoped_actor sys(system());
            auto result = request_receive<utility::UuidActor>(
                *sys, container_[tree.value().uuid()], duplicate_atom_v);

            if (type == "Timeline")
                anon_send(
                    result.actor(),
                    playhead::source_atom_v,
                    caf::actor_cast<caf::actor>(this),
                    UuidUuidMap());

            tree.value().set_uuid(result.uuid());
            container_[result.uuid()] = result.actor();
            link_to(result.actor());
            // update name
            auto name = request_receive<std::string>(*sys, result.actor(), name_atom_v);
            tree.value().set_name(name + " - copy");
            send(result.actor(), name_atom_v, tree.value().name());
            // send(event_group_, utility::event_atom_v, add_playlist_atom_v, result);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            // set to invalid uuid ?
            tree.value().set_uuid(Uuid());
        }
    }

    for (auto &i : tree.children_) {
        duplicate_tree(i);
    }
}

// void PlaylistActor::load_from_path(const caf::uri &path, const bool recursive) {
//     auto items = utility::scan_posix_path(uri_to_posix_path(path), recursive ? -1 : 0);
//     std::sort(
//         std::begin(items),
//         std::end(items),
//         []( std::pair<caf::uri, FrameList> a, std::pair<caf::uri, FrameList> b) {return
//         a.first < b.first; }
//     );

//     for(const auto &i : items) {
//         try {
//         	if(is_file_supported(i.first)){
// 	            anon_send(
// 	            	actor_cast<caf::actor>(this),
// 	            	playlist::add_media_atom_v,
// 	                "New media",
// 	                i.first,
// 	                i.second
// 	            );
// 	        } else {
// 	        	spdlog::warn("Unsupported file type {}.", to_string(i.first));
// 	        }
//         } catch(const std::exception &e) {
//             spdlog::error("Failed to create media {}", e.what());
//         }
//     }
// }

void PlaylistActor::open_media_readers() {

    const utility::UuidList media = base_.media();
    for (auto i : media) {
        if (media_.count(i)) {
            open_media_reader(media_.at(i));
        }
    }
}

void PlaylistActor::open_media_reader(caf::actor media_actor) {

#pragma message                                                                                \
    "Disabling auto initialising of readers, it hits IO too hard when playlist is large. Needs a rethink."
    return;

    auto global_reader = system().registry().template get<caf::actor>(media_reader_registry);
    if (not global_reader)
        return;

    /*
    // Disabling this idea for now - gets too heavy on IO front if the playlist
    // contains a lot of items.
    //
    // force load the first frame of every source in the playlist. This will open
    // the 'urgent' media reader for the media source ready for switching the source
    // in the UI

    request(media_actor, infinite, media::get_media_pointer_atom_v, 0).then(

            [=](const media::AVFrameID &media) mutable {

                    if (!media.is_nil()) {

                            anon_send(global_reader, media_reader::get_image_atom_v, media,
    true); std::string path = uri_to_posix_path(media.uri_);

                    }
            },
            [=](error&) mutable {
            }
    );*/

    // force cache the first frame of every source in the playlist. This will open
    // the precache media reader for the media source ready for playback
    request(media_actor, infinite, media::get_media_pointer_atom_v, 0)
        .then(

            [=](const media::AVFrameID &media) mutable {
                if (!media.is_nil()) {

                    anon_send(
                        global_reader,
                        media_reader::playback_precache_atom_v,
                        media,
                        utility::clock::now() +
                            std::chrono::seconds(
                                60), // saying we need this in 1 minute hence low priority
                        utility::Uuid());
                }
            },
            [=](error &) mutable {});
}

void PlaylistActor::send_content_changed_event(const bool queue) {
    if (not queue) {
        std::vector<UuidActor> actors;
        for (const auto &i : base_.media())
            actors.emplace_back(i, media_.at(i));

        send(event_group_, utility::event_atom_v, media_content_changed_atom_v, actors);
        send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
        content_changed_ = false;
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__,
        // to_string(caf::actor_cast<caf::actor>(this)));

    } else {
        if (not content_changed_) {
            content_changed_ = true;
            delayed_anon_send(
                this, std::chrono::milliseconds(100), media_content_changed_atom_v);
        }
    }
}

// how do we rewrite the media ids on the sub items, as they'll need to change, ah uuiduuidmap..
// need to pass that as well.. ? copy selection.. find items add to new tree. create new actors
// as required and build list of referenced media clone media and regenerate uuids. wonder if
// sub items should own their own media.. add media before reparenting, as that'll handle the
// relinking. this should support cloning entire content.. (no cuuids)

void PlaylistActor::copy_container(
    const std::vector<utility::Uuid> &cuuids,
    caf::actor bookmarks,
    caf::typed_response_promise<utility::CopyResult> rp) const {
    bool all_media = false;
    PlaylistTree new_tree("Copy", "Playlist");

    if (cuuids.empty()) {
        all_media = true;
        new_tree  = PlaylistTree(base_.containers());
    } else if (cuuids.size() == 1) {
        auto old = base_.containers().cfind_any(cuuids[0]);
        if (old)
            // clone structure
            new_tree = PlaylistTree(**old);
    } else {
        for (const auto &i : cuuids) {
            auto old = base_.containers().cfind_any(i);
            if (old)
                new_tree.insert(PlaylistTree(**old));
        }
    }

    // collect src containers.
    auto src_containers = get_containers(new_tree);

    // rewrite our cuuids.
    new_tree.reset_uuid(true);

    // should have a valid tree
    auto containers = copy_tree(new_tree);

    caf::scoped_actor sys(system());
    std::set<Uuid> media_uuids;

    // collect media id's
    if (all_media) {
        auto tmp    = base_.media();
        media_uuids = std::set<Uuid>(tmp.begin(), tmp.end());
    } else {
        // interogate containers.
        for (const auto &i : src_containers) {
            try {
                auto result =
                    request_receive<UuidActorVector>(*sys, i.actor(), get_media_atom_v);
                for (const auto &m : result) {
                    media_uuids.insert(m.uuid());
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }
    }

    // duplicate and add to media/media map in playlist order
    UuidActorVector media;
    UuidUuidMap media_map;

    for (const auto &m : base_.media()) {
        if (not media_uuids.count(m))
            continue;
        try {
            auto ua = request_receive<UuidUuidActor>(
                *sys, media_.at(m), utility::duplicate_atom_v, bookmarks, bookmarks);
            media_map[m] = ua.second.uuid();
            media.emplace_back(ua.second);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }
    rp.deliver(utility::CopyResult(new_tree, containers, media, media_map));
}

// session actors container groups/dividers and playlists..
UuidActorVector PlaylistActor::copy_tree(utility::UuidTree<utility::PlaylistItem> &tree) const {
    UuidActorVector uav;

    if (tree.value().type() == "ContainerDivider") {
        tree.value().set_uuid(Uuid::generate());
    } else if (tree.value().type() == "ContainerGroup") {
        tree.value().set_uuid(Uuid::generate());
    } else if (
        tree.value().type() == "Subset" || tree.value().type() == "ContactSheet" ||
        tree.value().type() == "Timeline") {
        // need to issue a duplicate action, as we actors are blackboxes..
        // try not to confuse this with duplicating a container, as opposed to the actor..
        // we need to insert the new playlist in to the session and update the UUID
        try {
            caf::scoped_actor sys(system());
            auto result = request_receive<utility::UuidActor>(
                *sys, container_.at(tree.value().uuid()), duplicate_atom_v);
            tree.value().set_uuid(result.uuid());
            uav.emplace_back(result);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            tree.value().set_uuid(Uuid());
        }
    }

    for (auto &i : tree.children_) {
        auto ii = copy_tree(i);
        uav.insert(uav.end(), ii.begin(), ii.end());
    }
    return uav;
}

utility::UuidActorVector
PlaylistActor::get_containers(utility::UuidTree<utility::PlaylistItem> &tree) const {
    UuidActorVector uav;

    if (tree.value().type() == "Subset" || tree.value().type() == "ContactSheet" ||
        tree.value().type() == "Timeline") {
        // need to issue a duplicate action, as we actors are blackboxes..
        // try not to confuse this with duplicating a container, as opposed to the actor..
        // we need to insert the new playlist in to the session and update the UUID
        uav.emplace_back(UuidActor(tree.value().uuid(), container_.at(tree.value().uuid())));
    }

    for (auto &i : tree.children_) {
        auto ii = get_containers(i);
        uav.insert(uav.end(), ii.begin(), ii.end());
    }
    return uav;
}

void PlaylistActor::sort_alphabetically() {

    using SourceAndUuid = std::pair<std::string, utility::Uuid>;
    auto media_names_vs_uuids =
        std::make_shared<std::vector<std::pair<std::string, utility::Uuid>>>();

    for (const auto &i : media_) {

        // Pro tip: because i is a reference, it's the reference that is captured in our lambda
        // below and therefore it is 'unstable' so we make a copy here and use that in the
        // lambda as this is object-copied in the capture instead.
        UuidActor media_actor(i.first, i.second);

        request(media_actor.actor(), infinite, media::media_reference_atom_v, utility::Uuid())
            .await(

                [=](const std::pair<Uuid, MediaReference> &m_ref) mutable {
                    std::string path = uri_to_posix_path(m_ref.second.uri());
                    path             = std::string(path, path.rfind("/") + 1);
                    path             = to_lower(path);

                    (*media_names_vs_uuids).push_back(std::make_pair(path, media_actor.uuid()));

                    if (media_names_vs_uuids->size() == media_.size()) {

                        std::sort(
                            media_names_vs_uuids->begin(),
                            media_names_vs_uuids->end(),
                            [](const SourceAndUuid &a, const SourceAndUuid &b) -> bool {
                                return a.first < b.first;
                            });

                        utility::UuidList ordered_uuids;
                        for (const auto &p : (*media_names_vs_uuids)) {
                            ordered_uuids.push_back(p.second);
                        }

                        anon_send(
                            caf::actor_cast<caf::actor>(this),
                            move_media_atom_v,
                            ordered_uuids,
                            utility::Uuid());
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}
