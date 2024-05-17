// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <caf/policy/select_all.hpp>
#include <tuple>

#include <zstr.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmarks_actor.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/bookmark/bookmarks_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/tag/tag_actor.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/tag/tag_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::session;
using namespace nlohmann;
using namespace caf;
namespace fs = std::filesystem;

namespace {

// offload media actor copy as it blocks the session actor..
class MediaCopyActor : public caf::event_based_actor {

  public:
    MediaCopyActor(
        caf::actor_config &cfg,
        caf::actor session,
        caf::actor bookmarks,
        std::map<utility::Uuid, caf::actor> playlists);

    ~MediaCopyActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }
    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "MediaCopyActor";
    caf::behavior behavior_;
    caf::actor session_;
    caf::actor bookmarks_;

    std::vector<caf::uri> uris_;

    void copy_media_to(
        caf::typed_response_promise<utility::UuidVector> &rp,
        const utility::Uuid &dst,
        const utility::Uuid &src,
        const utility::UuidVector &media,
        const bool remove_source         = false,
        const bool force_duplicate       = false,
        const utility::Uuid &uuid_before = utility::Uuid(),
        const bool into                  = false);

    std::map<utility::Uuid, caf::actor> playlists_;
};

MediaCopyActor::MediaCopyActor(
    caf::actor_config &cfg,
    caf::actor session,
    caf::actor bookmarks,
    std::map<utility::Uuid, caf::actor> playlists)
    : caf::event_based_actor(cfg),
      session_(std::move(session)),
      bookmarks_(std::move(bookmarks)),
      playlists_(std::move(playlists)) {

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](playlist::copy_media_atom,
            const utility::Uuid &dst,
            const utility::Uuid &src,
            const UuidVector &media,
            const bool remove_source,
            const bool force_duplicate,
            const utility::Uuid &before,
            const bool into) -> result<UuidVector> {
            auto rp = make_response_promise<UuidVector>();
            copy_media_to(rp, dst, src, media, remove_source, force_duplicate, before, into);
            return rp;
        });
}


void MediaCopyActor::copy_media_to(
    caf::typed_response_promise<utility::UuidVector> &rp,
    const utility::Uuid &dst,
    const utility::Uuid &src,
    const utility::UuidVector &media,
    const bool remove_source,
    const bool force_duplicate,
    const utility::Uuid &uuid_before,
    const bool) {

    //  get media actors..
    try {
        // now find src..
        caf::scoped_actor sys(system());
        caf::actor src_actor;
        if (not src.is_null()) {
            if (playlists_.count(src))
                src_actor = playlists_[src];
            else {
                for (const auto &i : playlists_) {
                    try {
                        auto result = request_receive<std::vector<UuidActor>>(
                            *sys, i.second, playlist::get_container_atom_v, true);
                        for (const auto &ii : result) {
                            if (ii.uuid() == src) {
                                src_actor = ii.actor();
                                break;
                            }
                        }
                        if (src_actor)
                            break;
                    } catch (const XStudioError &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            }
        }

        // find target
        caf::actor target;
        caf::actor target_playlist;
        // now find dst..
        if (playlists_.count(dst)) {
            target = target_playlist = playlists_[dst];
        } else {
            // subgroup ?
            // find it and it's parent.
            for (const auto &i : playlists_) {
                try {
                    auto result = request_receive<std::vector<UuidActor>>(
                        *sys, i.second, playlist::get_container_atom_v, true);
                    for (const auto &ii : result) {
                        if (ii.uuid() == dst) {
                            target          = ii.actor();
                            target_playlist = i.second;
                            break;
                        }
                    }
                    if (target)
                        break;
                } catch (const XStudioError &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            }
        }

        // we should have target..
        if (not target) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, "Invalid destination uuid");
            rp.deliver(make_error(xstudio_error::error, "Invalid destination uuid"));
            return;
        }

        fan_out_request<policy::select_all>(
            map_value_to_vec(playlists_), infinite, playlist::get_media_atom_v)
            .then(
                [=](std::vector<std::vector<UuidActor>> media_ua) mutable {
                    try {
                        caf::scoped_actor sys(system());

                        auto parent_media = request_receive<std::vector<UuidActor>>(
                            *sys, target_playlist, playlist::get_media_atom_v);
                        auto my_media =
                            (target == target_playlist
                                 ? parent_media
                                 : request_receive<std::vector<UuidActor>>(
                                       *sys, target, playlist::get_media_atom_v));

                        UuidVector result;
                        utility::UuidActorVector target_media;

                        for (const auto &i : media_ua) {
                            for (const auto &ii : i) {
                                if (std::find(media.begin(), media.end(), ii.uuid()) !=
                                    std::end(media))
                                    target_media.push_back(ii);
                            }
                        }

                        // remove media that already exist in target.
                        auto parent_media_set = uuidactor_vect_to_uuid_set(parent_media);
                        auto my_media_set     = uuidactor_vect_to_uuid_set(my_media);
                        UuidVector removal_list;

                        //  collect media that needs adding/duplicating/ignoring
                        for (const auto &i : target_media) {
                            // already exists in target
                            if (my_media_set.count(i.uuid()) and not force_duplicate) {
                                result.push_back(i.uuid());
                                continue;
                            }
                            // missing from playlist
                            if (not parent_media_set.count(i.uuid()) or force_duplicate) {
                                // duplicate
                                auto new_media = request_receive<UuidUuidActor>(
                                    *sys,
                                    i.actor(),
                                    utility::duplicate_atom_v,
                                    bookmarks_,
                                    bookmarks_);

                                // add to playlist.
                                request_receive<UuidActor>(
                                    *sys,
                                    target_playlist,
                                    playlist::add_media_atom_v,
                                    new_media.second,
                                    target_playlist == target ? uuid_before : utility::Uuid());

                                //  add to subgroup
                                if (target_playlist != target) {
                                    spdlog::warn("add dup to sub");
                                    anon_send(
                                        target,
                                        playlist::add_media_atom_v,
                                        new_media.second.uuid(),
                                        uuid_before);
                                }

                                // remove source
                                if (remove_source)
                                    removal_list.push_back(i.uuid());

                                result.push_back(new_media.second.uuid());
                            } else {
                                // just adding to subgroup..
                                // we don't delete as this from the parent
                                anon_send(
                                    target, playlist::add_media_atom_v, i.uuid(), uuid_before);
                                result.push_back(i.uuid());
                                // but we do from the sibling
                                if (remove_source and src_actor) {
                                    anon_send(
                                        src_actor, playlist::remove_media_atom_v, i.uuid());
                                }
                            }
                        }
                        // all done just do removal..
                        if (not removal_list.empty()) {
                            fan_out_request<policy::select_all>(
                                map_value_to_vec(playlists_),
                                infinite,
                                playlist::remove_media_atom_v,
                                removal_list)
                                .then([=](std::vector<bool>) {}, [=](error &) {});
                        }

                        rp.deliver(result);

                    } catch (const XStudioError &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable { rp.deliver(std::move(err)); });
    } catch (const XStudioError &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}


class LoadUrisActor : public caf::event_based_actor {

  public:
    LoadUrisActor(caf::actor_config &cfg, caf::actor session, std::vector<caf::uri> uris);

    ~LoadUrisActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }
    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "LoadUrisActor";
    caf::behavior behavior_;
    caf::actor session_;
    std::vector<caf::uri> uris_;
    bool load_uris(const bool single_playlist = false);
};

LoadUrisActor::LoadUrisActor(
    caf::actor_config &cfg, caf::actor session, std::vector<caf::uri> uris)
    : caf::event_based_actor(cfg), session_(std::move(session)), uris_(std::move(uris)) {

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](load_uris_atom, const bool single_playlist) -> result<bool> {
            return load_uris(single_playlist);
        });
}

bool LoadUrisActor::load_uris(const bool single_playlist) {

    bool has_files = false;
    caf::actor playlist;

    if (single_playlist) {
        request(session_, infinite, add_playlist_atom_v, std::string("Untitled Playlist"))
            .then(
                [=](UuidUuidActor playlist) {
                    for (const auto &i : uris_) {
                        fs::path p(uri_to_posix_path(i));
                        if (not is_session(p.string()))
                            anon_send(
                                playlist.second.actor(),
                                playlist::add_media_atom_v,
                                i,
                                true,
                                Uuid());
                    }
                },
                [=](error &err) mutable {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } else {

        // scan URLS.. collect dirs and files..
        for (const auto &i : uris_) {
            fs::path p(uri_to_posix_path(i));
            if (fs::is_directory(p)) {
#ifdef _WIN32
                request(session_, infinite, add_playlist_atom_v, std::string(p.filename().string()))
#else
                request(session_, infinite, add_playlist_atom_v, std::string(p.filename()))
#endif

                    .then(
                        [=](UuidUuidActor playlist) {
                            anon_send(
                                playlist.second.actor(),
                                playlist::add_media_atom_v,
                                i,
                                true,
                                Uuid());
                        },
                        [=](error &err) mutable {
                            spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });

            } else {
                if (is_session(p.string()))
                    anon_send(session_, merge_session_atom_v, i);
                else
                    has_files = true;
            }
        }

        if (has_files) {
            request(session_, infinite, add_playlist_atom_v, std::string("Untitled Playlist"))
                .then(
                    [=](UuidUuidActor playlist) {
                        for (const auto &i : uris_) {
                            fs::path p(uri_to_posix_path(i));
                            if (!fs::is_directory(p) and not is_session(p.string()))
                                anon_send(
                                    playlist.second.actor(),
                                    playlist::add_media_atom_v,
                                    i,
                                    true,
                                    Uuid());
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }
    }
    return true;
}
} // namespace

SessionActor::~SessionActor() { spdlog::debug("{}", __PRETTY_FUNCTION__); }

SessionActor::SessionActor(
    caf::actor_config &cfg, const utility::JsonStore &jsn, const caf::uri &path)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"]), path) {
    // deserialize actors..
    // and inject into maps..

    if (not jsn.count("store") or jsn["store"].is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(),
            utility::JsonStore(R"({})"_json),
            std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(),
            static_cast<JsonStore>(jsn["store"]),
            std::chrono::milliseconds(50));
    }

    link_to(json_store_);

    if (not jsn.count("bookmarks") or jsn["bookmarks"].is_null()) {
        bookmarks_ = spawn<bookmark::BookmarksActor>();
    } else {
        bookmarks_ = spawn<bookmark::BookmarksActor>(static_cast<JsonStore>(jsn["bookmarks"]));
    }
    join_event_group(this, bookmarks_);
    link_to(bookmarks_);

    if (not jsn.count("tags") or jsn["tags"].is_null()) {
        tags_ = spawn<tag::TagActor>();
    } else {
        tags_ = spawn<tag::TagActor>(static_cast<JsonStore>(jsn["tags"]));
    }
    join_event_group(this, tags_);
    link_to(tags_);

    for (const auto &[key, value] : jsn["actors"].items()) {
        if (value["base"]["container"]["type"] == "Playlist") {
            try {
                playlists_[key] = spawn<playlist::PlaylistActor>(
                    static_cast<utility::JsonStore>(value), caf::actor_cast<caf::actor>(this));
                link_to(playlists_[key]);
                join_event_group(this, playlists_[key]);
            } catch (const std::exception &e) {
                spdlog::error("{}", e.what());
            }
        }
    }

    init();

    check_media_hook_plugin_version(jsn, path);
}

SessionActor::SessionActor(caf::actor_config &cfg, const std::string &name)
    : caf::event_based_actor(cfg), base_(name) {

    try {
        auto prefs = GlobalStoreHelper(system());
        base_.set_playhead_rate(
            FrameRate(1.0 / prefs.value<double>("/core/session/play_rate")));
        base_.set_media_rate(FrameRate(1.0 / prefs.value<double>("/core/session/media_rate")));
    } catch (...) {
    }

    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(),
        utility::JsonStore(R"({})"_json),
        std::chrono::milliseconds(50));
    link_to(json_store_);

    bookmarks_ = spawn<bookmark::BookmarksActor>();
    join_event_group(this, bookmarks_);
    link_to(bookmarks_);

    tags_ = spawn<tag::TagActor>();
    join_event_group(this, tags_);
    link_to(tags_);

    init();
}

void SessionActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    // monitor serilise targets.
    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        // if they don't unsubscribe we blow their data..!
        auto target = caf::actor_cast<caf::actor_addr>(msg.source);
        if (serialise_targets_.count(target)) {
            anon_send(json_store_, json_store::erase_json_atom_v, serialise_targets_[target]);
            serialise_targets_.erase(target);
            demonitor(msg.source);
        }
    });

    behavior_.assign(message_handler()
                         .or_else(bookmark::BookmarksActor::default_event_handler())
                         .or_else(playlist::PlaylistActor::default_event_handler())
                         .or_else(tag::TagActor::default_event_handler()));

    anon_send(caf::actor_cast<caf::actor>(this), bookmark::associate_bookmark_atom_v);
}

caf::message_handler SessionActor::message_handler() {
    return {
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),
        base_.make_ignore_error_handler(),
        make_get_version_handler(),

        [=](bookmark::associate_bookmark_atom) -> result<int> {
            auto rp = make_response_promise<int>();
            associate_bookmarks(rp);
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::milliseconds(1000),
                bookmark::associate_bookmark_atom_v);
            return rp;
        },

        [=](add_playlist_atom atom, const Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this), atom, "Untitled Playlist", uuid_before, false);
        },

        [=](add_playlist_atom atom, const std::string name) {
            delegate(actor_cast<caf::actor>(this), atom, name, Uuid(), false);
        },

        [=](add_playlist_atom, caf::actor actor, const Uuid &uuid_before, const bool into)
            -> result<std::pair<utility::Uuid, UuidActor>> {
            // try insert as requested, but add to end if it fails.
            auto rp = make_response_promise<std::pair<utility::Uuid, UuidActor>>();
            insert_playlist(rp, actor, uuid_before, into);
            return rp;
        },

        [=](add_playlist_atom,
            const std::string &name,
            const Uuid &uuid_before,
            const bool into) -> result<std::pair<utility::Uuid, UuidActor>> {
            // try insert as requested, but add to end if it fails.
            auto rp = make_response_promise<std::pair<utility::Uuid, UuidActor>>();
            create_playlist(rp, name, uuid_before, into);
            return rp;
        },

        [=](bookmark::get_bookmark_atom) -> caf::actor { return bookmarks_; },

        [=](media_hook::gather_media_sources_atom,
            const caf::actor &media,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            auto rp = make_response_promise<UuidActorVector>();
            gather_media_sources(rp, media, media_rate);
            return rp;
        },

        // gather sources for media and return new sources.
        [=](media_hook::gather_media_sources_atom atom, const caf::actor &media) {
            delegate(caf::actor_cast<caf::actor>(this), atom, media, base_.media_rate());
        },

        [=](tag::get_tag_atom) -> caf::actor { return tags_; },

        [=](get_playlist_atom) -> result<caf::actor> {
            // gets the first playlist
            if (!playlists_.size()) {
                return make_error(
                    xstudio_error::error, "get_playlist called on session with no playlists.");
            }
            return playlists_.begin()->second;
        },

        [=](get_playlist_atom, const Uuid &uuid) -> result<caf::actor> {
            // includes subgroups..
            if (playlists_.count(uuid))
                return playlists_[uuid];

            if (not playlists_.empty()) {
                auto rp = make_response_promise<caf::actor>();

                fan_out_request<policy::select_all>(
                    map_value_to_vec(playlists_),
                    infinite,
                    playlist::get_container_atom_v,
                    true)
                    .then(
                        [=](std::vector<std::vector<UuidActor>> results) mutable {
                            for (const auto &i : results) {
                                for (const auto &j : i) {
                                    if (j.uuid() == uuid)
                                        return rp.deliver(j.actor());
                                }
                            }
                            rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        // not safe, as names are not unique..// should be removed.
        [=](get_playlist_atom, const std::string &name) -> caf::actor {
            std::function<Uuid(const PlaylistTree &, const std::string &)>
                recursive_name_search;
            recursive_name_search = [&recursive_name_search](
                                        const PlaylistTree &tree,
                                        const std::string &name) -> Uuid {
                if (tree.name() == name) {
                    return tree.value_uuid();
                }
                for (auto i : tree.children_ref()) {
                    const Uuid uuid = recursive_name_search(i, name);
                    if (uuid != Uuid())
                        return uuid;
                }
                return Uuid();
            };

            Uuid match = recursive_name_search(base_.containers(), name);
            if (playlists_.count(match))
                return playlists_[match];
            return caf::actor();
        },

        [=](get_playlists_atom) -> std::vector<UuidActor> {
            // We use this method to build a list of playlist names for secondary UI playlist
            // menus and so-on. We want out list of playlists in these menus to match the order
            // of playlists in the session tree view.

            // flatten the tree of containers to a straight (ordered) list of
            // value uuids
            std::function<void(const PlaylistTree &, std::vector<Uuid> &)> flatten_tree;
            flatten_tree = [&flatten_tree](const PlaylistTree &tree, std::vector<Uuid> &rt) {
                rt.push_back(tree.value_uuid());
                for (auto i : tree.children_ref()) {
                    flatten_tree(i, rt);
                }
            };

            std::vector<Uuid> ordered_uuids;
            flatten_tree(base_.containers(), ordered_uuids);

            // make a list of the playlist actors that reflects their order
            // in the tree
            std::vector<UuidActor> actors;
            for (const auto &i : ordered_uuids) {
                auto p = playlists_.find(i);
                if (p != playlists_.end()) {
                    actors.emplace_back(p->first, p->second);
                }
            }
            return actors;
        },

        [=](global_store::save_atom atom) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                base_.filepath(),
                std::vector<utility::Uuid>(),
                static_cast<size_t>(0),
                true);
        },


        [=](global_store::save_atom atom, const caf::uri &path) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                path,
                std::vector<utility::Uuid>(),
                static_cast<size_t>(0),
                true);
        },

        [=](global_store::save_atom atom, const caf::uri &path, const size_t hash) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                path,
                std::vector<utility::Uuid>(),
                hash,
                true);
        },

        [=](global_store::save_atom atom,
            const caf::uri &path,
            const size_t hash,
            const bool update_path) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                path,
                std::vector<utility::Uuid>(),
                hash,
                update_path);
        },

        [=](global_store::save_atom atom,
            const caf::uri &path,
            const std::vector<utility::Uuid> &containers) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                path,
                containers,
                static_cast<size_t>(0),
                false);
        },

        [=](global_store::save_atom,
            const caf::uri &path,
            const std::vector<utility::Uuid> &containers,
            const size_t hash,
            const bool update_path) -> result<size_t> {
            if (path.path().empty())
                return make_error(xstudio_error::error, "Save path invalid.");

            auto rp = make_response_promise<size_t>();
            if (containers.empty()) {
                request(
                    actor_cast<caf::actor>(this),
                    std::chrono::seconds(60),
                    utility::serialise_atom_v)
                    .then(
                        [=](const utility::JsonStore &js) mutable {
                            save_json_to(rp, js, path, update_path, hash);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                request(
                    actor_cast<caf::actor>(this),
                    std::chrono::seconds(60),
                    utility::serialise_atom_v,
                    containers)
                    .then(
                        [=](const utility::JsonStore &js) mutable {
                            save_json_to(rp, js, path, false, hash);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            }

            return rp;
        },

        [=](json_store::get_json_atom atom, const std::string &path) {
            delegate(json_store_, atom, path);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            delegate(json_store_, atom, json, path);
        },

        [=](load_uris_atom, const std::vector<caf::uri> &uris, const bool single_playlist) {
            auto loader = system().spawn<LoadUrisActor>(actor_cast<caf::actor>(this), uris);
            anon_send(loader, load_uris_atom_v, single_playlist);
        },

        [=](media_rate_atom) -> FrameRate { return base_.media_rate(); },

        [=](media_rate_atom, const FrameRate &rate) {
            base_.set_media_rate(rate);
            send(event_group_, utility::event_atom_v, media_rate_atom_v, rate);
            base_.send_changed(event_group_, this);
            // force all playlists to ave the same new media rate ?
            for (auto &i : playlists_) {
                anon_send(i.second, media_rate_atom_v, rate);
            }
        },

        [=](remove_serialise_target_atom,
            const caf::actor target,
            const bool remove_data) -> bool {
            // remove from list, optional path to remove metadata. (garbage collection)
            auto target_addr = caf::actor_cast<caf::actor_addr>(target);
            if (serialise_targets_.count(target_addr)) {
                if (remove_data) {
                    anon_send(
                        json_store_,
                        json_store::erase_json_atom_v,
                        serialise_targets_[target_addr]);
                }
                serialise_targets_.erase(target_addr);
                demonitor(target);
                return true;
            }

            return false;
        },

        [=](merge_playlist_atom atom,
            const std::string &name,
            const utility::Uuid &before,
            const UuidVector &uuids) {
            std::vector<caf::actor> pls;
            try {
                for (const auto &uuid : uuids)
                    pls.push_back(playlists_[uuid]);
                delegate(actor_cast<caf::actor>(this), atom, name, before, pls);
            } catch (...) {
            }
        },

        [=](merge_playlist_atom,
            const std::string &name,
            const utility::Uuid &before,
            const std::vector<caf::actor> &playlists) -> result<utility::UuidUuidActor> {
            auto rp = make_response_promise<utility::UuidUuidActor>();

            // create merge destination
            request(
                actor_cast<caf::actor>(this),
                infinite,
                add_playlist_atom_v,
                name,
                before,
                false)
                .then(
                    [=](const utility::UuidUuidActor &merged) mutable {
                        // fanout..
                        fan_out_request<policy::select_all>(
                            playlists, infinite, playlist::copy_container_atom_v, bookmarks_)
                            .then(
                                [=](std::vector<utility::CopyResult> cr) mutable {
                                    // ordering will be wrong..
                                    // need to unpack.
                                    utility::CopyResult mcr;

                                    for (const auto &i : cr)
                                        mcr.push_back(i);

                                    request(
                                        merged.second.actor(),
                                        infinite,
                                        playlist::insert_container_atom_v,
                                        mcr,
                                        utility::Uuid(),
                                        false)
                                        .then(
                                            [=](const UuidVector &) mutable {
                                                // now add containers/actors. and remap sources.
                                                rp.deliver(merged);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(merged);
                                            });
                                },
                                [=](error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](merge_session_atom, caf::actor session) -> result<UuidVector> {
            // piggy back off of duplicate..
            auto rp = make_response_promise<UuidVector>();
            request(session, infinite, playlist::get_container_atom_v)
                .then(
                    [=](const PlaylistTree &pt) mutable {
                        duplicate_container(
                            rp, pt, session, utility::Uuid(), false, false, true);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](merge_session_atom, const caf::uri &path) -> result<UuidVector> {
            auto rp = make_response_promise<UuidVector>();

            try {
                auto session = spawn<session::SessionActor>(utility::open_session(path), path);
                rp.delegate(actor_cast<caf::actor>(this), merge_session_atom_v, session);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return make_error(xstudio_error::error, err.what());
            }

            return rp;
        },

        [=](playlist::move_media_atom,
            const Uuid &dst,
            const Uuid &src,
            const UuidVector &media,
            const Uuid &before,
            const bool into) {
            auto copy_actor = system().spawn<MediaCopyActor>(
                actor_cast<caf::actor>(this), bookmarks_, playlists_);
            delegate(
                copy_actor,
                playlist::copy_media_atom_v,
                dst,
                src,
                media,
                true,
                false,
                before,
                into);
        },
        [=](playlist::copy_media_atom,
            const Uuid &dst,
            const UuidVector &media,
            const bool force_duplicate,
            const Uuid &before,
            const bool into) {
            auto copy_actor = system().spawn<MediaCopyActor>(
                actor_cast<caf::actor>(this), bookmarks_, playlists_);
            delegate(
                copy_actor,
                playlist::copy_media_atom_v,
                dst,
                utility::Uuid(),
                media,
                false,
                force_duplicate,
                before,
                into);
        },

        [=](path_atom) -> std::pair<caf::uri, fs::file_time_type> {
            return std::make_pair(base_.filepath(), base_.session_file_mtime());
        },

        [=](path_atom, const caf::uri &uri) -> bool {
            base_.set_filepath(uri);
            send(
                event_group_,
                utility::event_atom_v,
                path_atom_v,
                std::make_pair(base_.filepath(), base_.session_file_mtime()));
            return true;
        },

        [=](playhead::playhead_rate_atom) -> FrameRate { return base_.playhead_rate(); },

        [=](playhead::playhead_rate_atom, const FrameRate &rate) {
            base_.set_playhead_rate(rate);
            base_.send_changed(event_group_, this);
        },

        [=](playlist::create_divider_atom,
            const std::string &name,
            const Uuid &uuid_before,
            const bool into) -> result<Uuid> {
            auto i = base_.insert_divider(name, uuid_before, into);
            if (i) {
                base_.send_changed(event_group_, this);
                send(event_group_, utility::event_atom_v, playlist::create_divider_atom_v, *i);
                return *i;
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        // [&](create_player_atom, const std::string name) -> result<caf::actor> {// 	// use
        // the first playlist as the default// 	caf::actor default_playlist = playlists_.size()
        // ? playlists_.begin()->second :// caf::actor();// 	auto rp =
        // make_response_promise<caf::actor>();// 	auto self =
        // caf::actor_cast<caf::actor>(this);//     auto player =
        // spawn<player::PlayerActor>(self, default_playlist, name);// 	link_to(player);//
        // request(player, infinite,
        // uuid_atom_v).await(// 		[=] (const Uuid &uuid) mutable {//
        // link_to(player);// 			players_[uuid] = player;//
        // rp.deliver(player);// 		},//         [=](error& err) mutable {//
        // rp.deliver(std::move(err));//        	}// 	);// 	return rp;// },
        [=](playlist::create_group_atom,
            const std::string &name,
            const Uuid &uuid_before) -> result<Uuid> {
            auto i = base_.insert_group(name, uuid_before);
            if (i) {
                base_.send_changed(event_group_, this);
                send(event_group_, utility::event_atom_v, playlist::create_group_atom_v, *i);
                return *i;
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](playlist::duplicate_container_atom,
            const Uuid &uuid,
            const Uuid &uuid_before,
            const bool into) -> result<UuidVector> {
            // may just be cosmetic, or possible involve actors.
            auto rp  = make_response_promise<UuidVector>();
            auto old = base_.containers().cfind(uuid);

            if (old) {
                duplicate_container(rp, **old, this, uuid_before, into);
                return rp;
            }
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](playlist::get_container_atom) -> PlaylistTree { return base_.containers(); },

        [=](playlist::get_container_atom, const Uuid &uuid) -> caf::result<PlaylistTree> {
            auto found = base_.containers().cfind(uuid);
            if (not found) {
                return make_error(xstudio_error::error, "Container not found");
            }

            return PlaylistTree(**found);
        },

        [=](bookmark::render_annotations_atom, const std::string path) {
            // This is for debugging but may be useful in the future. It will
            // render all the annotations in the session as a sequential set of
            // image files.
            // 'path' needs to be something like 'filename.%04d.exr' due
            // to sprintf below
            auto offscreen_renderer =
                system().registry().template get<caf::actor>(offscreen_viewport_registry);

            std::vector<utility::Uuid> d;
            request(bookmarks_, infinite, bookmark::bookmark_detail_atom_v, d)
                .then(
                    [=](std::vector<bookmark::BookmarkDetail> bd) mutable {
                        std::sort(
                            bd.begin(),
                            bd.end(),
                            [](const bookmark::BookmarkDetail &a,
                               const bookmark::BookmarkDetail &b) -> bool {
                                return *(a.logical_start_frame_) < *(b.logical_start_frame_);
                            });

                        int idx = 0;

                        std::array<char, 4096> buf;

                        for (const auto &d : bd) {
                            if (d.owner_ && d.logical_start_frame_) {
                                caf::actor owner = d.owner_->actor();
                                sprintf(buf.data(), path.c_str(), idx);
                                caf::uri u = posix_path_to_uri(buf.data());
                                request(
                                    offscreen_renderer,
                                    infinite,
                                    ui::viewport::render_viewport_to_image_atom_v,
                                    owner,
                                    *(d.logical_start_frame_),
                                    1920,
                                    1080,
                                    u)
                                    .then(
                                        [=](bool) mutable {},
                                        [=](caf::error &err) {
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        });
                                idx++;
                            }
                        }
                    },
                    [=](caf::error err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](playlist::get_media_atom) -> result<std::vector<UuidActor>> {
            if (playlists_.empty()) {
                return result<std::vector<UuidActor>>(std::vector<UuidActor>());
            }

            auto rp = make_response_promise<std::vector<UuidActor>>();
            fan_out_request<policy::select_all>(
                playlists(), infinite, playlist::get_media_atom_v)
                .then(
                    [=](std::vector<std::vector<UuidActor>> results) mutable {
                        std::vector<UuidActor> result;

                        for (const auto &i : results)
                            result.insert(result.end(), i.begin(), i.end());

                        rp.deliver(result);
                    },

                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](playlist::get_media_atom, const Uuid &uuid) -> result<caf::actor> {
            if (playlists_.empty()) {
                return make_error(xstudio_error::error, "Invalid uuid");
            }

            auto rp = make_response_promise<caf::actor>();

            fan_out_request<policy::select_all>(
                playlists(), infinite, playlist::get_media_atom_v, uuid, true)
                .then(
                    [=](std::vector<caf::actor> results) mutable {
                        caf::actor result;

                        for (const auto &i : results) {
                            if (i) {
                                result = i;
                                break;
                            }
                        }

                        if (result)
                            rp.deliver(result);
                        else
                            rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
                    },

                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](playlist::move_container_atom atom, const Uuid &uuid, const Uuid &uuid_before) {
            delegate(actor_cast<caf::actor>(this), atom, uuid, uuid_before, false);
        },

        [=](playlist::copy_container_to_atom,
            const Uuid &uuid,
            const UuidVector &uuids,
            const Uuid &uuid_before,
            const bool into) -> result<UuidVector> {
            auto rp = make_response_promise<UuidVector>();
            copy_containers_to(rp, uuid, uuids, uuid_before, into);
            return rp;
        },

        [=](playlist::move_container_to_atom,
            const Uuid &uuid,
            const UuidVector &uuids,
            const bool with_media,
            const Uuid &uuid_before,
            const bool into) -> result<UuidVector> {
            auto rp = make_response_promise<UuidVector>();
            move_containers_to(rp, uuid, uuids, with_media, uuid_before, into);
            return rp;
        },

        [=](playlist::move_container_atom,
            const Uuid &uuid,
            const Uuid &uuid_before,
            const bool into) -> bool {
            if (into && playlists_.count(uuid_before))
                return false;

            bool changed = base_.move_container(uuid, uuid_before, into);

            if (changed) {
                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    playlist::move_container_atom_v,
                    uuid,
                    uuid_before);
            }

            return changed;
        },

        [=](playlist::reflag_container_atom,
            const std::string &flag,
            const Uuid &uuid) -> bool {
            bool result = base_.reflag_container(flag, uuid);
            if (result) {
                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    playlist::reflag_container_atom_v,
                    uuid,
                    flag);
            }
            return result;
        },

        [=](playlist::remove_container_atom, const Uuid &cuuid) -> bool {
            bool result = false;
            // if group, remove children first..
            // should wait for those to succeed...
            utility::Uuid auuid;

            if (base_.cfind(cuuid)) {
                auuid = (*base_.cfind(cuuid))->value().uuid();
            } else
                return false;

            auto children = base_.container_children(cuuid);

            std::vector<Uuid> removed_playlist_uuids;

            if (children) {
                for (const auto &i : *children) {
                    if (playlists_.count(i)) {
                        auto a = playlists_[i];
                        removed_playlist_uuids.push_back(i);
                        unlink_from(a);
                        playlists_.erase(playlists_.find(i));
                        send_exit(a, caf::exit_reason::user_shutdown);
                    }
                }
            }

            if (base_.remove_container(cuuid)) {
                result = true;
                if (playlists_.count(auuid)) {
                    auto a = playlists_[auuid];
                    removed_playlist_uuids.push_back(auuid);
                    unlink_from(a);
                    playlists_.erase(playlists_.find(auuid));
                    send_exit(a, caf::exit_reason::user_shutdown);
                }
            }

            if (result) {
                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    playlist::remove_container_atom_v,
                    cuuid);
                send(
                    event_group_,
                    utility::event_atom_v,
                    playlist::remove_container_atom_v,
                    removed_playlist_uuids);
            }

            return result;
        },

        [=](playlist::rename_container_atom,
            const std::string &name,
            const Uuid &uuid) -> bool {
            bool result = base_.rename_container(name, uuid);
            if (result) {
                // also propergate to actor is there is one..
                auto found = base_.containers().cfind(uuid);
                if (found and playlists_.count((*found)->value().uuid()))
                    anon_send(playlists_[(*found)->value().uuid()], name_atom_v, name);

                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    playlist::rename_container_atom_v,
                    uuid,
                    name);
            }
            return result;
        },

        [=](session::current_playlist_atom) -> result<caf::actor> {
            auto a = actor_cast<caf::actor>(current_playlist_);
            if (a)
                return a;

            return make_error(xstudio_error::error, "No current playlist");
        },

        [=](session::current_playlist_atom, caf::actor actor, bool broadcast) {
            current_playlist_ = actor_cast<caf::actor_addr>(actor);

            if (broadcast) {
                send(
                    event_group_,
                    utility::event_atom_v,
                    session::current_playlist_atom_v,
                    actor);
            }
        },

        [=](session::current_playlist_atom, caf::actor actor) {
            current_playlist_ = actor_cast<caf::actor_addr>(actor);
            send(event_group_, utility::event_atom_v, session::current_playlist_atom_v, actor);
        },

        [=](utility::event_atom, playlist::add_media_atom, const UuidActor &ua) {
            send(event_group_, utility::event_atom_v, playlist::add_media_atom_v, ua);
        },

        [=](utility::event_atom, playlist::remove_media_atom, const UuidVector &) {},

        // still exist ?
        [=](utility::event_atom, playlist::remove_container_atom, const std::vector<Uuid> &) {},

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::serialise_atom,
            const caf::actor &actor,
            const JsonStore &data) -> result<bool> {
            // allow actor to push it's serialisation data.
            auto target_addr = caf::actor_cast<caf::actor_addr>(actor);
            if (serialise_targets_.count(target_addr)) {
                anon_send(
                    json_store_,
                    json_store::set_json_atom_v,
                    data,
                    serialise_targets_[target_addr]);
                return true;
            }

            return false;
        },
        [=](utility::serialise_atom, const bool) -> result<bool> {
            auto rp = make_response_promise<bool>();
            sync_to_json_store(rp);
            return rp;
        },
        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            // flush abitary data to our store.
            request(
                caf::actor_cast<caf::actor>(this), infinite, utility::serialise_atom_v, true)
                .then(
                    [=](const bool) mutable {
                        auto stores = std::make_shared<std::map<std::string, JsonStore>>();

                        request(
                            system().registry().template get<caf::actor>(global_store_registry),
                            infinite,
                            utility::serialise_atom_v,
                            "SESSION")
                            .then(
                                [=](const JsonStore &result) mutable {
                                    (*stores)["global_store"] = result;
                                    check_save_serialise_payload(stores, rp);
                                },
                                [=](error &err) mutable {
                                    spdlog::warn(
                                        "{} global_store {}",
                                        __PRETTY_FUNCTION__,
                                        to_string(err));
                                    rp.deliver(std::move(err));
                                });

                        request(json_store_, infinite, json_store::get_json_atom_v)
                            .then(
                                [=](const JsonStore &result) mutable {
                                    (*stores)["store"] = result;
                                    check_save_serialise_payload(stores, rp);
                                },
                                [=](error &err) mutable {
                                    spdlog::warn(
                                        "{} store {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(std::move(err));
                                });

                        request(bookmarks_, infinite, utility::serialise_atom_v)
                            .then(
                                [=](const JsonStore &result) mutable {
                                    (*stores)["bookmarks"] = result;
                                    check_save_serialise_payload(stores, rp);
                                },
                                [=](error &err) mutable {
                                    spdlog::warn(
                                        "{} bookmarks {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(std::move(err));
                                });

                        request(tags_, infinite, utility::serialise_atom_v)
                            .then(
                                [=](const JsonStore &result) mutable {
                                    (*stores)["tags"] = result;
                                    check_save_serialise_payload(stores, rp);
                                },
                                [=](error &err) mutable {
                                    spdlog::warn(
                                        "{} tags {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(std::move(err));
                                });

                        request(
                            system().registry().template get<caf::actor>(media_hook_registry),
                            infinite,
                            utility::serialise_atom_v)
                            .then(
                                [=](const JsonStore &result) mutable {
                                    (*stores)["media_hook_versions"] = result;
                                    check_save_serialise_payload(stores, rp);
                                },
                                [=](error &err) mutable {
                                    spdlog::warn(
                                        "{} media_hook_versions {}",
                                        __PRETTY_FUNCTION__,
                                        to_string(err));
                                    rp.deliver(std::move(err));
                                });

                        if (playlists().empty()) {
                            (*stores)["actors"] = JsonStore(R"({})"_json);
                        } else {
                            fan_out_request<policy::select_all>(
                                playlists(), infinite, serialise_atom_v)
                                .then(
                                    [=](std::vector<JsonStore> json) mutable {
                                        (*stores)["actors"] = {};
                                        for (const auto &j : json) {
                                            (*stores)["actors"][static_cast<std::string>(
                                                j["base"]["container"]["uuid"])] = j;
                                        }
                                        check_save_serialise_payload(stores, rp);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} actors {}",
                                            __PRETTY_FUNCTION__,
                                            to_string(err));
                                        rp.deliver(std::move(err));
                                    });
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](utility::serialise_atom,
            const std::vector<utility::Uuid> &containers) -> result<JsonStore> {
            caf::scoped_actor sys(system());
            // get session json
            auto gsa = system().registry().template get<caf::actor>(global_store_registry);

            try {

                auto global_store_json =
                    request_receive<JsonStore>(*sys, gsa, utility::serialise_atom_v, "SESSION");

                // get my json
                auto local_json =
                    request_receive<JsonStore>(*sys, json_store_, json_store::get_json_atom_v);

                // clone and serialise only selected containers.
                // find actor uuid from container uuids.
                std::vector<caf::actor> clients;
                auto new_session = Session(base_);
                auto selection   = base_.containers().intersect(containers);
                if (selection)
                    new_session.set_container(*selection);
                else
                    new_session.set_container(
                        PlaylistTree(base_.containers().name(), base_.containers().type()));

                // pickup remaining playlists.
                for (const auto &i : new_session.containers().children_uuid()) {
                    if (playlists_.count(i))
                        clients.push_back(playlists_[i]);
                }

                if (not clients.empty()) {
                    auto rp = make_response_promise<JsonStore>();
                    fan_out_request<policy::select_all>(clients, infinite, serialise_atom_v)
                        .then(
                            [=](std::vector<JsonStore> json) mutable {
                                JsonStore jsn;
                                jsn["_Application_"] = "xStudio";
                                jsn["base"]          = new_session.serialise();
                                jsn["actors"]        = {};
                                jsn["global_store"]  = global_store_json;
                                jsn["store"]         = local_json;
                                for (const auto &j : json) {
                                    jsn["actors"][static_cast<std::string>(
                                        j["base"]["container"]["uuid"])] = j;
                                }
                                rp.deliver(jsn);
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });

                    return rp;
                }
                JsonStore jsn;
                jsn["_Application_"] = "xStudio";
                jsn["base"]          = new_session.serialise();
                jsn["actors"]        = {};
                jsn["global_store"]  = global_store_json;
                jsn["store"]         = local_json;

                return result<JsonStore>(jsn);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
        },
        [=](ui::open_quickview_window_atom,
            const utility::UuidActorVector &media_items,
            std::string compare_mode,
            bool force) {
            // forward to the studio actor
            anon_send(
                home_system().registry().get<caf::actor>(studio_registry),
                ui::open_quickview_window_atom_v,
                media_items,
                compare_mode,
                force);
        }};
}

void SessionActor::check_save_serialise_payload(
    std::shared_ptr<std::map<std::string, JsonStore>> &payload,
    caf::typed_response_promise<utility::JsonStore> &rp) {

    // all data must be present
    if (not payload->count("global_store"))
        return;
    if (not payload->count("store"))
        return;
    if (not payload->count("bookmarks"))
        return;
    if (not payload->count("tags"))
        return;
    if (not payload->count("media_hook_versions"))
        return;
    if (not payload->count("actors"))
        return;

    JsonStore jsn;
    jsn["_Application_"]       = "xStudio";
    jsn["base"]                = base_.serialise();
    jsn["actors"]              = (*payload)["actors"];
    jsn["global_store"]        = (*payload)["global_store"];
    jsn["store"]               = (*payload)["store"];
    jsn["bookmarks"]           = (*payload)["bookmarks"];
    jsn["tags"]                = (*payload)["tags"];
    jsn["media_hook_versions"] = (*payload)["media_hook_versions"];

    rp.deliver(jsn);
}


void SessionActor::on_exit() { spdlog::debug("{} {}", __PRETTY_FUNCTION__, base_.name()); }

void SessionActor::sync_to_json_store(caf::typed_response_promise<bool> &rp) {
    // sync all registered actors.
    auto actors = std::make_shared<std::map<caf::actor, std::string>>();

    for (const auto &i : serialise_targets_) {
        auto a = caf::actor_cast<caf::actor>(i.first);
        if (a)
            (*actors)[a] = i.second;
    }

    if (actors->empty())
        rp.deliver(true);

    for (const auto &i : *actors) {
        request(i.first, infinite, utility::serialise_atom_v)
            .then(
                [=](const JsonStore &json) mutable {
                    auto target = caf::actor_cast<caf::actor>(current_sender());
                    request(json_store_, infinite, json_store::set_json_atom_v, json, i.second)
                        .then(
                            [=](const bool) mutable {
                                (*actors).erase(target);
                                if (actors->empty())
                                    rp.deliver(true);
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                (*actors).erase(target);
                                if (actors->empty())
                                    rp.deliver(true);
                            });
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    (*actors).erase(caf::actor_cast<caf::actor>(current_sender()));
                    if (actors->empty())
                        rp.deliver(true);
                });
    }
}


void SessionActor::create_playlist(
    caf::typed_response_promise<std::pair<utility::Uuid, UuidActor>> &rp,
    std::string name,
    const utility::Uuid &uuid_before,
    const bool into) {
    auto actor = spawn<playlist::PlaylistActor>(
        name, utility::Uuid(), caf::actor_cast<caf::actor>(this));
    anon_send(actor, media_rate_atom_v, base_.media_rate());
    anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
    create_container(actor, rp, uuid_before, into);
}

void SessionActor::insert_playlist(
    caf::typed_response_promise<std::pair<utility::Uuid, UuidActor>> &rp,
    caf::actor actor,
    const utility::Uuid &uuid_before,
    const bool into) {
    anon_send(actor, media_rate_atom_v, base_.media_rate());
    anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
    create_container(actor, rp, uuid_before, into);
}

void SessionActor::create_container(
    caf::actor actor,
    caf::typed_response_promise<std::pair<utility::Uuid, UuidActor>> rp,
    const Uuid &uuid_before,
    const bool into) {

    request(actor, infinite, detail_atom_v)
        .await(
            [=](const ContainerDetail &detail) mutable {
                playlists_[detail.uuid_] = actor;
                link_to(actor);
                join_event_group(this, actor);
                PlaylistItem tmp(detail.name_, detail.type_, detail.uuid_);
                auto cuuid = base_.insert_container(tmp, uuid_before, into);
                if (not cuuid) {
                    cuuid = base_.insert_container(tmp);
                }
                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    add_playlist_atom_v,
                    UuidActor(detail.uuid_, actor));

                rp.deliver(std::make_pair(*cuuid, UuidActor(detail.uuid_, actor)));
            },
            [=](error &err) mutable { rp.deliver(err); });
}

// this is gonna be fun...
// we need to serialise all items under and including uuid
// and also change uuids, and update linkage..
// not sure how we deal with hidden children
// I think the actors themselves need to support a clone atom..

void SessionActor::duplicate_container(
    caf::typed_response_promise<UuidVector> rp,
    const utility::UuidTree<utility::PlaylistItem> &tree,
    caf::actor source_session,
    const utility::Uuid &uuid_before,
    const bool into,
    const bool rename,
    const bool kill_source) {
    // find all actor children.. (Timeline/SUBSET/CONTACTSHEET)
    // quick clone of divider..

    // clone structure
    PlaylistTree new_tree(tree);
    // regenerate uuids..
    new_tree.reset_uuid(true);
    duplicate_tree(new_tree, source_session, rename);

    std::function<void(const PlaylistTree &, std::vector<Uuid> &)> flatten_tree;
    flatten_tree = [&flatten_tree](const PlaylistTree &tree, std::vector<Uuid> &rt) {
        rt.push_back(tree.uuid());
        for (auto i : tree.children_ref()) {
            flatten_tree(i, rt);
        }
    };

    std::vector<Uuid> ordered_uuids;

    // If we clone a session we need to reparent the children of the tree.
    // As they sit under the session item.
    if (new_tree.value().type() == "Session" or new_tree.value().type() == "") {
        // reparent..
        for (auto &i : new_tree.children_) {
            flatten_tree(i, ordered_uuids);
            base_.insert_container(i, uuid_before, into);
        }

    } else {
        flatten_tree(new_tree, ordered_uuids);
        base_.insert_container(new_tree, uuid_before, into);
    }

    base_.send_changed(event_group_, this);

    if (kill_source)
        send_exit(source_session, caf::exit_reason::user_shutdown);

    rp.deliver(ordered_uuids);
}

// session actors container groups/dividers and playlists..
void SessionActor::duplicate_tree(
    utility::UuidTree<utility::PlaylistItem> &tree,
    caf::actor source_session,
    const bool rename) {

    if (rename)
        tree.value().set_name(tree.value().name() + " - copy");

    if (tree.value().type() == "ContainerDivider") {
        tree.value().set_uuid(Uuid::generate());
        send(event_group_, utility::event_atom_v, playlist::create_divider_atom_v, tree.uuid());
    } else if (tree.value().type() == "ContainerGroup") {
        tree.value().set_uuid(Uuid::generate());
        send(event_group_, utility::event_atom_v, playlist::create_group_atom_v, tree.uuid());
    } else if (tree.value().type() == "Playlist") {
        // need to issue a duplicate action, as we actors are blackboxes..
        // try not to confuse this with duplicating a container, as opposed to the actor..
        // we need to insert the new playlist in to the session and update the UUID
        try {
            caf::scoped_actor sys(system());

            // request playlist actor from source, may not be local..
            caf::actor playlist;
            caf::actor bookmarks = bookmarks_;
            if (source_session == this)
                playlist = playlists_[tree.value().uuid()];
            else {
                // request bookmarks..
                bookmarks = request_receive<caf::actor>(
                    *sys, source_session, bookmark::get_bookmark_atom_v);
                playlist = request_receive<caf::actor>(
                    *sys, source_session, get_playlist_atom_v, tree.value().uuid());
            }

            auto result = request_receive<UuidActor>(
                *sys, playlist, duplicate_atom_v, bookmarks, bookmarks_);

            tree.value().set_uuid(result.uuid());
            playlists_[result.uuid()] = result.actor();
            if (rename) {
                auto name = request_receive<std::string>(*sys, result.actor(), name_atom_v);
                tree.value().set_name(name + " - copy");
                send(result.actor(), name_atom_v, tree.value().name());
            }

            link_to(result.actor());
            send(event_group_, utility::event_atom_v, add_playlist_atom_v, result);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            // set to invalid uuid ?
            tree.value().set_uuid(Uuid());
        }
    }

    for (auto &i : tree.children_) {
        duplicate_tree(i, source_session);
    }
}

void SessionActor::copy_containers_to(
    caf::typed_response_promise<utility::UuidVector> &rp,
    const utility::Uuid &playlist,
    const utility::UuidVector &containers,
    const utility::Uuid &uuid_before,
    const bool into) {
    // find destination..
    // find containers
    // copy and send to destination
    // return list of new items.

    // make sure playlist is valid
    auto dst = base_.containers().cfind_any(playlist);
    if (not dst or (*dst)->value().type() != "Playlist") {
        rp.deliver(make_error(xstudio_error::error, "Invalid playlist uuid"));
    } else {
        // got dest..
        auto dst_actor = playlists_[(*dst)->value().uuid()];

        // find sources.. We don't know which playlists these exist in
        // build a playlist/containers map?
        // just fanout..
        fan_out_request<policy::select_all>(
            playlists(), infinite, playlist::copy_container_atom_v, containers, bookmarks_)
            .then(
                [=](std::vector<utility::CopyResult> cr) mutable {
                    // ordering will be wrong..
                    // need to unpack.
                    utility::CopyResult mcr;
                    for (const auto &i : cr)
                        mcr.push_back(i);

                    request(
                        dst_actor,
                        infinite,
                        playlist::insert_container_atom_v,
                        mcr,
                        uuid_before,
                        into)
                        .then(
                            [=](const utility::UuidVector &newitems) mutable {
                                // now add containers/actors. and remap sources.
                                rp.deliver(newitems);
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(err);
                            });
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}

void SessionActor::move_containers_to(
    caf::typed_response_promise<utility::UuidVector> &rp,
    const utility::Uuid &playlist,
    const utility::UuidVector &containers,
    const bool with_media,
    const utility::Uuid &uuid_before,
    const bool into) {
    // find destination..
    // find containers
    // copy and send to destination
    // return list of new items.

    // make sure playlist is valid
    auto dst = base_.containers().cfind_any(playlist);
    if (not dst or (*dst)->value().type() != "Playlist") {
        rp.deliver(make_error(xstudio_error::error, "Invalid playlist uuid"));
    } else {
        // got dest..
        auto dst_actor = playlists_[(*dst)->value().uuid()];

        // find sources.. We don't know which playlists these exist in
        // build a playlist/containers map?
        // just fanout..
        fan_out_request<policy::select_all>(
            playlists(), infinite, playlist::copy_container_atom_v, containers, bookmarks_)
            .then(
                [=](std::vector<utility::CopyResult> cr) mutable {
                    // ordering will be wrong..
                    // need to unpack.
                    // remove src containers
                    fan_out_request<policy::select_all>(
                        playlists(),
                        infinite,
                        playlist::remove_container_atom_v,
                        containers,
                        with_media)
                        .then(
                            [=](std::vector<bool>) mutable {},
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });

                    utility::CopyResult mcr;
                    for (const auto &i : cr)
                        mcr.push_back(i);

                    request(
                        dst_actor,
                        infinite,
                        playlist::insert_container_atom_v,
                        mcr,
                        uuid_before,
                        into)
                        .then(
                            [=](const utility::UuidVector &newitems) mutable {
                                // now add containers/actors. and remap sources.
                                rp.deliver(newitems);
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(err);
                            });
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}

void SessionActor::save_json_to(
    caf::typed_response_promise<size_t> &rp,
    const utility::JsonStore &js,
    const caf::uri &path,
    const bool update_path,
    const size_t hash) {

    size_t new_hash = 0;

    try {
        auto data = js.dump(2);

        auto resolve_link = false;
        new_hash          = std::hash<std::string>{}(data);

        // no change in hash, so skip save (autosave)
        if (new_hash == hash) {
            return rp.deliver(new_hash);
        }

        // fix something ?
        auto ppath = utility::posix_path_to_uri(utility::uri_to_posix_path(path));

        // try and save, we are already looking at this file
        if (update_path) {
            // same path as session, are we allowed ?
            resolve_link = true;
        }

        auto save_path = uri_to_posix_path(ppath);
        if (resolve_link && fs::exists(save_path) && fs::is_symlink(save_path))
#ifdef _WIN32
            save_path = fs::canonical(save_path).string();
#else
            save_path = fs::canonical(save_path);
#endif


        // compress data.
        if (to_lower(path_to_string(fs::path(save_path).extension())) == ".xsz") {
            zstr::ofstream o(save_path + ".tmp");
            try {
                o.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                // if(not o.is_open())
                //     throw std::runtime_error();
                o << std::setw(4) << data << std::endl;
                o.close();
            } catch (const std::exception &) {
                // remove failed file
                if (o.is_open()) {
                    o.close();
                    fs::remove(save_path + ".tmp");
                }
                throw std::runtime_error("Failed to open file");
            }
        } else {
            // this maybe a symlink in which case we should resolve it.
            std::ofstream o(save_path + ".tmp");
            try {
                o.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                // if(not o.is_open())
                //     throw std::runtime_error();
                o << std::setw(4) << data << std::endl;
                o.close();
            } catch (const std::exception &) {
                // remove failed file
                if (o.is_open()) {
                    o.close();
                    fs::remove(save_path + ".tmp");
                }
                throw std::runtime_error("Failed to open file");
            }
        }

        // rename tmp to final name
        fs::rename(save_path + ".tmp", save_path);

        if (update_path) {
            base_.set_filepath(path);
            send(
                event_group_,
                utility::event_atom_v,
                path_atom_v,
                std::make_pair(base_.filepath(), base_.session_file_mtime()));
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        return rp.deliver(make_error(xstudio_error::error, err.what()));
    }

    rp.deliver(new_hash);
}

void SessionActor::associate_bookmarks(caf::typed_response_promise<int> &rp) {
    fan_out_request<policy::select_all>(playlists(), infinite, playlist::get_media_atom_v)
        .then(
            [=](std::vector<std::vector<UuidActor>> media_ua) mutable {
                // turn into single vector..
                UuidActorVector result;
                for (const auto &i : media_ua)
                    result.insert(result.end(), i.begin(), i.end());
                rp.delegate(bookmarks_, bookmark::associate_bookmark_atom_v, result);
                // rp.deliver(20);
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });
}

void SessionActor::check_media_hook_plugin_version(
    const utility::JsonStore &jsn, const caf::uri &path) {
    // here we check if the plugin versions for the media hook that are available
    // match the versions that were available when a session file (that we are
    // now loading) was created. If they don't match, we re-run the media hook
    // across all media to ensure that metadata is constructed according to the
    // latest plugin.

    if (playlists_.empty())
        return;

    auto hooks = system().registry().template get<caf::actor>(media_hook_registry);

    utility::JsonStore media_hook_plugin_serialised_info;

    if (jsn.contains("media_hook_versions")) {
        media_hook_plugin_serialised_info = jsn["media_hook_versions"];
    }

    request(
        hooks,
        infinite,
        media_hook::check_media_hook_plugin_versions_atom_v,
        media_hook_plugin_serialised_info)
        .then(
            [=](bool hook_plugin_versions_ok) {
                if (!hook_plugin_versions_ok) {
                    // now we need to re-run media hook on all media items .
                    // Get all media items from all playlists, then send them
                    // the get_media_hook_atom

                    spdlog::warn(
                        "A mismtach in plugin versions between session file {} and current "
                        "available plugins has been detected. Re-fetching metadata for all "
                        "media items.",
                        uri_to_posix_path(path));

                    fan_out_request<policy::select_all>(
                        map_value_to_vec(playlists_), infinite, playlist::get_media_atom_v)
                        .then(
                            [=](const std::vector<std::vector<UuidActor>> media_ua) mutable {
                                for (const auto &v : media_ua) {
                                    for (const auto &m : v) {
                                        anon_send(m.actor(), media_hook::get_media_hook_atom_v);
                                    }
                                }
                            },
                            [=](const caf::error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                }
            },
            [=](const caf::error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SessionActor::gather_media_sources(
    caf::typed_response_promise<utility::UuidActorVector> &rp,
    const caf::actor &media,
    const FrameRate &media_rate) {
    auto hook    = system().registry().template get<caf::actor>(media_hook_registry);
    auto sources = utility::UuidActorVector();
    if (hook) {
        gather_media_sources_media_hook(rp, media, media_rate, sources);
    } else {
        gather_media_sources_data_source(rp, media, media_rate, sources);
    }
}

void SessionActor::gather_media_sources_media_hook(
    caf::typed_response_promise<utility::UuidActorVector> &rp,
    const caf::actor &media,
    const FrameRate &media_rate,
    utility::UuidActorVector &sources) {
    // This allows us to run studio specific code to add media sources
    // to a media item, particularly to add proxy resolutions or
    // multiple QTs that might have different codecs etc. We're
    // doing 'anon_send' so we can do non-blocking evaluation and
    // push results to the MediaActor as it might be slow
    auto hook = system().registry().template get<caf::actor>(media_hook_registry);

    if (hook) {
        request(hook, infinite, media_hook::gather_media_sources_atom_v, media, media_rate)
            .then(
                [=](const UuidActorVector &dsources) mutable {
                    sources.insert(sources.end(), dsources.begin(), dsources.end());
                    gather_media_sources_data_source(rp, media, media_rate, sources);
                },
                [=](const caf::error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });
    } else {
        gather_media_sources_data_source(rp, media, media_rate, sources);
    }
}

void SessionActor::gather_media_sources_data_source(
    caf::typed_response_promise<utility::UuidActorVector> &rp,
    const caf::actor &media,
    const FrameRate &media_rate,
    utility::UuidActorVector &sources) {
    auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);

    if (pm) {
        request(pm, infinite, data_source::use_data_atom_v, media, media_rate)
            .then(
                [=](const UuidActorVector &dsources) mutable {
                    sources.insert(sources.end(), dsources.begin(), dsources.end());
                    gather_media_sources_add_media(rp, media, sources);
                },
                [=](const caf::error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });
    } else {
        gather_media_sources_add_media(rp, media, sources);
    }
}

void SessionActor::gather_media_sources_add_media(
    caf::typed_response_promise<utility::UuidActorVector> &rp,
    const caf::actor &media,
    utility::UuidActorVector &sources) {
    // add sources to media
    // we may have duplicates

    // build list of current source/name pairs
    // build new candidates from list.

    // prune and kill.. then add.
    if (sources.empty())
        return rp.deliver(sources);

    request(media, infinite, media::get_media_source_names_atom_v)
        .then(
            [=](const std::vector<std::pair<utility::Uuid, std::string>>
                    &current_names) mutable {
                // get names from sources..
                fan_out_request<policy::select_all>(
                    vector_to_caf_actor_vector(sources), infinite, utility::detail_atom_v)
                    .then(
                        [=](const std::vector<ContainerDetail> details) mutable {
                            auto cnameset        = vpair_second_to_s(current_names);
                            auto deduped_sources = utility::UuidActorVector();

                            for (const auto &i : details) {
                                if (not cnameset.count(i.name_)) {
                                    deduped_sources.emplace_back(UuidActor(i.uuid_, i.actor_));
                                } else {
                                    send_exit(i.actor_, caf::exit_reason::user_shutdown);
                                }
                            }

                            request(
                                media,
                                infinite,
                                media::add_media_source_atom_v,
                                deduped_sources)
                                .then(
                                    [=](const bool) mutable { rp.deliver(deduped_sources); },
                                    [=](const caf::error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        rp.deliver(err);
                                    });
                        },
                        [=](const caf::error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}
