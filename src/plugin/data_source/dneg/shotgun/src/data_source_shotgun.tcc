
#include <fmt/format.h>
#include <caf/policy/select_all.hpp>

#include "data_source_shotgun.hpp"
#include "data_source_shotgun_worker.hpp"
#include "data_source_shotgun_definitions.hpp"

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/event/event.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/shotgun_client/shotgun_client_actor.hpp"
#include "xstudio/tag/tag.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace std::chrono_literals;


/*CAF_BEGIN_TYPE_ID_BLOCK(shotgun, xstudio::shotgun_client::shotgun_client_error)
CAF_ADD_ATOM(shotgun, xstudio::shotgun_client, test_atom)
CAF_END_TYPE_ID_BLOCK(shotgun)*/

// Datasource should support a common subset of operations that apply to multiple datasources.
// not idea what they are though.
// get and put should try and map from this to the relevant sources.

// shotgun piggy backs on the shotgun client actor, so most of the work is done in the actor
// class. because shotgun is very flexible, it's hard to write helpers, as entities/properties
// are entirely configurable. but we also don't want to put all the logic into the frontend. as
// python module may want access to this logic.

// This value helps tune the rate that jobs to build media are processed, if it
// is zero xstudio tends to get overwhelmed when building large playlists, increasing
// the value means xstudio stays interactive at the cost of slowing the overall
#define JOB_DISPATCH_DELAY std::chrono::milliseconds(10)

#include "data_source_shotgun_action.tcc"
#include "data_source_shotgun_get_actions.tcc"
#include "data_source_shotgun_put_actions.tcc"
#include "data_source_shotgun_post_actions.tcc"

template <typename T>
void ShotgunDataSourceActor<T>::attribute_changed(const utility::Uuid &attr_uuid) {
    // properties changed somewhere.
    // update loop ?
    if (attr_uuid == data_source_.authentication_method_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            data_source_.authentication_method_->value(),
            "/plugin/data_source/shotgun/authentication/grant_type");
    }
    if (attr_uuid == data_source_.client_id_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            data_source_.client_id_->value(),
            "/plugin/data_source/shotgun/authentication/client_id");
    }
    // if (attr_uuid == data_source_.client_secret_->uuid()) {
    //     auto prefs = GlobalStoreHelper(system());
    //     prefs.set_value(data_source_.client_secret_->value(),
    //     "/plugin/data_source/shotgun/authentication/client_secret");
    // }
    if (attr_uuid == data_source_.timeout_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            data_source_.timeout_->value(), "/plugin/data_source/shotgun/server/timeout");
    }

    if (attr_uuid == data_source_.username_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            data_source_.username_->value(),
            "/plugin/data_source/shotgun/authentication/username");
    }
    // if (attr_uuid == data_source_.password_->uuid()) {
    //     auto prefs = GlobalStoreHelper(system());
    //     prefs.set_value(data_source_.password_->value(),
    //     "/plugin/data_source/shotgun/authentication/password");
    // }
    if (attr_uuid == data_source_.session_token_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            data_source_.session_token_->value(),
            "/plugin/data_source/shotgun/authentication/session_token");
    }
}


template <typename T>
ShotgunDataSourceActor<T>::ShotgunDataSourceActor(
    caf::actor_config &cfg, const utility::JsonStore &)
    : caf::event_based_actor(cfg) {

    data_source_.bind_attribute_changed_callback(
        [this](auto &&PH1) { attribute_changed(std::forward<decltype(PH1)>(PH1)); });

    spdlog::debug("Created ShotgunDataSourceActor {}", name());
    // print_on_exit(this, "MediaHookActor");
    secret_source_ = actor_cast<caf::actor_addr>(this);

    shotgun_ = spawn<ShotgunClientActor>();
    link_to(shotgun_);

    // we need to recieve authentication updates.
    join_event_group(this, shotgun_);

    // we are the source of the secret..
    anon_send(shotgun_, shotgun_authentication_source_atom_v, actor_cast<caf::actor>(this));


    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        update_preferences(j);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if(not disable_integration_)
        system().registry().put(shotgun_datasource_registry, caf::actor_cast<caf::actor>(this));

    pool_ = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count_,
        [&] {
            return system().template spawn<ShotgunMediaWorker>(
                actor_cast<caf::actor_addr>(this));
        },
        caf::actor_pool::round_robin());
    link_to(pool_);

    // data_source_.connect_to_ui(); coz async
    data_source_.set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
    delayed_anon_send(
        caf::actor_cast<caf::actor>(this),
        std::chrono::milliseconds(500),
        module::connect_to_ui_atom_v);

    behavior_.assign(
        [=](utility::name_atom) -> std::string { return name(); },
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](shotgun_projects_atom atom) { delegate(shotgun_, atom); },

        [=](shotgun_groups_atom atom, const int project_id) {
            delegate(shotgun_, atom, project_id);
        },

        [=](shotgun_schema_atom atom, const int project_id) {
            delegate(shotgun_, atom, project_id);
        },

        [=](shotgun_authentication_source_atom, caf::actor source) {
            secret_source_ = actor_cast<caf::actor_addr>(source);
        },

        [=](shotgun_authentication_source_atom) -> caf::actor {
            return actor_cast<caf::actor>(secret_source_);
        },

        [=](shotgun_update_entity_atom atom,
            const std::string &entity,
            const int record_id,
            const JsonStore &body) { delegate(shotgun_, atom, entity, record_id, body); },

        [=](shotgun_image_atom atom,
            const std::string &entity,
            const int record_id,
            const bool thumbnail) { delegate(shotgun_, atom, entity, record_id, thumbnail); },

        [=](shotgun_delete_entity_atom atom, const std::string &entity, const int record_id) {
            delegate(shotgun_, atom, entity, record_id);
        },

        [=](shotgun_image_atom atom,
            const std::string &entity,
            const int record_id,
            const bool thumbnail,
            const bool as_buffer) {
            delegate(shotgun_, atom, entity, record_id, thumbnail, as_buffer);
        },

        [=](shotgun_upload_atom atom,
            const std::string &entity,
            const int record_id,
            const std::string &field,
            const std::string &name,
            const std::vector<std::byte> &data,
            const std::string &content_type) {
            delegate(shotgun_, atom, entity, record_id, field, name, data, content_type);
        },

        // just use the default with jsonstore ?
        [=](put_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            put_action(rp, js);
            return rp;
        },

        [=](data_source::use_data_atom, const caf::actor &media, const FrameRate &media_rate)
            -> result<UuidActorVector> { return UuidActorVector(); },

        // no drop support..
        [=](data_source::use_data_atom, const JsonStore &, const FrameRate &, const bool)
            -> UuidActorVector { return UuidActorVector(); },

        // do we need the UI to have spun up before we can issue calls to shotgun...
        // erm...
        [=](use_data_atom atom, const caf::uri &uri) -> result<UuidActorVector> {
            auto rp      = make_response_promise<UuidActorVector>();
            use_action(rp, uri, FrameRate());
            return rp;
        },

        [=](use_data_atom,
            const caf::uri &uri,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            auto rp      = make_response_promise<UuidActorVector>();
            use_action(rp, uri, media_rate);
            return rp;
        },

        [=](use_data_atom,
            const utility::JsonStore &js,
            const caf::actor &session) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            use_action(rp, js, session);
            return rp;
        },

        // just use the default with jsonstore ?
        [=](use_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            use_action(rp, js);
            return rp;
        },

        // just use the default with jsonstore ?

        [=](post_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            post_action(rp, js);
            return rp;
        },

        [=](shotgun_entity_atom atom,
            const std::string &entity,
            const int record_id,
            const std::vector<std::string> &fields) {
            delegate(shotgun_, atom, entity, record_id, fields);
        },

        [=](shotgun_entity_filter_atom atom,
            const std::string &entity,
            const JsonStore &filter,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort) {
            delegate(shotgun_, atom, entity, filter, fields, sort);
        },

        [=](shotgun_entity_filter_atom atom,
            const std::string &entity,
            const JsonStore &filter,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort,
            const int page,
            const int page_size) {
            delegate(shotgun_, atom, entity, filter, fields, sort, page, page_size);
        },

        [=](shotgun_schema_entity_fields_atom atom,
            const std::string &entity,
            const std::string &field,
            const int id) { delegate(shotgun_, atom, entity, field, id); },

        [=](shotgun_entity_search_atom atom,
            const std::string &entity,
            const JsonStore &conditions,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort,
            const int page,
            const int page_size) {
            delegate(shotgun_, atom, entity, conditions, fields, sort, page, page_size);
        },

        [=](shotgun_text_search_atom atom,
            const std::string &text,
            const JsonStore &conditions,
            const int page,
            const int page_size) {
            delegate(shotgun_, atom, text, conditions, page, page_size);
        },

        // can't reply via qt mixin.. this is a work around..
        [=](shotgun_acquire_authentication_atom, const bool cancelled) {
            if (cancelled) {
                data_source_.set_authenticated(false);
                for (auto &i : waiting_)
                    i.deliver(
                        make_error(xstudio_error::error, "Authentication request cancelled."));
            } else {
                auto auth = data_source_.get_authentication();
                if (waiting_.empty()) {
                    anon_send(shotgun_, shotgun_authenticate_atom_v, auth);
                } else {
                    for (auto &i : waiting_)
                        i.deliver(auth);
                }
            }
            waiting_.clear();
        },

        [=](shotgun_acquire_authentication_atom atom,
            const std::string &message) -> result<shotgun_client::AuthenticateShotgun> {
            if (secret_source_ == actor_cast<caf::actor_addr>(this))
                return make_error(xstudio_error::error, "No authentication source.");

            auto rp = make_response_promise<shotgun_client::AuthenticateShotgun>();
            waiting_.push_back(rp);
            data_source_.set_authenticated(false);
            anon_send(actor_cast<caf::actor>(secret_source_), atom, message);
            return rp;
        },

        [=](utility::event_atom,
            shotgun_acquire_token_atom,
            const std::pair<std::string, std::string> &tokens) {
            auto prefs = GlobalStoreHelper(system());
            prefs.set_value(
                tokens.second,
                "/plugin/data_source/shotgun/authentication/refresh_token",
                false);
            prefs.save("APPLICATION");
            data_source_.set_authenticated(true);
        },

        [=](playlist::add_media_atom,
            const utility::JsonStore &data,
            const utility::Uuid &playlist_uuid,
            const caf::actor &playlist,
            const utility::Uuid &before) -> result<std::vector<UuidActor>> {
            auto rp = make_response_promise<std::vector<UuidActor>>();
            add_media_to_playlist(rp, data, playlist_uuid, playlist, before);
            return rp;
        },

        [=](playlist::add_media_atom) {
            // this message handler is called in a loop until all build media
            // tasks in the queue are exhausted

            bool is_ivy_build_task;

            auto build_media_task_data = get_next_build_task(is_ivy_build_task);
            while (build_media_task_data) {

                if (is_ivy_build_task) {

                    do_add_media_sources_from_ivy(build_media_task_data);

                } else {

                    do_add_media_sources_from_shotgun(build_media_task_data);
                }

                // N.B. we only get a new build task if the number of incomplete tasks
                // already dispatched is less than the number of actors in our
                // worker pool
                build_media_task_data = get_next_build_task(is_ivy_build_task);
            }
        },

        [=](get_data_atom, const utility::JsonStore &js) -> result<utility::JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            get_action(rp, js);
            return rp;
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &js) {
            try {
                update_preferences(js);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        });
}

template <typename T> void ShotgunDataSourceActor<T>::on_exit() {
    // maybe on timer.. ?
    for (auto &i : waiting_)
        i.deliver(make_error(xstudio_error::error, "Password request cancelled."));
    waiting_.clear();
    system().registry().erase(shotgun_datasource_registry);
}

template <typename T> void ShotgunDataSourceActor<T>::update_preferences(const JsonStore &js) {
    try {
        auto grant = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/grant_type");

        auto client_id = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/client_id");
        auto client_secret = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/client_secret");
        auto username = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/username");
        auto password = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/password");
        auto session_token = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/session_token");

        auto refresh_token = preference_value<std::string>(
            js, "/plugin/data_source/shotgun/authentication/refresh_token");

        auto host =
            preference_value<std::string>(js, "/plugin/data_source/shotgun/server/host");
        auto port = preference_value<int>(js, "/plugin/data_source/shotgun/server/port");
        auto protocol =
            preference_value<std::string>(js, "/plugin/data_source/shotgun/server/protocol");
        auto timeout = preference_value<int>(js, "/plugin/data_source/shotgun/server/timeout");


        auto cache_dir = expand_envvars(
            preference_value<std::string>(js, "/plugin/data_source/shotgun/download/path"));
        auto cache_size =
            preference_value<size_t>(js, "/plugin/data_source/shotgun/download/size");

        auto disable_integration =
            preference_value<bool>(js, "/plugin/data_source/shotgun/disable_integration");

        if(disable_integration_ != disable_integration) {
            disable_integration_ = disable_integration;
            if(disable_integration_)
                system().registry().erase(shotgun_datasource_registry);
            else
                system().registry().put(shotgun_datasource_registry, caf::actor_cast<caf::actor>(this));
        }

        download_cache_.prune_on_exit(true);
        download_cache_.target(cache_dir, true);
        download_cache_.max_size(cache_size * 1024 * 1024 * 1024);

        auto category = preference_value<JsonStore>(js, "/core/bookmark/category");
        category_colours_.clear();
        if (category.is_array()) {
            for (const auto &i : category) {
                category_colours_[i.value("value", "default")] = i.value("colour", "");
            }
        }

        // no op ?
        data_source_.set_authentication_method(grant);
        data_source_.set_client_id(client_id);
        data_source_.set_client_secret(client_secret);
        data_source_.set_username(expand_envvars(username));
        data_source_.set_password(password);
        data_source_.set_session_token(session_token);
        data_source_.set_timeout(timeout);

        // what hppens if we get a sequence of changes... should this be on a timed event ?
        // watch out for multiple instances.
        auto new_hash = std::hash<std::string>{}(
            grant + username + client_id + host + std::to_string(port) + protocol);

        if (new_hash != changed_hash_) {
            changed_hash_ = new_hash;
            // set server
            anon_send(
                shotgun_,
                shotgun_host_atom_v,
                std::string(fmt::format(
                    "{}://{}{}", protocol, host, (port ? ":" + std::to_string(port) : ""))));

            auto auth = data_source_.get_authentication();
            if (not refresh_token.empty())
                auth.set_refresh_token(refresh_token);

            anon_send(shotgun_, shotgun_credential_atom_v, auth);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::refresh_playlist_versions(
    caf::typed_response_promise<JsonStore> rp, const utility::Uuid &playlist_uuid) {
    // grab playlist id, get versions compare/load into playlist
    try {

        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);


        auto plsg = request_receive<JsonStore>(
            *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

        auto pl_id = plsg["id"].template get<int>();

        // this is a list of the media..
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);


        // foreach media actor get it's shogtun metadata.
        std::set<int> current_version_ids;

        for (const auto &i : media) {
            try {
                auto mjson = request_receive<JsonStore>(
                    *sys,
                    i.actor(),
                    json_store::get_json_atom_v,
                    utility::Uuid(),
                    ShotgunMetadataPath + "/version");
                current_version_ids.insert(mjson["id"].template get<int>());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }

        // we got media shotgun ids, plus playlist id
        // get current shotgun playlist/versions
        request(
            caf::actor_cast<caf::actor>(this),
            infinite,
            shotgun_entity_atom_v,
            "Playlists",
            pl_id,
            std::vector<std::string>())
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        scoped_actor sys{system()};
                        // update playlist
                        anon_send(
                            playlist,
                            json_store::set_json_atom_v,
                            JsonStore(result["data"]),
                            ShotgunMetadataPath + "/playlist");

                        //  gather versions, to get more detail..
                        std::vector<std::string> version_ids;
                        for (const auto &i :
                             result.at("data").at("relationships").at("versions").at("data")) {
                            if (not current_version_ids.count(i.at("id").template get<int>()))
                                version_ids.emplace_back(
                                    std::to_string(i.at("id").template get<int>()));
                        }

                        if (version_ids.empty()) {
                            rp.deliver(result);
                            return;
                        }

                        auto query  = R"({})"_json;
                        query["id"] = join_as_string(version_ids, ",");

                        // get details..
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            shotgun_entity_filter_atom_v,
                            "Versions",
                            JsonStore(query),
                            VersionFields,
                            std::vector<std::string>(),
                            1,
                            1000)
                            .then(
                                [=](const JsonStore &result2) mutable {
                                    try {
                                        // got version details.
                                        // we can now just call add versions to playlist..
                                        anon_send(
                                            caf::actor_cast<caf::actor>(this),
                                            playlist::add_media_atom_v,
                                            result2,
                                            playlist_uuid,
                                            playlist,
                                            utility::Uuid());

                                        // return this as the result.
                                        rp.deliver(result);

                                    } catch (const std::exception &err) {
                                        rp.deliver(
                                            make_error(xstudio_error::error, err.what()));
                                    }
                                },

                                [=](error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(err);
                                });
                    } catch (const std::exception &err) {
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });


    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::add_media_to_playlist(
    caf::typed_response_promise<UuidActorVector> rp,
    const utility::JsonStore &data,
    utility::Uuid playlist_uuid,
    caf::actor playlist,
    const utility::Uuid &before) {
    // data can be in multiple forms..

    auto sys = caf::scoped_actor(system());

    nlohmann::json versions;

    try {
        versions = data.at("data").at("relationships").at("versions").at("data");
    } catch (...) {
        try {
            versions = data.at("data");
        } catch (...) {
            try {
                versions = data.at("result").at("data");
            } catch (...) {
                return rp.deliver(make_error(xstudio_error::error, "Invalid JSON"));
            }
        }
    }

    if (versions.empty())
        return rp.deliver(std::vector<UuidActor>());

    auto event_msg = std::shared_ptr<event::Event>();


    // get uuid for playlist
    if (playlist and playlist_uuid.is_null()) {
        try {
            playlist_uuid =
                request_receive<utility::Uuid>(*sys, playlist, utility::uuid_atom_v);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            playlist = caf::actor();
        }
    }

    // get playlist for uuid
    if (not playlist and not playlist_uuid.is_null()) {
        try {
            auto session = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(global_registry),
                session::session_atom_v);

            playlist = request_receive<caf::actor>(
                *sys, session, session::get_playlist_atom_v, playlist_uuid);

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            playlist_uuid = utility::Uuid();
        }
    }

    // create playlist..
    if (not playlist and playlist_uuid.is_null()) {
        try {
            auto session = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(global_registry),
                session::session_atom_v);

            playlist_uuid = utility::Uuid::generate();
            playlist = spawn<playlist::PlaylistActor>("ShotGrid Media", playlist_uuid, session);
        } catch (const std::exception &err) {
            spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    if (not playlist_uuid.is_null()) {
        event_msg = std::make_shared<event::Event>(
            "Loading ShotGrid Playlist Media {}",
            0,
            0,
            versions.size(), // we increment progress once per version loaded - ivy leafs are
                             // added after progress hits 100%
            std::set<utility::Uuid>({playlist_uuid}));
        event::send_event(this, *event_msg);
    }

    try {
        auto media_rate =
            request_receive<FrameRate>(*sys, playlist, session::media_rate_atom_v);

        std::string flag_text, flag_colour;
        if (data.contains(json::json_pointer("/context/flag_text")) and not data.at("context").value("flag_text", "").empty() and
            not data.at("context").value("flag_colour", "").empty()) {
            flag_colour = data.at("context").value("flag_colour", "");
            flag_text   = data.at("context").value("flag_text", "");
        }

        std::string visual_source;
        if (data.contains(json::json_pointer("/context/visual_source"))) {
            visual_source = data.at("context").value("visual_source", "");
        }

        std::string audio_source;
        if (data.contains(json::json_pointer("/context/audio_source"))) {
            audio_source = data.at("context").value("audio_source", "");
        }

        // we need to ensure that media are added to playlist IN ORDER - this
        // is a bit fiddly because media are created out of order by the worker
        // pool so we use this utility::UuidList to ensure that the playlist builds
        // with media in order
        auto ordered_uuids = std::make_shared<utility::UuidList>();
        auto result        = std::make_shared<UuidActorVector>();
        auto result_count  = std::make_shared<int>(0);

        // get a new media item created for each of the names in our list
        for (const auto &i : versions) {

            std::string name(i.at("attributes").at("code"));

            // create a task data item, with the raw shotgun data that
            // can be used to build the media sources for each media
            // item in the playlist
            ordered_uuids->push_back(utility::Uuid::generate());
            build_playlist_media_tasks_.emplace_back(std::make_shared<BuildPlaylistMediaJob>(
                playlist,
                ordered_uuids->back(),
                name, // name for the media
                JsonStore(i),
                media_rate,
                visual_source,
                audio_source,
                event_msg,
                ordered_uuids,
                before,
                flag_colour,
                flag_text,
                rp,
                result,
                result_count));
        }

        // this call starts the work of building the media and consuming
        // the jobs in the 'build_playlist_media_tasks_' queue
        send(this, playlist::add_media_atom_v);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        if (not playlist_uuid.is_null()) {
            event_msg->set_complete();
            event::send_event(this, *event_msg);
        }
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::load_playlist(
    caf::typed_response_promise<UuidActor> rp,
    const int playlist_id,
    const caf::actor &session) {

    // this is going to get nesty :()

    // get playlist from shotgun
    request(
        caf::actor_cast<caf::actor>(this),
        infinite,
        shotgun_entity_atom_v,
        "Playlists",
        playlist_id,
        std::vector<std::string>())
        .then(
            [=](JsonStore pljs) mutable {
                // got playlist.
                // we can create an new xstudio playlist actor at this point..
                auto playlist = UuidActor();
                try {
                    if (session) {
                        scoped_actor sys{system()};

                        auto tmp = request_receive<std::pair<utility::Uuid, UuidActor>>(
                            *sys,
                            session,
                            session::add_playlist_atom_v,
                            pljs["data"]["attributes"]["code"].get<std::string>(),
                            utility::Uuid(),
                            false);

                        playlist = tmp.second;

                    } else {
                        auto uuid = utility::Uuid::generate();
                        auto tmp  = spawn<playlist::PlaylistActor>(
                            pljs["data"]["attributes"]["code"].get<std::string>(), uuid);
                        playlist = UuidActor(uuid, tmp);
                    }

                    // place holder for shotgun decorators.
                    anon_send(
                        playlist.actor(),
                        json_store::set_json_atom_v,
                        JsonStore(),
                        "/metadata/shotgun");
                    // should really be driven from back end not UI..
                } catch (const std::exception &err) {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }

                // get version order
                auto order_filter = R"(
            {
                "logical_operator": "and",
                "conditions": [
                    ["playlist", "is", {"type":"Playlist", "id":0}]
                ]
            })"_json;

                order_filter["conditions"][0][2]["id"] = playlist_id;

                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    shotgun_entity_search_atom_v,
                    "PlaylistVersionConnection",
                    JsonStore(order_filter),
                    std::vector<std::string>({"sg_sort_order", "version"}),
                    std::vector<std::string>({"sg_sort_order"}),
                    1,
                    4999)
                    .then(
                        [=](const JsonStore &order) mutable {
                            std::vector<std::string> version_ids;
                            for (const auto &i : order["data"])
                                version_ids.emplace_back(std::to_string(
                                    i["relationships"]["version"]["data"].at("id").get<int>()));

                            if (version_ids.empty())
                                return rp.deliver(
                                    make_error(xstudio_error::error, "No Versions found"));

                            // get versions
                            auto query  = R"({})"_json;
                            query["id"] = join_as_string(version_ids, ",");

                            // get versions ordered by playlist.
                            request(
                                caf::actor_cast<caf::actor>(this),
                                infinite,
                                shotgun_entity_filter_atom_v,
                                "Versions",
                                JsonStore(query),
                                VersionFields,
                                std::vector<std::string>(),
                                1,
                                4999)
                                .then(
                                    [=](JsonStore &js) mutable {
                                        // munge it..
                                        auto data = R"([])"_json;

                                        for (const auto &i : version_ids) {
                                            for (auto &j : js["data"]) {

                                                // spdlog::warn("{} {}",
                                                // std::to_string(j["id"].get<int>()), i);
                                                if (std::to_string(j["id"].get<int>()) == i) {
                                                    data.push_back(j);
                                                    break;
                                                }
                                            }
                                        }

                                        js["data"] = data;

                                        // add back in
                                        pljs["data"]["relationships"]["versions"] = js;

                                        // spdlog::warn("{}",pljs.dump(2));
                                        // now we have a playlist json struct with the versions
                                        // corrrecly ordered, set metadata on playlist..
                                        anon_send(
                                            playlist.actor(),
                                            json_store::set_json_atom_v,
                                            JsonStore(pljs["data"]),
                                            ShotgunMetadataPath + "/playlist");

                                        // addDecorator(playlist.uuid)
                                        // addMenusFull(playlist.uuid)

                                        anon_send(
                                            caf::actor_cast<caf::actor>(this),
                                            playlist::add_media_atom_v,
                                            pljs,
                                            playlist.uuid(),
                                            playlist.actor(),
                                            utility::Uuid());

                                        rp.deliver(playlist);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::error(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        rp.deliver(
                                            make_error(xstudio_error::error, to_string(err)));
                                    });
                        },
                        [=](error &err) mutable {
                            spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(make_error(xstudio_error::error, to_string(err)));
                        });
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(make_error(xstudio_error::error, to_string(err)));
            });
}

template <typename T>
std::shared_ptr<BuildPlaylistMediaJob>
ShotgunDataSourceActor<T>::get_next_build_task(bool &is_ivy_build_task) {

    std::shared_ptr<BuildPlaylistMediaJob> job_info;
    // if we already have popped N jobs off the queue that haven't completed
    // and N >= worker_count_ we don't pop a job off and instead return a null
    //
    if (build_tasks_in_flight_ < worker_count_) {
        if (!build_playlist_media_tasks_.empty()) {
            is_ivy_build_task = false;
            job_info          = build_playlist_media_tasks_.front();
            build_playlist_media_tasks_.pop_front();
        } else if (!extend_media_with_ivy_tasks_.empty()) {
            is_ivy_build_task = true;
            job_info          = extend_media_with_ivy_tasks_.front();
            extend_media_with_ivy_tasks_.pop_front();
        }
    }
    return job_info;
}

template <typename T>
void ShotgunDataSourceActor<T>::do_add_media_sources_from_shotgun(
    std::shared_ptr<BuildPlaylistMediaJob> build_media_task_data) {

    // now 'build' the MediaActor via our worker pool to create
    // MediaSources and add them
    build_tasks_in_flight_++;

    // spawn a media actor
    build_media_task_data->media_actor_ = spawn<media::MediaActor>(
        build_media_task_data->media_name_,
        build_media_task_data->media_uuid_,
        UuidActorVector());
    UuidActor ua(build_media_task_data->media_uuid_, build_media_task_data->media_actor_);

    // this is called when we get a result back - keeps track of the number
    // of jobs being processed and sends a message to self to continue working
    // through the queue
    auto continue_processing_job_queue = [=]() {
        build_tasks_in_flight_--;
        delayed_send(this, JOB_DISPATCH_DELAY, playlist::add_media_atom_v);
        if (build_media_task_data->event_msg_) {
            build_media_task_data->event_msg_->increment_progress();
            event::send_event(this, *(build_media_task_data->event_msg_));
        }
    };

    // now we get our worker pool to build media sources and add them to the
    // parent MediaActor using the shotgun query data
    request(
        pool_,
        caf::infinite,
        playlist::add_media_atom_v,
        build_media_task_data->media_actor_,
        build_media_task_data->sg_data_,
        build_media_task_data->media_rate_)
        .then(

            [=](bool) {
                // media sources were constructed successfully - now we can add to
                // the playlist, we pass in the overall ordered list of uuids that
                // we are building so the playlist can ensure everything is added
                // in order, even if they aren't created in the correct order
                request(
                    build_media_task_data->playlist_actor_,
                    caf::infinite,
                    playlist::add_media_atom_v,
                    ua,
                    *(build_media_task_data->ordererd_uuids_),
                    build_media_task_data->before_)
                    .then(

                        [=](const UuidActor &) {
                            if (!build_media_task_data->flag_colour_.empty()) {
                                anon_send(
                                    build_media_task_data->media_actor_,
                                    playlist::reflag_container_atom_v,
                                    std::make_tuple(
                                        std::optional<std::string>(
                                            build_media_task_data->flag_colour_),
                                        std::optional<std::string>(
                                            build_media_task_data->flag_text_)));
                            }

                            extend_media_with_ivy_tasks_.emplace_back(build_media_task_data);
                            continue_processing_job_queue();
                        },
                        [=](const caf::error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            continue_processing_job_queue();
                        });
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                continue_processing_job_queue();
            });
}

template <typename T>
void ShotgunDataSourceActor<T>::do_add_media_sources_from_ivy(
    std::shared_ptr<BuildPlaylistMediaJob> ivy_media_task_data) {

    auto ivy = system().registry().template get<caf::actor>("IVYDATASOURCE");
    build_tasks_in_flight_++;

    // this is called when we get a result back - keeps track of the number
    // of jobs being processed and sends a message to self to continue working
    // through the queue
    auto continue_processing_job_queue = [=]() {
        build_tasks_in_flight_--;
        delayed_send(this, JOB_DISPATCH_DELAY, playlist::add_media_atom_v);
        /* Commented out bevause we're not including ivy leaf addition
        in progress indicator now.
        if (ivy_media_task_data->event_msg) {
            ivy_media_task_data->event_msg->increment_progress();
            event::send_event(this, *(ivy_media_task_data->event_msg));
        }*/
    };


    auto good_sources = std::make_shared<utility::UuidActorVector>();
    auto count        = std::make_shared<int>(0);

    // this function adds the sources that are 'good' (i.e. were able
    // to acquire MediaDetail) to the MediaActor - we only call it
    // when we've fully 'built' each MediaSourceActor in our 'sources'
    // list -0 see the request/then handler below where it is used
    auto finalise = [=]() {
        request(
            ivy_media_task_data->media_actor_,
            infinite,
            media::add_media_source_atom_v,
            *good_sources)
            .then(
                [=](const bool) {
                    // media sources all in media actor.
                    // we can now select the ones we want..
                    anon_send(
                        ivy_media_task_data->media_actor_,
                        playhead::media_source_atom_v,
                        ivy_media_task_data->preferred_visual_source_,
                        media::MT_IMAGE,
                        true);

                    anon_send(
                        ivy_media_task_data->media_actor_,
                        playhead::media_source_atom_v,
                        ivy_media_task_data->preferred_audio_source_,
                        media::MT_AUDIO,
                        true);

                    continue_processing_job_queue();
                },
                [=](error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    continue_processing_job_queue();
                });
    };

    // here we get the ivy data source to fetch sources (ivy leafs) using the
    // ivy dnuuid for the MediaActor already created from shotgun data
    try {
        request(
            ivy,
            infinite,
            use_data_atom_v,
            ivy_media_task_data->sg_data_.at("attributes")
                .at("sg_project_name")
                .get<std::string>(),
            utility::Uuid(ivy_media_task_data->sg_data_.at("attributes")
                              .at("sg_ivy_dnuuid")
                              .get<std::string>()),
            ivy_media_task_data->media_rate_)
            .then(
                [=](const utility::UuidActorVector &sources) {
                    // we want to make sure the 'MediaDetail' has been fetched on the
                    // sources before adding to the parent MediaActor - this means we
                    // don't build up a massive queue of IO heavy MediaDetail fetches
                    // but instead deal with them sequentially as each media item is
                    // added to the playlist

                    if (sources.empty()) {
                        finalise();
                    } else {
                        *count = sources.size();
                    }

                    for (auto source : sources) {

                        // we need to get each source to get its detail to ensure that
                        // it is readable/valid
                        request(
                            source.actor(),
                            infinite,
                            media::acquire_media_detail_atom_v,
                            ivy_media_task_data->media_rate_)
                            .then(
                                [=](bool got_media_detail) mutable {
                                    if (got_media_detail)
                                        good_sources->push_back(source);
                                    else
                                        send_exit(
                                            source.actor(), caf::exit_reason::user_shutdown);

                                    (*count)--;
                                    if (!(*count))
                                        finalise();
                                },
                                [=](error &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));

                                    // kill bad source.
                                    send_exit(source.actor(), caf::exit_reason::user_shutdown);

                                    (*count)--;
                                    if (!(*count))
                                        finalise();
                                });
                    }
                },

                [=](error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    continue_processing_job_queue();
                });
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        continue_processing_job_queue();
    }
}

