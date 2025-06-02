// SPDX-License-Identifier: Apache-2.0

#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>
#include <filesystem>

#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/contact_sheet/contact_sheet_actor.hpp"
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

const auto MEDIA_NOTIFY_DELAY_SLOW = std::chrono::seconds(2);
const auto MEDIA_NOTIFY_DELAY_FAST = std::chrono::milliseconds(100);

using namespace nlohmann;

void blocking_loader(
    blocking_actor *self,
    caf::typed_response_promise<std::vector<UuidActor>> rp,
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

    anon_mail(playlist::loading_media_atom_v, true).send(dst.actor());

    // auto event_msg = event::Event("Loading Playlist Media {}", 0, 0, 1, {dst.uuid()});

    // event::send_event(self, event_msg);

    auto items = utility::scan_posix_path(
        to_string(path).find("http") == 0 ? to_string(path) : uri_to_posix_path(path),
        recursive ? -1 : 0);
    std::sort(
        std::begin(items),
        std::end(items),
        [](std::pair<caf::uri, FrameList> a, std::pair<caf::uri, FrameList> b) {
            return a.first < b.first;
        });

    // event_msg.set_progress_maximum(items.size());
    // event::send_event(self, event_msg);

    for (const auto &i : items) {
        // event_msg.set_progress(event_msg.progress() + 1);
        // event::send_event(self, event_msg);

        try {
            if (is_file_supported(i.first)) {

                const caf::uri &uri         = i.first;
                const FrameList &frame_list = i.second;

                const auto uuid = Uuid::generate();
#ifdef _WIN32
                std::string ext = ltrim_char(
                    get_path_extension(to_upper_path(fs::path(uri_to_posix_path(uri)))), '.');
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

                // use the stem of the filepath as the name for the media item.
                // We do a further trim to the first . so that numbered paths
                // like 'some_exr.####.exr' becomes 'some_exr'
                const auto path    = fs::path(uri_to_posix_path(uri));
                auto filename_stem = path.stem().string();
                const auto dotpos  = filename_stem.find(".");
                if (dotpos && dotpos != std::string::npos) {
                    filename_stem = std::string(filename_stem, 0, dotpos);
                }

                auto media =
                    self->spawn<media::MediaActor>(filename_stem, uuid, UuidActorVector());

                self->mail(

                        media::add_media_source_atom_v,
                        UuidActorVector({UuidActor(source_uuid, source)}))
                    .request(media, infinite)
                    .receive(
                        [=](bool) {},
                        [=](error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });

                if (auto_gather)
                    anon_mail(media_hook::gather_media_sources_atom_v, media, default_rate)
                        .send(session);

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
                // anon_mail(media_metadata::get_metadata_atom_v).send(media);

                batched_media_to_add.emplace_back(ua);

                // we batch to try and cut down thrashing..
                if (batched_media_to_add.size() == 1) {
                    anon_mail(playlist::loading_media_atom_v, true).send(dst.actor());
                    self->mail(playlist::add_media_atom_v, batched_media_to_add, before)
                        .request(dst.actor(), infinite)
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
        self->mail(playlist::add_media_atom_v, batched_media_to_add, Uuid())
            .request(dst.actor(), infinite)
            .receive(
                [=](const bool) mutable {},
                [=](error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
    // event_msg.set_complete();
    // event::send_event(self, event_msg);

    // this message will update the 'loading_media_atom' status of the playlist in the UI
    anon_mail(playlist::loading_media_atom_v, false).send(dst.actor());
    // self->anon_mail(//
    // media_content_changed_atom_v).delay(std::chrono::seconds(1)).send(dst.actor());
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

    if (jsn.contains("playhead")) {
        playhead_serialisation_ = jsn["playhead"];
    }

    link_to(json_store_);
    join_event_group(this, json_store_);
    // media needs to exist before we can deserialise containers.
    spdlog::stopwatch sw;

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
    spdlog::debug("media loaded in {:.3} seconds.", sw);
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
                anon_mail(timeline::link_media_atom_v, media_, false).send(actor);
            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        } else if (value.at("base").at("container").at("type") == "PlayheadSelection") {

            try {

                selection_actor_ = system().spawn<playhead::PlayheadSelectionActor>(
                    static_cast<utility::JsonStore>(value), caf::actor_cast<caf::actor>(this));
                link_to(selection_actor_);

            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        }
    }
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
    join_event_group(this, json_store_);
    init();
}

PlaylistActor::~PlaylistActor() {}

caf::message_handler PlaylistActor::default_event_handler() {
    return caf::message_handler(
               {[=](utility::event_atom, change_atom) {},
                [=](utility::event_atom, add_media_atom, const utility::UuidActorVector &) {},
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
                [=](json_store::update_atom,
                    const JsonStore &,
                    const std::string &,
                    const JsonStore &) {},
                [=](json_store::update_atom, const JsonStore &) {},
                [=](utility::event_atom, create_subset_atom, const utility::UuidActor &) {},
                [=](utility::event_atom,
                    create_contact_sheet_atom,
                    const utility::UuidActor &) {},
                [=](utility::event_atom, create_timeline_atom, const utility::UuidActor &) {},
                [=](utility::event_atom,
                    media_content_changed_atom,
                    const utility::UuidActorVector &) {},
                [=](utility::event_atom,
                    move_container_atom,
                    const Uuid &,
                    const Uuid &,
                    const bool) {}})
        .or_else(NotificationHandler::default_event_handler())
        .or_else(Container::default_event_handler());
}

caf::message_handler PlaylistActor::message_handler() {
    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        make_ignore_error_handler(),

        [=](broadcast::join_broadcast_atom) -> caf::actor { return playlist_broadcast_; },

        [=](playlist::add_media_atom, utility::event_atom) {
            // delayed add media event..
            if (not delayed_add_media_.empty()) {
                mail(utility::event_atom_v, add_media_atom_v, delayed_add_media_)
                    .send(base_.event_group());
                mail(utility::event_atom_v, add_media_atom_v, delayed_add_media_)
                    .send(playlist_broadcast_);
                // spdlog::warn("delayed update {}", delayed_add_media_.size());
                delayed_add_media_.clear();
            }
        },

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
                anon_mail(media_hook::gather_media_sources_atom_v, media, rate)
                    .send(actor_cast<caf::actor>(session_));

            return mail(add_media_atom_v, UuidActor(uuid, media), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](add_media_atom,
            const std::string &name,
            const caf::uri &uri,
            const utility::Uuid &uuid_before) {
            return mail(add_media_atom_v, name, uri, FrameList(), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
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
            std::string ext = ltrim_char(
                to_upper(fs::path(uri_to_posix_path(uri)).extension().string()), '.');
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
                anon_mail(media_hook::gather_media_sources_atom_v, media, base_.media_rate())
                    .send(actor_cast<caf::actor>(session_));

            return mail(add_media_atom_v, UuidActor(uuid, media), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](add_media_atom atom,
            const caf::uri &path,
            const bool recursive,
            const utility::Uuid &uuid_before) {
            return mail(atom, path, recursive, base_.media_rate(), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](add_media_with_subsets_atom,
            const caf::uri &path,
            const utility::Uuid &uuid_before) -> result<std::vector<UuidActor>> {
            auto rp = make_response_promise<std::vector<utility::UuidActor>>();
            recursive_add_media_with_subsets(rp, path, uuid_before);
            return rp;
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

            delayed_add_media_.push_back(ua);
            mail(playlist::add_media_atom_v, utility::event_atom_v)
                .delay(MEDIA_NOTIFY_DELAY_FAST)
                .send(this);

            // mail(utility::event_atom_v, add_media_atom_v,
            // UuidActorVector({ua})).send(base_.event_group()); mail(utility::event_atom_v,
            // add_media_atom_v, UuidActorVector({ua})).send(playlist_broadcast_);
            // send_content_changed_event();
            base_.send_changed();
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
            }

            delayed_add_media_.insert(delayed_add_media_.end(), result.begin(), result.end());

            mail(playlist::add_media_atom_v, utility::event_atom_v)
                .delay(MEDIA_NOTIFY_DELAY_FAST)
                .send(this);

            // mail(utility::event_atom_v, add_media_atom_v, result).send(base_.event_group());
            // mail(utility::event_atom_v, add_media_atom_v, result).send(playlist_broadcast_);

            send_content_changed_event();
            base_.send_changed();
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
            // we batch notifying of media if size if greater than 50
            add_media(ua, before, final_ordered_uuid_list.size() > 50, rp);

            return rp;
        },

        [=](add_media_atom,
            UuidActor ua,
            const utility::Uuid &uuid_before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            add_media(ua, uuid_before, false, rp);
            return rp;
        },

        [=](add_media_atom,
            caf::actor actor,
            const utility::Uuid &uuid_before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            // we block other requests, as we don't wat the order beening messed up.
            mail(uuid_atom_v)
                .request(actor, infinite)
                .await(
                    [=](const utility::Uuid &uuid) mutable {
                        // hmm this is going to knacker ordering..
                        mail(add_media_atom_v, UuidActor(uuid, actor), uuid_before)
                            .request(actor_cast<caf::actor>(this), infinite)
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

            mail(utility::event_atom_v, loading_media_atom_v, true).send(base_.event_group());
            return rp;
        },

        [=](add_media_atom,
            const std::vector<UuidActor> &ma,
            const utility::Uuid &uuid_before) -> result<bool> {
            // before we can add media actors, we have to make sure the detail has been acquired
            // so that the duration of the media is known. This is because the playhead will
            // update and build a timeline as soon as the playlist notifies of change, so the
            // duration and frame rate must be known up-front
            std::vector<UuidActor> media_actors;
            for (const auto &media : ma) {
                if (!media_.count(media.uuid())) {
                    media_actors.push_back(media);
                }
            }
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
                mail(media::acquire_media_detail_atom_v, base_.playhead_rate())
                    .request(i.actor(), infinite)
                    .then(

                        [=](const bool) mutable {
                            mail(media_hook::get_media_hook_atom_v)
                                .request(i.actor(), infinite)
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

                                            delayed_add_media_.insert(
                                                delayed_add_media_.end(),
                                                media_actors.begin(),
                                                media_actors.end());

                                            mail(
                                                playlist::add_media_atom_v,
                                                utility::event_atom_v)
                                                .delay(MEDIA_NOTIFY_DELAY_FAST)
                                                .send(this);

                                            // mail(//     utility::event_atom_v,
                                            //     add_media_atom_v,
                                            //     media_actors).send(// base_.event_group());
                                            // mail(//     utility::event_atom_v,
                                            //     add_media_atom_v,
                                            //     media_actors).send(// playlist_broadcast_);
                                        }
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        (*source_count)--;
                                        if (!(*source_count)) {
                                            // we're done!
                                            // send_content_changed_event();
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

            mail(create_contact_sheet_atom_v, name, uuid_before, false)
                .request(actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const utility::UuidUuidActor &result) mutable {
                        rp.deliver(result);
                        // clone data from target and inject into new actor
                        auto src_container = base_.containers().cfind_any(uuid);

                        if (src_container) {
                            mail(get_media_atom_v)
                                .request(container_[(*src_container)->value().uuid()], infinite)
                                .then(
                                    [=](const std::vector<UuidActor> &media) mutable {
                                        anon_mail(add_media_atom_v, media, Uuid())
                                            .send(result.second.actor());
                                        anon_mail(
                                            reflag_container_atom_v,
                                            (*src_container)->value().flag(),
                                            result.first)
                                            .send(actor_cast<caf::actor>(this));
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

            mail(create_timeline_atom_v, name, uuid_before, false, true)
                .request(actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const utility::UuidUuidActor &result) mutable {
                        rp.deliver(result);
                        // clone data from target and inject into new actor
                        auto src_container = base_.containers().cfind_any(uuid);

                        if (src_container) {
                            mail(get_media_atom_v)
                                .request(container_[(*src_container)->value().uuid()], infinite)
                                .then(
                                    [=](const std::vector<UuidActor> &media) mutable {
                                        anon_mail(add_media_atom_v, media, Uuid())
                                            .send(result.second.actor());
                                        anon_mail(
                                            reflag_container_atom_v,
                                            (*src_container)->value().flag(),
                                            result.first)
                                            .send(actor_cast<caf::actor>(this));
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

            mail(create_subset_atom_v, name, uuid_before, false)
                .request(actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const utility::UuidUuidActor &result) mutable {
                        rp.deliver(result);
                        // clone data from target and inject into new actor
                        auto src_container = base_.containers().cfind_any(uuid);

                        if (src_container) {
                            mail(get_media_atom_v)
                                .request(container_[(*src_container)->value().uuid()], infinite)
                                .then(
                                    [=](const std::vector<UuidActor> &media) mutable {
                                        anon_mail(add_media_atom_v, media, Uuid())
                                            .send(result.second.actor());
                                        anon_mail(
                                            reflag_container_atom_v,
                                            (*src_container)->value().flag(),
                                            result.first)
                                            .send(actor_cast<caf::actor>(this));
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

        [=](copy_container_atom atom, caf::actor bookmarks) -> result<utility::CopyResult> {
            auto rp = make_response_promise<utility::CopyResult>();

            copy_container(std::vector<utility::Uuid>(), bookmarks, rp);
            return rp;
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
            anon_mail(playhead::playhead_rate_atom_v, base_.playhead_rate()).send(actor);
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
                mail(utility::event_atom_v, create_divider_atom_v, *i)
                    .send(base_.event_group());
                base_.send_changed();
                return *i;
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](create_group_atom,
            const std::string &name,
            const utility::Uuid &uuid_before) -> result<Uuid> {
            auto i = base_.insert_group(name, uuid_before);
            if (i) {
                mail(utility::event_atom_v, create_group_atom_v, *i).send(base_.event_group());
                base_.send_changed();
                return *i;
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](create_timeline_atom,
            const std::string &name,
            const utility::Uuid &uuid_before,
            const bool into,
            const bool with_tracks) -> result<utility::UuidUuidActor> {
            // try insert as requested, but add to end if it fails.
            auto rp    = make_response_promise<utility::UuidUuidActor>();
            auto actor = spawn<timeline::TimelineActor>(
                name,
                base_.media_rate(),
                utility::Uuid::generate(),
                actor_cast<caf::actor>(this),
                with_tracks);
            // anon_mail(playhead::playhead_rate_atom_v, base_.playhead_rate()).send(actor);
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
            anon_mail(playhead::playhead_rate_atom_v, base_.playhead_rate()).send(actor);
            create_container(actor, rp, uuid_before, into);
            return rp;
        },

        [=](json_store::get_json_atom atom, const std::string &path) {
            return mail(atom, path).delegate(json_store_);
        },

        [=](json_store::get_json_atom atom) { return mail(atom).delegate(json_store_); },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            return mail(atom, json, path).delegate(json_store_);
        },

        [=](duplicate_atom,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            duplicate(rp, src_bookmarks, dst_bookmarks);

            return rp;
        },

        // is this actually used ?
        [=](duplicate_atom,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks,
            const bool /*include_self*/) -> result<std::pair<UuidActor, UuidActor>> {
            auto rp = make_response_promise<std::pair<UuidActor, UuidActor>>();
            mail(duplicate_atom_v, src_bookmarks, dst_bookmarks)
                .request(actor_cast<caf::actor>(this), infinite)
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
        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &,
            const utility::UuidList &) {},
        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        [=](utility::event_atom,
            media::media_display_info_atom,
            const utility::JsonStore &,
            caf::actor_addr &) {},

        [=](utility::event_atom, media::media_display_info_atom, const utility::JsonStore &a) {
            mail(
                utility::event_atom_v,
                media::media_display_info_atom_v,
                a,
                caf::actor_cast<caf::actor_addr>(current_sender()))
                .send(base_.event_group());
        },

        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &,
            const media::MediaType) {
            base_.send_changed();
            mail(utility::event_atom_v, utility::change_atom_v).send(change_event_group_);
        },

        [=](get_change_event_group_atom) -> caf::actor { return change_event_group_; },

        [=](get_container_atom) -> PlaylistTree { return base_.containers(); },

        // unused..// [=](get_container_atom, const utility::Uuid &uuid) -> result<caf::actor>
        // {// if(container_.count(uuid))//         return container_[uuid];//  return
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

        [=](media::current_media_source_atom)
            -> caf::result<std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>> {
            auto rp = make_response_promise<
                std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>>();
            if (not media_.empty()) {
                fan_out_request<policy::select_all>(
                    map_value_to_vec(media_), infinite, media::current_media_source_atom_v)
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
                fan_out_request<policy::select_all>(
                    map_value_to_vec(media_), infinite, utility::detail_atom_v)
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

        [=](media::get_media_source_atom,
            const utility::Uuid &uuid,
            bool return_null) -> result<caf::actor> {
            auto rp = make_response_promise<caf::actor>();
            // collect media data..
            std::vector<caf::actor> actors;
            for (const auto &i : media_)
                actors.push_back(i.second);

            if (actors.empty()) {
                if (return_null) {
                    rp.deliver(caf::actor());
                } else {
                    rp.deliver(make_error(xstudio_error::error, "No matching media source"));
                }
                return rp;
            }

            fan_out_request<policy::select_all>(
                actors, infinite, media::get_media_source_atom_v, uuid, true)
                .then(
                    [=](const std::vector<caf::actor> matching_sources) mutable {
                        for (auto &a : matching_sources) {
                            if (a) {
                                rp.deliver(a);
                                return;
                            }
                        }
                        if (return_null) {
                            rp.deliver(caf::actor());
                        } else {
                            rp.deliver(
                                make_error(xstudio_error::error, "No matching media source"));
                        }
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](filter_media_atom, const std::string &filter_string) -> result<utility::UuidList> {
            // for each media item in the playlist, check if any fields in the
            // 'media display info' for the media matches filter_string. If
            // it does, include it in the result. We have to do some awkward
            // shenanegans thanks to async requests ... the usual stuff!!

            if (filter_string.empty())
                return base_.media();

            auto rp = make_response_promise<utility::UuidList>();
            // share ptr to store result for each media piece (in order)
            auto vv = std::make_shared<std::vector<Uuid>>(base_.media().size());
            // keep count of responses with this
            auto ct                        = std::make_shared<int>(base_.media().size());
            const auto filter_string_lower = utility::to_lower(filter_string);

            int idx = 0;
            for (const auto &uuid : base_.media()) {
                UuidActor media(uuid, media_.at(uuid));
                mail(media::media_display_info_atom_v)
                    .request(media.actor(), infinite)
                    .then(
                        [=](const utility::JsonStore &media_display_info) mutable {
                            if (media_display_info.is_array()) {
                                for (int i = 0; i < media_display_info.size(); ++i) {
                                    std::string data = media_display_info[i].dump();
                                    if (utility::to_lower(data).find(filter_string_lower) !=
                                        std::string::npos) {
                                        (*vv)[idx] = media.uuid();
                                        break;
                                    }
                                }
                            }
                            (*ct)--;
                            if (*ct == 0) {
                                utility::UuidList r;
                                for (const auto &v : *vv) {
                                    if (!v.is_null()) {
                                        r.push_back(v);
                                    }
                                }
                                rp.deliver(r);
                            }
                        },
                        [=](caf::error &err) mutable {
                            (*ct)--;
                            if (*ct == 0) {
                                utility::UuidList r;
                                for (const auto &v : *vv) {
                                    if (!v.is_null()) {
                                        r.push_back(v);
                                    }
                                }
                                rp.deliver(r);
                            }
                        });
                idx++;
            }
            return rp;
        },

        [=](get_next_media_atom,
            const utility::Uuid &after_this_uuid,
            int skip_by,
            const std::string &filter_string) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            mail(filter_media_atom_v, filter_string)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const utility::UuidList &media) mutable {
                        if (skip_by > 0) {
                            auto i = std::find(media.begin(), media.end(), after_this_uuid);
                            if (i == media.end()) {
                                // not found!
                                rp.deliver(make_error(
                                    xstudio_error::error,
                                    "playlist::get_next_media_atom called with uuid that is "
                                    "not in "
                                    "playlist"));
                            }
                            while (skip_by--) {
                                i++;
                                if (i == media.end()) {
                                    i--;
                                    break;
                                }
                            }
                            if (media_.count(*i))
                                rp.deliver(UuidActor(*i, media_.at(*i)));

                        } else {
                            auto i = std::find(media.rbegin(), media.rend(), after_this_uuid);
                            if (i == media.rend()) {
                                // not found!
                                rp.deliver(make_error(
                                    xstudio_error::error,
                                    "playlist::get_next_media_atom called with uuid that is "
                                    "not in "
                                    "playlist"));
                            }
                            while (skip_by++) {
                                i++;
                                if (i == media.rend()) {
                                    i--;
                                    break;
                                }
                            }

                            if (media_.count(*i))
                                rp.deliver(UuidActor(*i, media_.at(*i)));
                        }
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](insert_container_atom,
            const utility::CopyResult &cr,
            const utility::Uuid &before,
            const bool into) -> result<UuidVector> {
            auto rp = make_response_promise<UuidVector>();

            base_.insert_container(cr.tree_, before, into);

            // for(const auto &m : cr.media_)
            //     spdlog::warn("add new media {} {}", to_string(m.first), to_string(m.second));

            mail(playlist::add_media_atom_v, cr.media_, utility::Uuid())
                .request(actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const bool) mutable {
                        // now add containers actors and remap sources.
                        for (const auto &i : cr.containers_) {
                            container_[i.uuid()] = i.actor();
                            link_to(i.actor());
                            join_event_group(this, i.actor());
                            anon_mail(
                                playhead::source_atom_v,
                                actor_cast<caf::actor>(this),
                                cr.media_map_)
                                .send(i.actor());
                        }
                        notify_tree(cr.tree_);
                        base_.send_changed();
                        rp.deliver(cr.tree_.children_uuid(true));
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
            return rp;
        },

        [=](loading_media_atom atom, bool is_loading) {
            mail(utility::event_atom_v, atom, is_loading).send(base_.event_group());
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
                mail(utility::event_atom_v, move_container_atom_v, uuid, uuid_before, into)
                    .send(base_.event_group());
                base_.send_changed();
            }
            return result;
        },

        [=](playlist::move_media_atom atom, const Uuid &uuid, const Uuid &uuid_before) {
            return mail(atom, utility::UuidVector({uuid}), uuid_before)
                .delegate(actor_cast<caf::actor>(this));
        },

        [=](playlist::move_media_atom atom,
            const UuidList &media_uuids,
            const Uuid &uuid_before) {
            return mail(
                       atom,
                       utility::UuidVector(media_uuids.begin(), media_uuids.end()),
                       uuid_before)
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
                send_content_changed_event();
            }
            return result;
        },

        [=](playhead::playhead_rate_atom) -> FrameRate { return base_.playhead_rate(); },

        [=](playhead::playhead_rate_atom, const FrameRate &rate) {
            base_.set_playhead_rate(rate);
            base_.send_changed();
        },

        // create a new timeline, attach it to new playhead.
        [=](playlist::create_playhead_atom) -> result<utility::UuidActor> {
            if (playhead_)
                return playhead_;

            std::stringstream ss;
            ss << base_.name() << " Playhead";
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<playhead::PlayheadActor>(
                ss.str(),
                playhead::GLOBAL_AUDIO,
                selection_actor_,
                uuid,
                caf::actor_cast<caf::actor_addr>(this));

            anon_mail(playhead::playhead_rate_atom_v, base_.playhead_rate()).send(actor);
            playhead_ = UuidActor(uuid, actor);
            link_to(playhead_.actor());

            monitor(actor, [this, addr = actor.address()](const error &) {
                if (addr == playhead_.actor()) {
                    mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
                    base_.send_changed();
                }
            });

            base_.send_changed();

            // have we actually selected anything for initial view/playback?
            // If not, make the selection actor select something
            mail(playhead::get_selection_atom_v)
                .request(selection_actor_, infinite)
                .then(
                    [=](const UuidList &selection) {
                        if (!selection.size()) {
                            // by sending an empty list this will force the
                            // selection actor to select the first item in this playlist
                            anon_mail(playlist::select_media_atom_v, utility::UuidVector())
                                .send(selection_actor_);
                        }
                        if (!playhead_serialisation_.is_null()) {
                            anon_mail(module::deserialise_atom_v, playhead_serialisation_)
                                .send(playhead_.actor());
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
            return playhead_;
        },

        [=](playlist::get_playhead_atom) {
            return mail(playlist::create_playhead_atom_v)
                .delegate(caf::actor_cast<caf::actor>(this));
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
                mail(utility::event_atom_v, reflag_container_atom_v, uuid, flag)
                    .send(base_.event_group());
                base_.send_changed();
            }
            return result;
        },

        [=](remove_container_atom atom, const utility::Uuid &uuid) {
            return mail(atom, uuid, false).delegate(actor_cast<caf::actor>(this));
        },

        [=](remove_container_atom,
            const utility::UuidVector &cuuids,
            const bool remove_orphans) -> bool {
            bool something_removed = false;

            for (const auto &i : cuuids) {
                if (base_.cfind(i)) {
                    something_removed = true;
                    anon_mail(remove_container_atom_v, i, remove_orphans)
                        .send(actor_cast<caf::actor>(this));
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
                if (remove_orphans && not check_media.empty()) {
                    // for (const auto &i : check_media)
                    //     spdlog::info("candiate {}", to_string(i));
                    anon_mail(remove_orphans_atom_v, check_media)
                        .send(actor_cast<caf::actor>(this));
                }
                mail(utility::event_atom_v, remove_container_atom_v, cuuid)
                    .send(base_.event_group());
                base_.send_changed();
            }

            return result;
        },

        [=](media_hook::gather_media_sources_atom, const utility::UuidVector &uuids) {
            for (const auto &uuid : uuids) {
                if (media_.count(uuid)) {
                    anon_mail(
                        media_hook::gather_media_sources_atom_v,
                        media_.at(uuid),
                        base_.media_rate())
                        .send(caf::actor_cast<caf::actor>(session_));
                }
            }
        },

        [=](media_hook::gather_media_sources_atom, const utility::UuidList &uuids) {
            for (const auto &uuid : uuids) {
                if (media_.count(uuid)) {
                    anon_mail(
                        media_hook::gather_media_sources_atom_v,
                        media_.at(uuid),
                        base_.media_rate())
                        .send(caf::actor_cast<caf::actor>(session_));
                }
            }
        },

        [=](remove_media_atom, const utility::Uuid &uuid) -> caf::result<bool> {
            // this needs to propergate to children somehow..
            auto rp = make_response_promise<bool>();

            if (base_.remove_media(uuid)) {
                auto removed_media = media_.at(uuid);
                media_.erase(media_.find(uuid));
                unlink_from(removed_media);

                std::vector<caf::actor> actors;
                for (const auto &i : container_)
                    actors.push_back(i.second);

                if (actors.empty()) {
                    send_exit(removed_media, caf::exit_reason::user_shutdown);
                    rp.deliver(true);
                    send_content_changed_event();
                    base_.send_changed();
                } else {
                    fan_out_request<policy::select_all>(
                        actors, infinite, remove_media_atom_v, uuid)
                        .await(
                            [=](std::vector<bool>) mutable {
                                send_exit(removed_media, caf::exit_reason::user_shutdown);
                                rp.deliver(true);
                                send_content_changed_event();
                                base_.send_changed();
                            },
                            [=](error &) mutable {
                                send_exit(removed_media, caf::exit_reason::user_shutdown);
                                rp.deliver(true);
                                send_content_changed_event();
                                base_.send_changed();
                            });
                }
            } else {
                rp.deliver(false);
            }

            return rp;
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

            mail(media::decompose_atom_v)
                .request(media_.at(media_uuid), infinite)
                .then(
                    [=](const UuidActorVector &result) mutable {
                        if (not result.empty())
                            anon_mail(add_media_atom_v, result, base_.next_media(media_uuid))
                                .send(caf::actor_cast<caf::actor>(this));
                        rp.deliver(result);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](remove_media_atom atom, const utility::UuidList &uuids) {
            return mail(atom, utility::UuidVector(uuids.begin(), uuids.end()))
                .delegate(actor_cast<caf::actor>(this));
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
                base_.send_changed();
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
                if (uuids.empty()) {
                    auto media_uuids = map_key_to_vec(media_);
                    std::copy(
                        media_uuids.begin(),
                        media_uuids.end(),
                        std::inserter(candidates, candidates.end()));
                } else
                    std::copy(
                        uuids.begin(),
                        uuids.end(),
                        std::inserter(candidates, candidates.end()));

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
                                anon_mail(remove_media_atom_v, i)
                                    .send(actor_cast<caf::actor>(this));
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            } else {
                for (const auto &i : uuids)
                    anon_mail(remove_media_atom_v, i).send(actor_cast<caf::actor>(this));
            }
        },

        [=](rename_container_atom, const std::string &name, const utility::Uuid &uuid) -> bool {
            bool result = base_.rename_container(name, uuid);
            if (result) {
                mail(utility::event_atom_v, rename_container_atom_v, uuid, name)
                    .send(base_.event_group());
                base_.send_changed();
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
            base_.send_changed();
        },

        [=](sort_by_media_display_info_atom,
            const int sort_column_index,
            const bool ascending) { sort_by_media_display_info(sort_column_index, ascending); },

        [=](utility::event_atom, playlist::remove_media_atom, const UuidVector &) {},

        [=](utility::event_atom,
            playlist::add_media_atom,
            const utility::UuidActorVector &uav) {
            mail(utility::event_atom_v, add_media_atom_v, uav).send(base_.event_group());
        },

        [=](utility::event_atom, playlist::move_media_atom, const UuidVector &, const Uuid &) {
        },

        [=](utility::event_atom,
            media_reader::get_thumbnail_atom,
            const thumbnail::ThumbnailBufferPtr &buf) {},

        [=](utility::event_atom, utility::change_atom) {
            // one of our media changed..
            // this gets spammy...
            // buffer it ?
            // send_content_changed_event();
        },

        // watch for events...
        [=](json_store::update_atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            if (current_sender() == global_prefs_actor_) {
                try {
                    auto_gather_sources_ =
                        preference_value<bool>(full, "/core/media_reader/auto_gather_sources");
                } catch (...) {
                }
            } else if (current_sender() == json_store_)
                mail(json_store::update_atom_v, change, path, full).send(base_.event_group());
        },

        [=](json_store::update_atom, const JsonStore &full) mutable {
            if (current_sender() == global_prefs_actor_) {
                try {
                    auto_gather_sources_ =
                        preference_value<bool>(full, "/core/media_reader/auto_gather_sources");
                } catch (...) {
                }
            } else if (current_sender() == json_store_)
                mail(json_store::update_atom_v, full).send(base_.event_group());
        },

        [=](session::import_atom,
            const caf::uri &path,
            const Uuid &uuid_before,
            const bool wait) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            try {
                caf::scoped_actor sys(system());
                auto global = system().registry().template get<caf::actor>(global_registry);
                auto epa = request_receive<caf::actor>(*sys, global, global::get_python_atom_v);
                // spdlog::warn("Load from python");
                // request otio xml from embedded_python.
                mail(session::import_atom_v, path)
                    .request(epa, infinite)
                    .then(
                        [=](const std::string &data) mutable {
                            // got data create timeline and load it..
                            const auto name = fs::path(uri_to_posix_path(path)).stem().string();
                            // spdlog::warn("Loaded from python {}", name);
                            mail(create_timeline_atom_v, name, uuid_before, false, false)
                                .request(actor_cast<caf::actor>(this), infinite)
                                .then(
                                    [=](const utility::UuidUuidActor &uua) mutable {
                                        // request loading of timeline
                                        if (not wait) {
                                            anon_mail(session::import_atom_v, path, data)
                                                .send(uua.second.actor());
                                            rp.deliver(uua.second);
                                        } else {
                                            mail(session::import_atom_v, path, data)
                                                .request(uua.second.actor(), infinite)
                                                .then(
                                                    [=](const bool) mutable {
                                                        rp.deliver(uua.second);
                                                    },

                                                    [=](error &err) mutable {
                                                        spdlog::warn(
                                                            "{} {}",
                                                            __PRETTY_FUNCTION__,
                                                            to_string(err));
                                                        rp.deliver(std::move(err));
                                                    });
                                        }
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        rp.deliver(std::move(err));
                                    });
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, err.what()));
            }

            return rp;
        },

        [=](expanded_atom) -> bool { return base_.expanded(); },

        [=](expanded_atom, const bool expanded) { base_.set_expanded(expanded); },

        // handle child change events.
        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            mail(json_store::get_json_atom_v, "")
                .request(json_store_, infinite)
                .then(
                    [=](const JsonStore &meta) mutable {
                        std::vector<caf::actor> clients;
                        for (const auto &i : media_)
                            clients.push_back(i.second);
                        for (const auto &i : container_)
                            clients.push_back(i.second);
                        clients.push_back(selection_actor_);

                        if (not clients.empty()) {

                            fan_out_request<policy::select_all>(
                                clients, infinite, serialise_atom_v)
                                .then(
                                    [=](std::vector<JsonStore> json) mutable {
                                        JsonStore jsn;
                                        jsn["store"]  = meta;
                                        jsn["base"]   = base_.serialise();
                                        jsn["actors"] = {};
                                        for (const auto &j : json) {
                                            jsn["actors"][static_cast<std::string>(
                                                j["base"]["container"]["uuid"])] = j;
                                        }
                                        if (playhead_) {
                                            mail(utility::serialise_atom_v)
                                                .request(playhead_.actor(), infinite)
                                                .then(
                                                    [=](const utility::JsonStore
                                                            &playhead_state) mutable {
                                                        playhead_serialisation_ =
                                                            playhead_state;
                                                        jsn["playhead"] = playhead_state;
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
        }};
}


void PlaylistActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    // base_.event_group() =
    // system().groups().get_local(to_string(actor_cast<caf::actor>(this)));//system().groups().anonymous();
    change_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(change_event_group_);

    playlist_broadcast_ = spawn<broadcast::BroadcastActor>(this);

    if (!selection_actor_) {
        selection_actor_ = spawn<playhead::PlayheadSelectionActor>(
            base_.name() + " SelectionActor", caf::actor_cast<caf::actor>(this));
        link_to(selection_actor_);
    }

    join_broadcast(
        caf::actor_cast<caf::event_based_actor *>(selection_actor_), playlist_broadcast_);

    global_prefs_actor_ = caf::actor();

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        global_prefs_actor_ = prefs.get_jsonactor();
        join_broadcast(this, prefs.get_group(j));
        auto_gather_sources_ =
            preference_value<bool>(j, "/core/media_reader/auto_gather_sources");
    } catch (...) {
    }

    // side step problem with anon_send..
    // set_default_handler(skip);
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
    mail(detail_atom_v)
        .request(actor, infinite)
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
                    mail(
                        utility::event_atom_v,
                        create_subset_atom_v,
                        UuidActor(detail.uuid_, actor))
                        .send(base_.event_group());
                else if (detail.type_ == "ContactSheet")
                    mail(
                        utility::event_atom_v,
                        create_contact_sheet_atom_v,
                        UuidActor(detail.uuid_, actor))
                        .send(base_.event_group());
                else if (detail.type_ == "Timeline")
                    mail(
                        utility::event_atom_v,
                        create_timeline_atom_v,
                        UuidActor(detail.uuid_, actor))
                        .send(base_.event_group());

                base_.send_changed();
                rp.deliver(std::make_pair(*cuuid, UuidActor(detail.uuid_, actor)));
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void PlaylistActor::add_media(
    UuidActor &ua,
    const utility::Uuid &uuid_before,
    const bool delayed,
    caf::typed_response_promise<UuidActor> rp) {

    media_[ua.uuid()] = ua.actor();
    join_event_group(this, ua.actor());
    link_to(ua.actor());
    base_.insert_media(ua.uuid(), uuid_before);

    if (delayed and delayed_add_media_.empty()) {
        delayed_add_media_.push_back(ua);

        mail(playlist::add_media_atom_v, utility::event_atom_v)
            .delay(MEDIA_NOTIFY_DELAY_SLOW)
            .send(this);

    } else {
        delayed_add_media_.push_back(ua);
        mail(playlist::add_media_atom_v, utility::event_atom_v)
            .delay(MEDIA_NOTIFY_DELAY_FAST)
            .send(this);
    }

    mail(media::acquire_media_detail_atom_v, base_.playhead_rate())
        .request(ua.actor(), infinite)
        .then(
            [=](const bool) mutable {
                mail(media_hook::get_media_hook_atom_v)
                    .request(ua.actor(), infinite)
                    .then(
                        [=](bool) {},
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });

                // mail(utility::event_atom_v, add_media_atom_v,
                // UuidActorVector({ua})).send(base_.event_group());
                send_content_changed_event();
                base_.send_changed();
                rp.deliver(ua);
                if (is_in_viewer_)
                    open_media_reader(ua.actor());
            },
            [=](error &err) mutable {
                spdlog::warn(
                    "{} {} {}", __PRETTY_FUNCTION__, to_string(err), to_string(ua.actor()));
                send_content_changed_event();
                base_.send_changed();
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

    base_.send_changed();
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
        mail(utility::event_atom_v, playlist::create_divider_atom_v, tree.uuid())
            .send(base_.event_group());
    } else if (tree.value().type() == "ContainerGroup") {
        mail(utility::event_atom_v, playlist::create_group_atom_v, tree.uuid())
            .send(base_.event_group());
    } else if (
        tree.value().type() == "Subset" || tree.value().type() == "ContactSheet" ||
        tree.value().type() == "Timeline") {

        // auto uua = UuidUuidActor({tree.uuid(),{tree.value().uuid(),
        // container_[tree.value().uuid()]}});
        auto ua = UuidActor(tree.value().uuid(), container_[tree.value().uuid()]);

        if (tree.value().type() == "Subset") {
            mail(utility::event_atom_v, create_subset_atom_v, ua).send(base_.event_group());
        } else if (tree.value().type() == "ContactSheet") {
            mail(utility::event_atom_v, create_contact_sheet_atom_v, ua)
                .send(base_.event_group());
        } else if (tree.value().type() == "Timeline") {
            mail(utility::event_atom_v, create_timeline_atom_v, ua).send(base_.event_group());
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

            // need a list of source/dest media uuids.
            if (type == "Timeline")
                anon_mail(
                    playhead::source_atom_v, caf::actor_cast<caf::actor>(this), UuidUuidMap())
                    .send(result.actor());

            tree.value().set_uuid(result.uuid());
            container_[result.uuid()] = result.actor();
            link_to(result.actor());
            // update name
            auto name = request_receive<std::string>(*sys, result.actor(), name_atom_v);
            tree.value().set_name(name + " - copy");
            mail(name_atom_v, tree.value().name()).send(result.actor());
            // mail(utility::event_atom_v, add_playlist_atom_v,
            // result).send(base_.event_group());
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
// 	            anon_mail(// 	            	playlist::add_media_atom_v,
// 	                "New media",
// 	                i.first,
// 	                i.second
// 	            ).send(// 	            	actor_cast<caf::actor>(this));
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

    // #pragma message                                                                                \
//     "Disabling auto initialising of readers, it hits IO too hard when playlist is large. Needs a rethink."
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

    mail(media::get_media_pointer_atom_v, 0).request(media_actor, infinite).then(

            [=](const media::AVFrameID &media) mutable {

                    if (!media.is_nil()) {

                            anon_mail(media_reader::get_image_atom_v, media,
    true).send(global_reader); std::string path = uri_to_posix_path(media.uri_);

                    }
            },
            [=](error&) mutable {
            }
    );

    // force cache the first frame of every source in the playlist. This will open
    // the precache media reader for the media source ready for playback
    mail(media::get_media_pointer_atom_v, 0)
        .request(media_actor, infinite).then(

            [=](const media::AVFrameID &media) mutable {
                if (!media.is_nil()) {

                    anon_mail(media_reader::playback_precache_atom_v,
                        media,
                        utility::clock::now() +
                            std::chrono::seconds(
                                60), // saying we need this in 1 minute hence low priority
                        utility::Uuid()).send(global_reader);
                }
            },
            [=](error &) mutable {});*/
}

void PlaylistActor::send_content_changed_event(const bool queue) {
    if (not queue) {
        std::vector<UuidActor> actors;
        for (const auto &i : base_.media())
            actors.emplace_back(i, media_.at(i));

        mail(utility::event_atom_v, media_content_changed_atom_v, actors)
            .send(base_.event_group());
        mail(utility::event_atom_v, utility::change_atom_v).send(change_event_group_);
        content_changed_ = false;
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__,
        // to_string(caf::actor_cast<caf::actor>(this)));

    } else {
        if (not content_changed_) {
            content_changed_ = true;
            anon_mail(media_content_changed_atom_v)
                .delay(std::chrono::milliseconds(100))
                .send(this);
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

void PlaylistActor::sort_by_media_display_info(
    const int sort_column_index, const bool ascending) {

    using SourceAndUuid = std::pair<std::string, utility::Uuid>;
    auto sort_keys_vs_uuids =
        std::make_shared<std::vector<std::pair<std::string, utility::Uuid>>>();

    int idx = 0;
    for (const auto &i : media_) {

        // Pro tip: because i is a reference, it's the reference that is captured in our lambda
        // below and therefore it is 'unstable' so we make a copy here and use that in the
        // lambda as this is object-copied in the capture instead.
        UuidActor media_actor(i.first, i.second);
        idx++;
        mail(media::media_display_info_atom_v)
            .request(media_actor.actor(), infinite)
            .await(

                [=](const utility::JsonStore &media_display_info) mutable {
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

                    if (sort_keys_vs_uuids->size() == media_.size()) {

                        std::sort(
                            sort_keys_vs_uuids->begin(),
                            sort_keys_vs_uuids->end(),
                            [ascending](
                                const SourceAndUuid &a, const SourceAndUuid &b) -> bool {
                                return ascending ? a.first < b.first : a.first > b.first;
                            });

                        utility::UuidList ordered_uuids;
                        for (const auto &p : (*sort_keys_vs_uuids)) {
                            ordered_uuids.push_back(p.second);
                        }

                        anon_mail(move_media_atom_v, ordered_uuids, utility::Uuid())
                            .send(caf::actor_cast<caf::actor>(this));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}


void PlaylistActor::duplicate(
    caf::typed_response_promise<UuidActor> rp,
    caf::actor src_bookmarks,
    caf::actor dst_bookmarks) {
    // spdlog::stopwatch sw;

    auto uuid = utility::Uuid::generate();
    auto actor =
        spawn<PlaylistActor>(base_.name(), uuid, caf::actor_cast<caf::actor>(session_));

    anon_mail(session::media_rate_atom_v, base_.media_rate()).send(actor);
    anon_mail(playhead::playhead_rate_atom_v, base_.playhead_rate()).send(actor);

    {
        caf::scoped_actor sys(system());
        auto json = request_receive<JsonStore>(*sys, json_store_, json_store::get_json_atom_v);
        anon_mail(json_store::set_json_atom_v, json, "").send(actor);
    }

    // clone media into new playlist
    // store mapping from old to new uuids

    // spdlog::info("duplicate {:.3} seconds.", sw);

    if (not media_.empty()) {
        fan_out_request<policy::select_all>(
            map_value_to_vec(media_),
            infinite,
            utility::duplicate_atom_v,
            src_bookmarks,
            dst_bookmarks)
            .then(
                [=](const std::vector<UuidUuidActor> dmedia) mutable {
                    std::map<Uuid, UuidActor> dmedia_map;

                    // spdlog::info("cloned media {:.3} seconds.", sw);

                    for (const auto &i : dmedia)
                        dmedia_map[i.first] = i.second;

                    UuidUuidMap media_map;
                    UuidActorVector new_media;

                    for (const auto &i : base_.media()) {
                        media_map[i] = dmedia_map[i].uuid();
                        new_media.push_back(dmedia_map[i]);
                    }

                    {
                        caf::scoped_actor sys(system());
                        request_receive<bool>(*sys, actor, add_media_atom_v, new_media, Uuid());
                    }

                    // spdlog::info("added media {:.3} seconds.", sw);

                    // handle containers.
                    duplicate_containers(rp, UuidActor(uuid, actor), media_map);
                    // spdlog::info("duplicate_containers {:.3} seconds.", sw);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } else {
        duplicate_containers(rp, UuidActor(uuid, actor), UuidUuidMap());
    }
}

void PlaylistActor::duplicate_containers(
    caf::typed_response_promise<UuidActor> rp,
    const utility::UuidActor &new_playlist,
    const utility::UuidUuidMap &media_map) {

    try {
        caf::scoped_actor sys(system());

        // need to clone container tree..
        // if we process in order....
        for (const auto &i : base_.containers().uuids(true)) {
            auto ii = base_.containers().cfind(i);
            if (ii) {
                // create in new playlist..
                //  is actor ?
                if (container_.count((*ii)->value().uuid())) {
                    auto ua = request_receive<UuidActor>(
                        *sys, container_[(*ii)->value().uuid()], utility::duplicate_atom_v);

                    // add to new playlist
                    auto nua = request_receive<UuidUuidActor>(
                        *sys,
                        new_playlist.actor(),
                        create_container_atom_v,
                        ua.actor(),
                        Uuid(),
                        false);

                    // reassign media / reparent

                    request_receive<bool>(
                        *sys,
                        nua.second.actor(),
                        playhead::source_atom_v,
                        new_playlist.actor(),
                        media_map);

                    request_receive<bool>(
                        *sys,
                        new_playlist.actor(),
                        reflag_container_atom_v,
                        (*ii)->value().flag(),
                        nua.first);
                } else {
                    // add to new playlist
                    auto u = request_receive<utility::Uuid>(
                        *sys,
                        new_playlist.actor(),
                        create_divider_atom_v,
                        (*ii)->value().name(),
                        Uuid(),
                        false);
                    request_receive<bool>(
                        *sys,
                        new_playlist.actor(),
                        reflag_container_atom_v,
                        (*ii)->value().flag(),
                        u);
                }
            }
        }

        // have we really changed ?
        base_.send_changed();
        rp.deliver(new_playlist);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void PlaylistActor::recursive_add_media_with_subsets(
    caf::typed_response_promise<std::vector<utility::UuidActor>> rp,
    const caf::uri &path,
    const utility::Uuid &uuid_before) {

    // 'path' should be a directory. Media directly within the directory
    // will be added to this playlsit only.
    // Any subdirectories in path will create a named subset, and the media
    // within that subdirectory will be added to this subset

    auto result = std::make_shared<std::vector<UuidActor>>();

    // First add media items that are directly inside 'path'
    mail(
        add_media_atom_v,
        path,
        false, // not recursive
        uuid_before)
        .request(caf::actor_cast<caf::actor>(this), infinite)
        .then(
            [=](const std::vector<UuidActor> &media) mutable {
                result->insert(result->end(), media.begin(), media.end());

                // find child folders
                std::vector<caf::uri> subfolders = utility::uri_subfolders(path);
                if (!subfolders.size()) {
                    rp.deliver(*result);
                }

                auto result_counter = std::make_shared<int>(subfolders.size());

                // func to decrement result_counter and deliver on the RP when
                // we've finished
                auto check_deliver = [result_counter, result, rp]() mutable {
                    (*result_counter)--;
                    if (!(*result_counter)) {
                        rp.deliver(*result);
                    }
                };

                for (auto subfolder : subfolders) {

                    // now add the media from the subfolders
                    mail(
                        add_media_atom_v,
                        subfolder,
                        true, // recursive yes!
                        uuid_before)
                        .request(caf::actor_cast<caf::actor>(this), infinite)
                        .then(
                            [=](const std::vector<UuidActor> &media) mutable {
                                result->insert(result->end(), media.begin(), media.end());
                                if (media.size()) {

                                    // make the subset and add the media into that.
                                    fs::path p(uri_to_posix_path(subfolder));
                                    const auto subset_name = std::string(p.filename().string());
                                    mail(
                                        create_subset_atom_v,
                                        subset_name,
                                        utility::Uuid(),
                                        false)
                                        .request(caf::actor_cast<caf::actor>(this), infinite)
                                        .then(
                                            [=](utility::UuidUuidActor subset) mutable {
                                                mail(add_media_atom_v, media, utility::Uuid())
                                                    .request(subset.second.actor(), infinite)
                                                    .then(
                                                        [=](bool) mutable { check_deliver(); },
                                                        [=](caf::error &err) mutable {
                                                            spdlog::warn(
                                                                "{} {}",
                                                                __PRETTY_FUNCTION__,
                                                                to_string(err));
                                                            check_deliver();
                                                        });
                                            },
                                            [=](caf::error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                check_deliver();
                                            });
                                }
                            },
                            [=](caf::error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                check_deliver();
                            });
                }
            },
            [=](caf::error &err) mutable { rp.deliver(err); });

    mail(utility::event_atom_v, loading_media_atom_v, true).send(base_.event_group());
}
