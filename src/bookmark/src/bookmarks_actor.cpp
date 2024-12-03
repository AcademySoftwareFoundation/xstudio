// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/bookmark/bookmarks_actor.hpp"
#include "xstudio/bookmark/bookmark_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/csv.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::bookmark;
using namespace nlohmann;
using namespace caf;

namespace {} // namespace


BookmarksActor::BookmarksActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"])) {

    for (const auto &[key, value] : jsn["actors"].items()) {
        try {
            auto uuid        = utility::Uuid(key);
            auto actor       = spawn<BookmarkActor>(static_cast<JsonStore>(value));
            bookmarks_[uuid] = actor;
            monitor(actor);
            join_event_group(this, actor);
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    init();
}

BookmarksActor::BookmarksActor(caf::actor_config &cfg, const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_() {
    base_.set_uuid(uuid);
    init();
}

caf::message_handler BookmarksActor::default_event_handler() {
    return {
        [=](utility::event_atom, remove_bookmark_atom, const utility::Uuid &) {},
        [=](utility::event_atom, add_bookmark_atom, const utility::UuidActor &) {},
        [=](utility::event_atom, bookmark_change_atom, const utility::UuidActor &) {}};
}

void BookmarksActor::on_exit() {
    for (auto &it : bookmarks_) {
        send_exit(it.second, caf::exit_reason::user_shutdown);
    }
}

caf::message_handler BookmarksActor::message_handler() {
    return caf::message_handler{
        make_ignore_error_handler(),

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // json events from bookmarks
        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {},

        // json events from bookmarks
        [=](json_store::update_atom, const JsonStore &js) {},

        [=](json_store::get_json_atom atom,
            const utility::UuidVector &uuids,
            const std::string &path)
            -> caf::result<std::vector<std::pair<UuidActor, JsonStore>>> {
            auto bookmarks = std::vector<caf::actor>();
            for (const auto &i : uuids) {
                if (bookmarks_.count(i))
                    bookmarks.push_back(bookmarks_[i]);
                else
                    return make_error(xstudio_error::error, "Invalid uuid");
            }

            auto rp = make_response_promise<std::vector<std::pair<UuidActor, JsonStore>>>();
            if (not bookmarks.empty()) {
                fan_out_request<policy::select_all>(bookmarks, infinite, atom, path, true)
                    .then(
                        [=](std::vector<std::pair<UuidActor, JsonStore>> vmeta) mutable {
                            rp.deliver(vmeta);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.deliver(std::vector<std::pair<UuidActor, JsonStore>>());
            }

            return rp;
        },

        [=](json_store::get_json_atom atom,
            const utility::Uuid &uuid,
            const std::string &path) -> caf::result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            if (bookmarks_.count(uuid))
                rp.delegate(bookmarks_[uuid], atom, path);
            else
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
            return rp;
        },

        [=](json_store::set_json_atom atom,
            const utility::Uuid &uuid,
            const JsonStore &json,
            const std::string &path) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();
            if (bookmarks_.count(uuid))
                rp.delegate(bookmarks_[uuid], atom, json, path);
            else
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
            return rp;
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto clients = std::vector<caf::actor>();

            // check for dead bookmarks
            {
                auto i = bookmarks_.begin();
                while (i != bookmarks_.end()) {
                    if (not i->second) {
                        i = bookmarks_.erase(i);
                    } else {
                        clients.push_back(i->second);
                        i++;
                    }
                }
            }

            // Big NOPE, you can't erase with an iterator in a loop like this
            /*for (auto i = bookmarks_) {
                if (not i.second)
                    bookmarks_.erase(i.first);
                else
                    clients.push_back(i.second);
            }*/

            if (not clients.empty()) {
                fan_out_request<policy::select_all>(clients, infinite, serialise_atom_v)
                    .then(
                        [=](std::vector<JsonStore> json) mutable {
                            JsonStore jsn;
                            jsn["base"]   = base_.serialise();
                            jsn["actors"] = {};
                            for (const auto &j : json) {
                                if (not j.is_null())
                                    jsn["actors"][static_cast<std::string>(
                                        j["base"]["container"]["uuid"])] = j;
                            }

                            rp.deliver(jsn);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                JsonStore jsn;
                jsn["base"]   = base_.serialise();
                jsn["actors"] = {};
                rp.deliver(jsn);
            }
            return rp;
        },

        [=](utility::serialise_atom, const utility::Uuid uuid) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            if (bookmarks_.count(uuid))
                rp.delegate(bookmarks_[uuid], utility::serialise_atom_v);
            else
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
            return rp;
        },

        // get bookmarkr associated with  owner uuids
        [=](get_bookmarks_atom, const utility::UuidVector &owner_uuids)
            -> result<std::vector<std::pair<Uuid, UuidActorVector>>> {
            if (bookmarks_.empty())
                return std::vector<std::pair<Uuid, UuidActorVector>>();

            auto rp = make_response_promise<std::vector<std::pair<Uuid, UuidActorVector>>>();
            // gather owners for all bookmarks.
            fan_out_request<policy::select_all>(
                map_value_to_vec(bookmarks_), infinite, associate_bookmark_atom_v)
                .then(
                    [=](const std::vector<UuidUuidActor> uav) mutable {
                        std::vector<std::pair<Uuid, UuidActorVector>> result;
                        std::map<Uuid, UuidActorVector> result_map;
                        std::set<Uuid> look_for;

                        // build look up for owners we are interested in.
                        for (const auto &i : owner_uuids)
                            look_for.insert(i);

                        for (const auto &i : uav) {
                            // are we looking for the owner
                            if (look_for.count(i.second.uuid())) {
                                if (not result_map.count(i.second.uuid()))
                                    result_map[i.second.uuid()] = UuidActorVector();
                                // add bookmark to owner map.
                                result_map[i.second.uuid()].emplace_back(
                                    UuidActor(i.first, bookmarks_[i.first]));
                            }
                        }
                        for (const auto &i : result_map)
                            result.emplace_back(std::make_pair(i.first, i.second));

                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        // get bookmakrs associated with uuid
        [=](get_bookmarks_atom, const utility::Uuid &owner_uuid) -> result<UuidActorVector> {
            if (bookmarks_.empty())
                return UuidActorVector();

            auto rp = make_response_promise<UuidActorVector>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(bookmarks_), infinite, associate_bookmark_atom_v)
                .then(
                    [=](const std::vector<UuidUuidActor> uav) mutable {
                        UuidActorVector result;
                        for (const auto &i : uav) {
                            if (i.second.uuid() == owner_uuid)
                                result.emplace_back(UuidActor(i.first, bookmarks_[i.first]));
                        }

                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](get_bookmarks_atom) -> result<utility::UuidActorVector> {
            UuidActorVector result;
            for (const auto &i : bookmarks_)
                result.emplace_back(UuidActor(i.first, i.second));
            return result;
        },

        [=](get_bookmark_atom, const utility::Uuid &uuid) -> result<UuidActor> {
            if (bookmarks_.count(uuid))
                return UuidActor(uuid, bookmarks_[uuid]);
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](get_bookmark_atom,
            const utility::UuidList &uuids) -> result<std::vector<UuidActor>> {
            std::vector<UuidActor> r;
            for (const auto &uuid : uuids) {
                if (bookmarks_.count(uuid))
                    r.emplace_back(UuidActor(uuid, bookmarks_[uuid]));
            }
            return r;
        },


        // duplicate and assign
        [=](utility::duplicate_atom,
            const utility::Uuid &uuid,
            const UuidActor &src) -> result<utility::UuidActor> {
            if (bookmarks_.count(uuid)) {
                // clone it..
                auto rp = make_response_promise<utility::UuidActor>();
                request(bookmarks_[uuid], infinite, utility::duplicate_atom_v)
                    .then(
                        [=](const utility::UuidActor &ua) mutable {
                            // assign to src.
                            BookmarkDetail bd;
                            bd.owner_ = src;
                            anon_send(ua.actor(), bookmark_detail_atom_v, bd);
                            rp.deliver(ua);
                        },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
                return rp;
            }
            return make_error(xstudio_error::error, "Invalid uuid");
        },
        // create and assign.
        [=](add_bookmark_atom, const UuidActorVector &bookmarks) -> bool {
            for (const auto &i : bookmarks) {
                bookmarks_[i.uuid()] = i.actor();
                monitor(i.actor());
                join_event_group(this, i.actor());
                send(base_.event_group(), utility::event_atom_v, add_bookmark_atom_v, i);
            }
            base_.send_changed();
            return true;
        },

        // create and assign.
        [=](add_bookmark_atom, const UuidActor &src) -> result<utility::UuidActor> {
            auto rp = make_response_promise<utility::UuidActor>();
            request(caf::actor_cast<caf::actor>(this), infinite, add_bookmark_atom_v)
                .then(
                    [=](const utility::UuidActor &ua) mutable {
                        // associate..
                        BookmarkDetail bd;
                        bd.owner_ = src;
                        anon_send(ua.actor(), bookmark_detail_atom_v, bd);
                        rp.deliver(ua);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        // change to bookmark
        [=](utility::event_atom, bookmark_change_atom, const utility::Uuid &uuid) {
            send(
                base_.event_group(),
                utility::event_atom_v,
                bookmark_change_atom_v,
                UuidActor(uuid, bookmarks_[uuid]));
        },

        [=](add_bookmark_atom) -> utility::UuidActor {
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<BookmarkActor>(uuid);

            bookmarks_[uuid] = actor;
            monitor(actor);
            join_event_group(this, actor);

            base_.send_changed();
            send(
                base_.event_group(),
                utility::event_atom_v,
                add_bookmark_atom_v,
                UuidActor(uuid, actor));

            return UuidActor(uuid, actor);
        },

        [=](add_bookmark_atom,
            const utility::Uuid &bookmark_uuid,
            utility::JsonStore &bookmark_serialised_data) -> result<utility::UuidActor> {
            auto rp = make_response_promise<utility::UuidActor>();

            if (bookmarks_.count(bookmark_uuid)) {
                // bookmark already exists, behaviour is we just update it with the serialized
                // data.
                if (bookmark_serialised_data.contains("base") &&
                    bookmark_serialised_data["base"].contains("annotation")) {
                    request(
                        bookmarks_[bookmark_uuid],
                        infinite,
                        bookmark::add_annotation_atom_v,
                        utility::JsonStore(bookmark_serialised_data["base"]["annotation"]))
                        .then(
                            [=](bool) mutable {
                                utility::UuidActor ua(bookmark_uuid, bookmarks_[bookmark_uuid]);
                                rp.deliver(ua);
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                } else {
                    rp.deliver(make_error(
                        xstudio_error::error,
                        "No annotation data in bookmark serialisation data."));
                }
            } else {

                auto actor                = spawn<BookmarkActor>(bookmark_serialised_data);
                bookmarks_[bookmark_uuid] = actor;
                monitor(actor);
                join_event_group(this, actor);
                base_.send_changed();
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    add_bookmark_atom_v,
                    UuidActor(bookmark_uuid, actor));
                anon_send(actor, associate_bookmark_atom_v, true);
                rp.deliver(UuidActor(bookmark_uuid, actor));
            }
            return rp;
        },
        [=](bookmark_detail_atom,
            const utility::UuidSet associated_uuids) -> result<std::vector<BookmarkDetail>> {
            if (bookmarks_.empty())
                return std::vector<BookmarkDetail>();

            auto rp = make_response_promise<std::vector<BookmarkDetail>>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(bookmarks_), infinite, bookmark_detail_atom_v)
                .then(
                    [=](const std::vector<BookmarkDetail> details) mutable {
                        std::vector<BookmarkDetail> results;
                        for (const auto &i : details) {

                            if (associated_uuids.empty() or
                                associated_uuids.count((*(i.owner_)).uuid())) {
                                results.push_back(i);
                            }
                        }

                        rp.deliver(results);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](bookmark_detail_atom, const std::vector<utility::Uuid> associated_uuids)
            -> result<std::vector<BookmarkDetail>> {
            if (bookmarks_.empty())
                return std::vector<BookmarkDetail>();

            auto rp = make_response_promise<std::vector<BookmarkDetail>>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(bookmarks_), infinite, bookmark_detail_atom_v)
                .then(
                    [=](const std::vector<BookmarkDetail> details) mutable {
                        std::set<utility::Uuid> lookup;
                        for (const auto &i : associated_uuids)
                            lookup.insert(i);

                        std::vector<BookmarkDetail> results;

                        for (const auto &i : details) {

                            if (lookup.empty() or lookup.count((*(i.owner_)).uuid()))
                                results.push_back(i);
                        }

                        rp.deliver(results);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](bookmark_detail_atom atom, const utility::Uuid &uuid) -> result<BookmarkDetail> {
            auto rp = make_response_promise<BookmarkDetail>();
            if (not bookmarks_.count(uuid))
                return make_error(xstudio_error::error, "Invalid bookmark uuid");
            rp.delegate(bookmarks_[uuid], atom);
            return rp;
        },

        [=](bookmark_detail_atom atom,
            const utility::Uuid &uuid,
            const BookmarkDetail &detail) -> result<bool> {
            if (not bookmarks_.count(uuid))
                return make_error(xstudio_error::error, "Invalid bookmark uuid");

            auto rp = make_response_promise<bool>();
            rp.delegate(bookmarks_[uuid], atom, detail);
            return rp;
        },

        [=](remove_bookmark_atom, const utility::Uuid &uuid) -> bool {
            if (not bookmarks_.count(uuid))
                return false;

            // will be removed by down handler.
            send_exit(bookmarks_[uuid], caf::exit_reason::user_shutdown);
            return true;
        },

        [=](media::get_media_pointer_atom atom,
            const utility::Uuid &uuid) -> caf::result<media::AVFrameID> {
            if (not bookmarks_.count(uuid))
                return make_error(xstudio_error::error, "Invalid bookmark uuid");
            auto rp = make_response_promise<media::AVFrameID>();
            rp.delegate(bookmarks_[uuid], atom, uuid);
            return rp;
        },

        [=](media::media_reference_atom atom,
            const Uuid &uuid) -> caf::result<utility::MediaReference> {
            if (not bookmarks_.count(uuid))
                return make_error(xstudio_error::error, "Invalid bookmark uuid");
            auto rp = make_response_promise<utility::MediaReference>();
            rp.delegate(bookmarks_[uuid], atom, uuid);
            return rp;
        },

        [=](associate_bookmark_atom, const UuidActor &src) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (not bookmarks_.empty()) {
                fan_out_request<policy::select_all>(
                    map_value_to_vec(bookmarks_), infinite, associate_bookmark_atom_v, src)
                    .then(
                        [=](std::vector<bool>) mutable { rp.deliver(true); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.deliver(true);
            }
            return rp;
        },

        [=](associate_bookmark_atom atom, const UuidActorVector &src) -> result<int> {
            auto rp = make_response_promise<int>();
            if (not bookmarks_.empty()) {
                fan_out_request<policy::select_all>(
                    map_value_to_vec(bookmarks_), infinite, atom, src)
                    .then(
                        [=](std::vector<bool> result) mutable {
                            int count = 0;
                            for (const auto i : result) {
                                if (i)
                                    count++;
                            }
                            rp.deliver(count);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.deliver(0);
            }
            return rp;
        },

        [=](session::export_atom, const session::ExportFormat ef)
            -> result<std::pair<std::string, std::vector<std::byte>>> {
            if (ef != session::ExportFormat::EF_CSV)
                return make_error(xstudio_error::error, "Unsupported export format.");
            auto rp = make_response_promise<std::pair<std::string, std::vector<std::byte>>>();
            csv_export(rp);
            return rp;
        },

        [=](default_category_atom, const std::string category) {
            default_category_ = category;
        },

        [=](default_category_atom) -> std::string { return default_category_; },

        [=](media_reader::get_thumbnail_atom,
            const BookmarkDetail detail,
            int width,
            caf::actor receiver) {
            auto offscreen_renderer =
                system().registry().template get<caf::actor>(offscreen_viewport_registry);
            if (!offscreen_renderer) {
                spdlog::warn("{} : Offscreen viewport not found.", __PRETTY_FUNCTION__);
                return;
            }
            if (!detail.owner_ || !detail.start_ | !detail.owner_->actor())
                return;

            request(
                offscreen_renderer,
                infinite,
                ui::viewport::render_viewport_to_image_atom_v,
                detail.owner_->actor(),
                *(detail.start_),
                thumbnail::THUMBNAIL_FORMAT::TF_RGB24,
                width,
                false, // autoscale (renders at source imate fomat if true)
                true /*show annotations*/)
                .then(
                    [=](const thumbnail::ThumbnailBufferPtr &thumbnail) {
                        if (thumbnail)
                            anon_send(
                                receiver,
                                media_reader::get_thumbnail_atom_v,
                                detail,
                                thumbnail);
                        else
                            spdlog::warn(
                                "{} {}",
                                __PRETTY_FUNCTION__,
                                "Null thumbanil returned by offscreen renderer.");
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }};
}


void BookmarksActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        for (auto it = std::begin(bookmarks_); it != std::end(bookmarks_); ++it) {
            if (msg.source == it->second) {
                demonitor(it->second);
                // spdlog::warn("bookmark exited {}", to_string(it->first));
                base_.send_changed();
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    remove_bookmark_atom_v,
                    it->first);
                bookmarks_.erase(it);
                break;
            }
        }
    });
}

void BookmarksActor::csv_export(
    caf::typed_response_promise<std::pair<std::string, std::vector<std::byte>>> rp) {
    // collect all bookmark details..
    request(actor_cast<caf::actor>(this), infinite, bookmark_detail_atom_v, UuidVector())
        .then(
            [=](std::vector<BookmarkDetail> details) mutable {
                std::vector<std::vector<std::string>> data;
                data.emplace_back(std::vector<std::string>(
                    {"Subject",
                     "Notes",
                     "Start Frame",
                     "End Frame",
                     "Created",
                     "User Name",
                     "Note Type",
                     "Media Flag"}));
                sort(details.begin(), details.end(), [](const auto &lhs, const auto &rhs) {
                    return (lhs.subject_ ? *(lhs.subject_) : "") <
                           (rhs.subject_ ? *(rhs.subject_) : "");
                });
                for (const auto &i : details) {
                    data.emplace_back(std::vector<std::string>(
                        {i.subject_ ? *(i.subject_) : "",
                         i.note_ ? *(i.note_) : "",
                         i.start_timecode(),
                         i.end_timecode(),
                         i.created(),
                         i.author_ ? *(i.author_) : "",
                         i.category_ ? *(i.category_) : "",
                         i.media_flag_ ? *(i.media_flag_) : ""}));
                }

                rp.deliver(std::make_pair(utility::to_csv(data), std::vector<std::byte>()));
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });
}
