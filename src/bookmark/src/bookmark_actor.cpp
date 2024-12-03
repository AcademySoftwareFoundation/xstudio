// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark_actor.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"


using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::bookmark;
using namespace nlohmann;
using namespace caf;

namespace {} // namespace


BookmarkActor::BookmarkActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"])) {

    if (not jsn.count("store") or jsn["store"].is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(),
            static_cast<JsonStore>(jsn["store"]),
            std::chrono::milliseconds(50));
    }
    link_to(json_store_);
    join_event_group(this, json_store_);
    init();

    if (jsn["base"].contains("annotation") and not jsn["base"]["annotation"].is_null()) {
        build_annotation_via_plugin(static_cast<utility::JsonStore>(jsn["base"]["annotation"]));
    }
}

BookmarkActor::BookmarkActor(
    caf::actor_config &cfg, const utility::Uuid &uuid, const Bookmark &base)
    : caf::event_based_actor(cfg), base_(base) {
    base_.set_uuid(uuid);

    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));

    link_to(json_store_);
    join_event_group(this, json_store_);

    init();

    if (base.annotation_) {
        utility::Uuid plugin_uuid;
        utility::JsonStore anno_data = base.annotation_->serialise(plugin_uuid);
        anno_data["plugin_uuid"]     = plugin_uuid;
        build_annotation_via_plugin(anno_data);
    }
}

caf::message_handler BookmarkActor::message_handler() {
    return caf::message_handler{
        make_ignore_error_handler(),

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](json_store::get_json_atom atom,
            const std::string &path,
            const bool /*fanout*/) -> caf::result<std::pair<UuidActor, JsonStore>> {
            auto rp = make_response_promise<std::pair<UuidActor, JsonStore>>();
            request(json_store_, infinite, atom, path)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(UuidActor(base_.uuid(), this), jsn));
                    },
                    [=](error &err) mutable {
                        rp.deliver(std::make_pair(UuidActor(base_.uuid(), this), JsonStore()));
                    });
            return rp;
        },


        // watch for events...
        [=](json_store::update_atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            if (current_sender() == json_store_) {
                send(base_.event_group(), json_store::update_atom_v, change, path, full);
            }
        },

        [=](json_store::update_atom, const JsonStore &full) mutable {
            if (current_sender() == json_store_) {
                send(base_.event_group(), json_store::update_atom_v, full);
            }
        },

        [=](json_store::get_json_atom atom, const std::string &path) {
            delegate(json_store_, atom, path);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json) {
            delegate(json_store_, atom, json);
        },

        [=](json_store::merge_json_atom atom, const JsonStore &json) {
            delegate(json_store_, atom, json);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            delegate(json_store_, atom, json, path);
        },


        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            request(json_store_, infinite, json_store::get_json_atom_v, "")
                .then(
                    [=](const JsonStore &meta) mutable {
                        JsonStore jsn;
                        jsn["store"] = meta;
                        jsn["base"]  = base_.serialise();
                        rp.deliver(jsn);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](associate_bookmark_atom) -> UuidUuidActor {
            return UuidUuidActor(
                base_.uuid(), UuidActor(base_.owner(), caf::actor_cast<caf::actor>(owner_)));
        },

        [=](associate_bookmark_atom, bool) -> result<bool> {
            // this 'reassociates' a bookmark with it's source media actor that
            // it is attached (associated) with.
            auto rp = make_response_promise<bool>();
            request(
                system().registry().template get<caf::actor>(studio_registry),
                infinite,
                session::session_atom_v)
                .then(
                    [=](caf::actor session) mutable {
                        request(session, infinite, playlist::get_media_atom_v, base_.owner())
                            .then(
                                [=](caf::actor media) mutable {
                                    utility::UuidActor ua(base_.owner(), media);
                                    request(
                                        caf::actor_cast<caf::actor>(this),
                                        infinite,
                                        associate_bookmark_atom_v,
                                        ua)
                                        .then(
                                            [=](bool r) mutable { rp.deliver(r); },
                                            [=](caf::error &err) mutable { rp.deliver(err); });
                                },
                                [=](caf::error &err) mutable { rp.deliver(err); });
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](bookmark_detail_atom) -> result<BookmarkDetail> {
            auto detail        = BookmarkDetail(base_);
            detail.actor_addr_ = caf::actor_cast<caf::actor_addr>(this);
            if (not owner_)
                return detail;

            auto rp = make_response_promise<BookmarkDetail>();

            request(
                caf::actor_cast<caf::actor>(owner_),
                infinite,
                playlist::reflag_container_atom_v)
                .then(
                    [=](const std::tuple<std::string, std::string> &flag) mutable {
                        detail.media_flag_ = std::get<1>(flag);
                        request(
                            caf::actor_cast<caf::actor>(owner_),
                            infinite,
                            media::media_reference_atom_v,
                            utility::Uuid())
                            .then(
                                [=](const std::pair<Uuid, MediaReference> result) mutable {
                                    (*(detail.owner_)).actor() =
                                        caf::actor_cast<caf::actor>(owner_);
                                    detail.logical_start_frame_ =
                                        result.second.duration().frame(max(
                                            timebase::k_flicks_zero_seconds, base_.start()));
                                    detail.media_reference_ = result.second;
                                    rp.deliver(detail);
                                },
                                [=](caf::error &err) mutable { rp.deliver(err); });
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](bookmark_detail_atom, const BookmarkDetail &detail) -> bool {
            auto changed = base_.update(detail);
            if (detail.owner_) {
                base_.set_owner((*detail.owner_).uuid());
                anon_send(
                    caf::actor_cast<caf::actor>(this),
                    associate_bookmark_atom_v,
                    *detail.owner_);
            }

            if (changed) {
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    bookmark_change_atom_v,
                    base_.uuid());
                base_.send_changed();
            }
            return changed;
        },
        // reconnect to owner actor.
        [=](associate_bookmark_atom, const UuidActor &src) -> bool {
            if (src.uuid() == base_.owner()) {
                if (caf::actor_cast<caf::actor>(owner_) != src.actor()) {
                    set_owner(src.actor());
                }
                return true;
            }
            return false;
        },

        // reconnect to owner actor.
        [=](associate_bookmark_atom, const UuidActorVector &src) -> bool {
            for (const auto &i : src) {
                if (i.uuid() == base_.owner()) {
                    if (caf::actor_cast<caf::actor>(owner_) != i.actor()) {
                        set_owner(i.actor());
                    }
                    return true;
                }
            }
            return false;
        },

        [=](add_annotation_atom, AnnotationBasePtr anno) -> bool {
            if (base_.annotation_ == anno)
                return false;
            base_.annotation_ = anno;
            send(
                base_.event_group(),
                utility::event_atom_v,
                bookmark_change_atom_v,
                base_.uuid());
            // base_.send_changed();
            return true;
        },

        [=](add_annotation_atom, const utility::JsonStore &anno_data) -> bool {
            build_annotation_via_plugin(anno_data);
            return true;
        },

        [=](get_annotation_atom) -> AnnotationBasePtr {
            if (!base_.annotation_) {
                // if there is no annotation on this note, we return a temporary empty
                // annotation base that just does the job of carrying the bookmark uuid through
                // to the annotation tools. We need this because when the user starts drawing on
                // screen on a frame where a note (bookmark) exists but there is as-yet no
                // annotation we want to add the new drawings to the note. The annotation tool
                // needs to know the uuid of the on-screen note to do this.
                return std::make_shared<bookmark::AnnotationBase>(
                    utility::JsonStore(), base_.uuid());
            }
            return base_.annotation_;
        },

        [=](bookmark_detail_atom, get_annotation_atom) -> result<BookmarkAndAnnotationPtr> {
            auto rp = make_response_promise<BookmarkAndAnnotationPtr>();
            request(caf::actor_cast<caf::actor>(this), infinite, bookmark_detail_atom_v)
                .then(
                    [=](const BookmarkDetail &detail) mutable {
                        auto data         = new BookmarkAndAnnotation;
                        data->detail_     = detail;
                        data->annotation_ = base_.annotation_;
                        BookmarkAndAnnotationPtr ptr(data);
                        rp.deliver(ptr);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](utility::duplicate_atom) -> result<utility::UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            request(json_store_, infinite, json_store::get_json_atom_v)
                .then(
                    [=](const JsonStore &meta) mutable {
                        auto uuid  = utility::Uuid::generate();
                        auto actor = spawn<BookmarkActor>(uuid, base_);

                        request(actor, infinite, json_store::set_json_atom_v, meta)
                            .then(
                                [=](bool) mutable { rp.deliver(UuidActor(uuid, actor)); },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](media::media_reference_atom atom) -> caf::result<utility::MediaReference> {
            if (not owner_)
                return make_error(xstudio_error::error, "Bookmark unassociated.");

            auto rp = make_response_promise<utility::MediaReference>();
            request(caf::actor_cast<caf::actor>(owner_), infinite, atom, utility::Uuid())
                .then(
                    [=](const std::pair<Uuid, MediaReference> result) mutable {
                        rp.deliver(result.second);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media::get_media_pointer_atom) -> caf::result<media::AVFrameID> {
            if (not owner_)
                return make_error(xstudio_error::error, "Bookmark unassociated.");

            auto rp = make_response_promise<media::AVFrameID>();

            // need media ref
            request(caf::actor_cast<caf::actor>(this), infinite, media::media_reference_atom_v)
                .then(
                    [=](const utility::MediaReference &mediaref) mutable {
                        // also need bookmark detail
                        // for bookmark start.
                        // cache ?
                        auto lframe = mediaref.duration().frame(base_.start());
                        rp.delegate(
                            caf::actor_cast<caf::actor>(owner_),
                            media::get_media_pointer_atom_v,
                            media::MediaType::MT_IMAGE,
                            lframe);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        }};
}

void BookmarkActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        if (msg.source == caf::actor_cast<caf::actor>(owner_)) {
            set_owner(caf::actor(), true);
            send_exit(this, caf::exit_reason::user_shutdown);
        }
    });

    attach_functor([=](const caf::error &reason) {
        // this sends a dummy change event that will propagate from the owner, to playhead
        // so that playhead knows bookmark frame ranges have changed, for example
        auto owner = caf::actor_cast<caf::actor>(owner_);
        if (owner)
            send(owner, utility::event_atom_v, remove_bookmark_atom_v, base_.uuid());
    });
}

void BookmarkActor::build_annotation_via_plugin(const utility::JsonStore &anno_data) {

    if (anno_data.contains("plugin_uuid")) {
        auto plugin_uuid = anno_data["plugin_uuid"].get<utility::Uuid>();
        if (plugin_uuid.is_null())
            return;
        auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);

        request(
            pm,
            infinite,
            plugin_manager::spawn_plugin_atom_v,
            plugin_uuid,
            utility::JsonStore())
            .then(
                [=](caf::actor annotations_plugin) {
                    request(annotations_plugin, infinite, build_annotation_atom_v, anno_data)
                        .then(
                            [=](AnnotationBasePtr &anno) {
                                anno->bookmark_uuid_ = base_.uuid();
                                base_.annotation_    = anno;
                                send(
                                    base_.event_group(),
                                    utility::event_atom_v,
                                    bookmark_change_atom_v,
                                    base_.uuid());
                            },
                            [=](caf::error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                // use base annotation class - this does nothing but stores and
                                // will later serialise annotation data, so annotations aren't
                                // erased from the session even though we've failed to load
                                // the desired plugin.
                                base_.annotation_.reset(
                                    new AnnotationBase(anno_data, base_.uuid()));
                                send(
                                    base_.event_group(),
                                    utility::event_atom_v,
                                    bookmark_change_atom_v,
                                    base_.uuid());
                            });
                },
                [=](caf::error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    // see comment above.
                    base_.annotation_.reset(new AnnotationBase(anno_data, base_.uuid()));
                    send(
                        base_.event_group(),
                        utility::event_atom_v,
                        bookmark_change_atom_v,
                        base_.uuid());
                });

    } else {
        if (not anno_data.is_null())
            spdlog::warn(
                "{} AnnotationBase serialised data does not include annotations plugin uuid",
                __PRETTY_FUNCTION__);
        base_.annotation_.reset(new AnnotationBase(anno_data, base_.uuid()));
        send(base_.event_group(), utility::event_atom_v, bookmark_change_atom_v, base_.uuid());
    }
}

void BookmarkActor::set_owner(caf::actor owner, const bool dead) {

    // Note, owner is a MediaActor
    if (owner_) {
        demonitor(caf::actor_cast<caf::actor>(owner_));
        if (not dead) {
            auto src = caf::actor_cast<caf::event_based_actor *>(owner_);
            if (src)
                utility::leave_event_group(src, caf::actor_cast<caf::actor>(this));
        }
    }

    owner_ = caf::actor_cast<caf::actor_addr>(owner);

    if (owner) {
        monitor(owner);
        request(base_.event_group(), infinite, broadcast::join_broadcast_atom_v, owner)
            .then(
                [=](const bool) mutable {},
                [=](const error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }

    send(base_.event_group(), utility::event_atom_v, bookmark_change_atom_v, base_.uuid());
}
