// SPDX-License-Identifier: Apache-2.0

#include <fmt/format.h>
#include <caf/policy/select_all.hpp>

#include "data_source_shotgun.hpp"
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

class BuildPlaylistMediaJob {

  public:
    BuildPlaylistMediaJob(
        caf::actor playlist_actor,
        const utility::Uuid &media_uuid,
        const std::string media_name,
        utility::JsonStore sg_data,
        utility::FrameRate media_rate,
        std::string preferred_visual_source,
        std::string preferred_audio_source,
        std::shared_ptr<event::Event> event,
        std::shared_ptr<utility::UuidList> ordererd_uuids,
        utility::Uuid before,
        std::string flag_colour,
        std::string flag_text,
        caf::typed_response_promise<UuidActorVector> response_promise,
        std::shared_ptr<UuidActorVector> result,
        std::shared_ptr<int> result_count)
        : playlist_actor_(std::move(playlist_actor)),
          media_uuid_(media_uuid),
          media_name_(media_name),
          sg_data_(sg_data),
          media_rate_(media_rate),
          preferred_visual_source_(std::move(preferred_visual_source)),
          preferred_audio_source_(std::move(preferred_audio_source)),
          event_msg_(std::move(event)),
          ordererd_uuids_(std::move(ordererd_uuids)),
          before_(std::move(before)),
          flag_colour_(std::move(flag_colour)),
          flag_text_(std::move(flag_text)),
          response_promise_(std::move(response_promise)),
          result_(std::move(result)),
          result_count_(result_count) {
        // increment a shared counter - the counter is shared between
        // all the indiviaual Media creation jobs in a single build playlist
        // task
        (*result_count)++;
    }

    BuildPlaylistMediaJob(const BuildPlaylistMediaJob &o) = default;
    BuildPlaylistMediaJob()                               = default;

    ~BuildPlaylistMediaJob() {
        // this gets destroyed when the job is done with.
        if (media_actor_) {
            result_->push_back(UuidActor(media_uuid_, media_actor_));
        }
        // decrement the counter
        (*result_count_)--;

        if (!(*result_count_)) {
            // counter has dropped to zero, all jobs within a single build playlist
            // tas are done. Our 'result' member here is in the order that the
            // media items were created (asynchronously), rather than the order
            // of the final playlist ... so we need to reorder our 'result' to
            // match the ordering in the playlist
            UuidActorVector reordered;
            reordered.reserve(result_->size());
            for (const auto &uuid : (*ordererd_uuids_)) {
                for (auto uai = result_->begin(); uai != result_->end(); uai++) {
                    if ((*uai).uuid() == uuid) {
                        reordered.push_back(*uai);
                        result_->erase(uai);
                        break;
                    }
                }
            }
            response_promise_.deliver(reordered);
        }
    }

    caf::actor playlist_actor_;
    utility::Uuid media_uuid_;
    std::string media_name_;
    utility::JsonStore sg_data_;
    utility::FrameRate media_rate_;
    std::string preferred_visual_source_;
    std::string preferred_audio_source_;
    std::shared_ptr<event::Event> event_msg_;
    std::shared_ptr<utility::UuidList> ordererd_uuids_;
    utility::Uuid before_;
    std::string flag_colour_;
    std::string flag_text_;
    caf::typed_response_promise<UuidActorVector> response_promise_;
    std::shared_ptr<UuidActorVector> result_;
    std::shared_ptr<int> result_count_;
    caf::actor media_actor_;
};

class ShotgunMediaWorker : public caf::event_based_actor {
  public:
    ShotgunMediaWorker(caf::actor_config &cfg, const caf::actor_addr source);
    ~ShotgunMediaWorker() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ShotgunMediaWorker";
    caf::behavior make_behavior() override { return behavior_; }

    void add_media_step_1(
        caf::typed_response_promise<bool> rp,
        caf::actor media,
        const JsonStore &jsn,
        const FrameRate &media_rate);
    void add_media_step_2(
        caf::typed_response_promise<bool> rp,
        caf::actor media,
        const JsonStore &jsn,
        const FrameRate &media_rate,
        const UuidActor &movie_source);
    void add_media_step_3(
        caf::typed_response_promise<bool> rp,
        caf::actor media,
        const JsonStore &jsn,
        const UuidActorVector &srcs);

  private:
    caf::behavior behavior_;
    caf::actor_addr data_source_;
};

void ShotgunMediaWorker::add_media_step_1(
    caf::typed_response_promise<bool> rp,
    caf::actor media,
    const JsonStore &jsn,
    const FrameRate &media_rate) {
    request(
        actor_cast<caf::actor>(this),
        infinite,
        media::add_media_source_atom_v,
        jsn,
        media_rate,
        true)
        .then(
            [=](const UuidActor &movie_source) mutable {
                add_media_step_2(rp, media, jsn, media_rate, movie_source);
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}

void ShotgunMediaWorker::add_media_step_2(
    caf::typed_response_promise<bool> rp,
    caf::actor media,
    const JsonStore &jsn,
    const FrameRate &media_rate,
    const UuidActor &movie_source) {
    // now get image..
    request(
        actor_cast<caf::actor>(this), infinite, media::add_media_source_atom_v, jsn, media_rate)
        .then(
            [=](const UuidActor &image_source) mutable {
                // check to see if what we've got..
                // failed...
                if (movie_source.uuid().is_null() and image_source.uuid().is_null()) {
                    spdlog::warn("{} No valid sources {}", __PRETTY_FUNCTION__, jsn.dump(2));
                    rp.deliver(false);
                } else {
                    try {
                        UuidActorVector srcs;

                        if (not movie_source.uuid().is_null())
                            srcs.push_back(movie_source);
                        if (not image_source.uuid().is_null())
                            srcs.push_back(image_source);


                        add_media_step_3(rp, media, jsn, srcs);

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}

void ShotgunMediaWorker::add_media_step_3(
    caf::typed_response_promise<bool> rp,
    caf::actor media,
    const JsonStore &jsn,
    const UuidActorVector &srcs) {
    request(media, infinite, media::add_media_source_atom_v, srcs)
        .then(
            [=](const bool) mutable {
                rp.deliver(true);
                // push metadata to media actor.
                anon_send(
                    media,
                    json_store::set_json_atom_v,
                    utility::Uuid(),
                    jsn,
                    ShotgunMetadataPath + "/version");

                // dispatch delayed shot data.
                try {
                    auto shotreq = JsonStore(GetShotFromIdJSON);
                    shotreq["shot_id"] =
                        jsn.at("relationships").at("entity").at("data").value("id", 0);

                    request(
                        caf::actor_cast<caf::actor>(data_source_),
                        infinite,
                        get_data_atom_v,
                        shotreq)
                        .then(
                            [=](const JsonStore &jsn) mutable {
                                try {
                                    if (jsn.count("data"))
                                        anon_send(
                                            media,
                                            json_store::set_json_atom_v,
                                            utility::Uuid(),
                                            JsonStore(jsn.at("data")),
                                            ShotgunMetadataPath + "/shot");
                                } catch (const std::exception &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                }
                            },
                            [=](const error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },
            [=](error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); });
}


ShotgunMediaWorker::ShotgunMediaWorker(caf::actor_config &cfg, const caf::actor_addr source)
    : data_source_(std::move(source)), caf::event_based_actor(cfg) {

    // for each input we spawn one media item with upto two media sources.


    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        // movie
        [=](media::add_media_source_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate,
            const bool /*movie*/) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            try {
                if (not jsn.at("attributes").at("sg_path_to_movie").is_null()) {
                    // spdlog::info("{}", i["attributes"]["sg_path_to_movie"]);
                    // prescan movie for duration..
                    // it may contain slate, which needs trimming..
                    // SLOW do we want to be offsetting the movie ?
                    // if we keep this code is needs threading..
                    auto uri = posix_path_to_uri(jsn["attributes"]["sg_path_to_movie"]);
                    const auto source_uuid = Uuid::generate();
                    auto source            = spawn<media::MediaSourceActor>(
                        "SG Movie", uri, media_rate, source_uuid);

                    request(source, infinite, media::acquire_media_detail_atom_v, media_rate)
                        .then(
                            [=](bool) mutable { rp.deliver(UuidActor(source_uuid, source)); },
                            [=](error &err) mutable {
                                // even though there is an error, we want the broken media
                                // source added so the user can see it in the UI (and its error
                                // state)
                                rp.deliver(UuidActor(source_uuid, source));
                            });

                } else {
                    rp.deliver(UuidActor());
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                rp.deliver(UuidActor());
            }
            return rp;
        },

        // frames
        [=](media::add_media_source_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            try {
                if (not jsn.at("attributes").at("sg_path_to_frames").is_null()) {
                    FrameList frame_list;
                    caf::uri uri;

                    if (jsn.at("attributes").at("frame_range").is_null()) {
                        // no frame range specified..
                        // try and aquire from filesystem..
                        uri = parse_cli_posix_path(
                            jsn.at("attributes").at("sg_path_to_frames"), frame_list, true);
                    } else {
                        frame_list = FrameList(
                            jsn.at("attributes").at("frame_range").template get<std::string>());
                        uri = parse_cli_posix_path(
                            jsn.at("attributes").at("sg_path_to_frames"), frame_list, false);
                    }

                    const auto source_uuid = Uuid::generate();
                    auto source =
                        frame_list.empty()
                            ? spawn<media::MediaSourceActor>(
                                  "SG Frames", uri, media_rate, source_uuid)
                            : spawn<media::MediaSourceActor>(
                                  "SG Frames", uri, frame_list, media_rate, source_uuid);

                    request(source, infinite, media::acquire_media_detail_atom_v, media_rate)
                        .then(
                            [=](bool) mutable { rp.deliver(UuidActor(source_uuid, source)); },
                            [=](error &err) mutable {
                                // even though there is an error, we want the broken media
                                // source added so the user can see it in the UI (and its error
                                // state)
                                rp.deliver(UuidActor(source_uuid, source));
                            });
                } else {
                    rp.deliver(UuidActor());
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                rp.deliver(UuidActor());
            }

            return rp;
        },

        [=](playlist::add_media_atom,
            caf::actor media,
            JsonStore jsn,
            const FrameRate &media_rate) -> result<bool> {
            auto rp = make_response_promise<bool>();

            try {
                // do stupid stuff, because data integrity is for losers.
                // if we've got a movie in the sg_frames property, swap them over.
                if (jsn.at("attributes").at("sg_path_to_movie").is_null() and
                    not jsn.at("attributes").at("sg_path_to_frames").is_null() and
                    jsn.at("attributes")
                            .at("sg_path_to_frames")
                            .template get<std::string>()
                            .find_first_of('#') == std::string::npos) {
                    // movie in image sequence..
                    jsn["attributes"]["sg_path_to_movie"] =
                        jsn.at("attributes").at("sg_path_to_frames");
                    jsn["attributes"]["sg_path_to_frames"] = nullptr;
                }

                // request movie .. THESE MUST NOT RETURN error on fail.
                add_media_step_1(rp, media, jsn, media_rate);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                rp.deliver(make_error(xstudio_error::error, err.what()));
            }
            return rp;
        });
}


void ShotgunDataSource::set_authentication_method(const std::string &value) {
    if (authentication_method_->value() != value)
        authentication_method_->set_value(value);
}
void ShotgunDataSource::set_client_id(const std::string &value) {
    if (client_id_->value() != value)
        client_id_->set_value(value);
}
void ShotgunDataSource::set_client_secret(const std::string &value) {
    if (client_secret_->value() != value)
        client_secret_->set_value(value);
}
void ShotgunDataSource::set_username(const std::string &value) {
    if (username_->value() != value)
        username_->set_value(value);
}
void ShotgunDataSource::set_password(const std::string &value) {
    if (password_->value() != value)
        password_->set_value(value);
}
void ShotgunDataSource::set_session_token(const std::string &value) {
    if (session_token_->value() != value)
        session_token_->set_value(value);
}
void ShotgunDataSource::set_authenticated(const bool value) {
    if (authenticated_->value() != value)
        authenticated_->set_value(value);
}
void ShotgunDataSource::set_timeout(const int value) {
    if (timeout_->value() != value)
        timeout_->set_value(value);
}

shotgun_client::AuthenticateShotgun ShotgunDataSource::get_authentication() const {
    AuthenticateShotgun auth;

    auth.set_session_uuid(to_string(session_id_));

    auth.set_authentication_method(authentication_method_->value());
    switch (*(auth.authentication_method())) {
    case AM_SCRIPT:
        auth.set_client_id(client_id_->value());
        auth.set_client_secret(client_secret_->value());
        break;
    case AM_SESSION:
        auth.set_session_token(session_token_->value());
        break;
    case AM_LOGIN:
        auth.set_username(expand_envvars(username_->value()));
        auth.set_password(password_->value());
        break;
    case AM_UNDEFINED:
    default:
        break;
    }

    return auth;
}

void ShotgunDataSource::add_attributes() {

    std::vector<std::string> auth_method_names = {
        "client_credentials", "password", "session_token"};

    module::QmlCodeAttribute *button = add_qml_code_attribute(
        "MyCode",
        R"(
import Shotgun 1.0
ShotgunButton {}
)");

    button->set_role_data(module::Attribute::ToolbarPosition, 1010.0);
    button->expose_in_ui_attrs_group("media_tools_buttons");


    authentication_method_ = add_string_choice_attribute(
        "authentication_method",
        "authentication_method",
        "password",
        auth_method_names,
        auth_method_names);

    playlist_notes_action_ =
        add_action_attribute("playlist_notes_to_shotgun", "playlist_notes_to_shotgun");
    selected_notes_action_ =
        add_action_attribute("selected_notes_to_shotgun", "selected_notes_to_shotgun");

    client_id_     = add_string_attribute("client_id", "client_id", "");
    client_secret_ = add_string_attribute("client_secret", "client_secret", "");
    username_      = add_string_attribute("username", "username", "");
    password_      = add_string_attribute("password", "password", "");
    session_token_ = add_string_attribute("session_token", "session_token", "");

    authenticated_ = add_boolean_attribute("authenticated", "authenticated", false);

    // should be int..
    timeout_ = add_float_attribute("timeout", "timeout", 120.0, 10.0, 600.0, 1.0, 0);


    // by setting static UUIDs on these module we only create them once in the UI
    playlist_notes_action_->set_role_data(
        module::Attribute::UuidRole, "92c780be-d0bc-462a-b09f-643e8986e2a1");
    playlist_notes_action_->set_role_data(
        module::Attribute::Title, "Publish Playlist Notes...");
    playlist_notes_action_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_menu"});
    playlist_notes_action_->set_role_data(
        module::Attribute::MenuPaths, std::vector<std::string>({"publish_menu|Shotgun"}));

    selected_notes_action_->set_role_data(
        module::Attribute::UuidRole, "7583a4d0-35d8-4f00-bc32-ae8c2bddc30a");
    selected_notes_action_->set_role_data(
        module::Attribute::Title, "Publish Selected Notes...");
    selected_notes_action_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_menu"});
    selected_notes_action_->set_role_data(
        module::Attribute::MenuPaths, std::vector<std::string>({"publish_menu|Shotgun"}));

    authentication_method_->set_role_data(
        module::Attribute::UuidRole, "ea7c47b8-a851-4f44-b9f1-3f5b38c11d96");
    client_id_->set_role_data(
        module::Attribute::UuidRole, "31925e29-674f-4f03-a861-502a2bc92f78");
    client_secret_->set_role_data(
        module::Attribute::UuidRole, "05d18793-ef4c-4753-8b55-1d98788eb727");
    username_->set_role_data(
        module::Attribute::UuidRole, "a012c508-a8a7-4438-97ff-05fc707331d0");
    password_->set_role_data(
        module::Attribute::UuidRole, "55982b32-3273-4f1c-8164-251d8af83365");
    session_token_->set_role_data(
        module::Attribute::UuidRole, "d6fac6a6-a6c9-4ac3-b961-499d9862a886");
    authenticated_->set_role_data(
        module::Attribute::UuidRole, "ce708287-222f-46b6-820c-f6dfda592ba9");
    timeout_->set_role_data(
        module::Attribute::UuidRole, "9947a178-b5bb-4370-905e-c6687b2d7f41");

    authentication_method_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    client_id_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    client_secret_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    username_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    password_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    session_token_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    authenticated_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    timeout_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});

    authentication_method_->set_role_data(
        module::Attribute::ToolTip, "Shotgun authentication method.");

    client_id_->set_role_data(module::Attribute::ToolTip, "Shotgun script key.");
    client_secret_->set_role_data(module::Attribute::ToolTip, "Shotgun script secret.");
    username_->set_role_data(module::Attribute::ToolTip, "Shotgun username.");
    password_->set_role_data(module::Attribute::ToolTip, "Shotgun password.");
    session_token_->set_role_data(module::Attribute::ToolTip, "Shotgun session token.");
    authenticated_->set_role_data(module::Attribute::ToolTip, "Authenticated.");
    timeout_->set_role_data(module::Attribute::ToolTip, "Shotgun server timeout.");
}

void ShotgunDataSource::attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) {
    // pass upto actor..
    call_attribute_changed(attr_uuid);
}

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

    system().registry().put(shotgun_datasource_registry, caf::actor_cast<caf::actor>(this));

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        update_preferences(j);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

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

        [=](data_source::use_data_atom, const caf::actor &media, const FrameRate &media_rate)
            -> result<UuidActorVector> { return UuidActorVector(); },

        // no drop support..
        [=](data_source::use_data_atom, const JsonStore &, const FrameRate &, const bool)
            -> UuidActorVector { return UuidActorVector(); },

        [=](data_source::use_data_atom,
            const std::string &project,
            const utility::Uuid &stalk_dnuuid) {
            auto jsre        = JsonStore(GetVersionIvyUuidJSON);
            jsre["ivy_uuid"] = to_string(stalk_dnuuid);
            jsre["job"]      = project;
            delegate(caf::actor_cast<caf::actor>(this), get_data_atom_v, jsre);
        },

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
            try {
                if (js["entity"] == "Playlist" and js["relationship"] == "Version") {
                    auto rp = make_response_promise<JsonStore>();
                    update_playlist_versions(rp, Uuid(js["playlist_uuid"]));
                    return rp;
                }
            } catch (const std::exception &err) {
                return make_error(
                    xstudio_error::error, std::string("Invalid operation.\n") + err.what());
            }

            return make_error(xstudio_error::error, "Invalid operation.");
        },

        // do we need the UI to have spun up before we can issue calls to shotgun...
        // erm...
        [=](use_data_atom atom, const caf::uri &uri) {
            delegate(actor_cast<caf::actor>(this), atom, uri, FrameRate());
        },
        [=](use_data_atom,
            const caf::uri &uri,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            // check protocol == shotgun..
            if (uri.scheme() != "shotgun")
                return UuidActorVector();

            if (to_string(uri.authority()) == "load") {
                // need type and id
                auto query = uri.query();
                if (query.count("type") and query["type"] == "Version" and query.count("ids")) {
                    auto ids = split(query["ids"], '|');
                    if (ids.empty())
                        return UuidActorVector();

                    auto count   = std::make_shared<int>(ids.size());
                    auto rp      = make_response_promise<UuidActorVector>();
                    auto results = std::make_shared<UuidActorVector>();

                    for (const auto i : ids) {
                        try {
                            auto type    = query["type"];
                            auto squery  = R"({})"_json;
                            squery["id"] = i;

                            request(
                                caf::actor_cast<caf::actor>(this),
                                std::chrono::seconds(
                                    static_cast<int>(data_source_.timeout_->value())),
                                shotgun_entity_filter_atom_v,
                                "Versions",
                                JsonStore(squery),
                                VersionFields,
                                std::vector<std::string>(),
                                1,
                                4999)
                                .then(
                                    [=](const JsonStore &js) mutable {
                                        // load version..
                                        request(
                                            caf::actor_cast<caf::actor>(this),
                                            infinite,
                                            playlist::add_media_atom_v,
                                            js,
                                            utility::Uuid(),
                                            caf::actor(),
                                            utility::Uuid())
                                            .then(
                                                [=](const UuidActorVector &uav) mutable {
                                                    (*count)--;

                                                    for (const auto &ua : uav)
                                                        results->push_back(ua);

                                                    if (not(*count))
                                                        rp.deliver(*results);
                                                },
                                                [=](const caf::error &err) mutable {
                                                    (*count)--;
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                    if (not(*count))
                                                        rp.deliver(*results);
                                                });
                                    },
                                    [=](const caf::error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        } catch (const std::exception &err) {
                            (*count)--;
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                    return rp;
                } else if (
                    query.count("type") and query["type"] == "Playlist" and
                    query.count("ids")) {
                    // will return an array of playlist actors..
                    auto ids = split(query["ids"], '|');
                    if (ids.empty())
                        return UuidActorVector();

                    auto rp      = make_response_promise<UuidActorVector>();
                    auto count   = std::make_shared<int>(ids.size());
                    auto results = std::make_shared<UuidActorVector>();

                    for (const auto i : ids) {
                        auto id           = std::atoi(i.c_str());
                        auto js           = JsonStore(LoadPlaylistJSON);
                        js["playlist_id"] = id;
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            use_data_atom_v,
                            js,
                            caf::actor())
                            .then(
                                [=](const UuidActor &ua) mutable {
                                    // process result to build playlist..
                                    (*count)--;
                                    results->push_back(ua);
                                    if (not(*count))
                                        rp.deliver(*results);
                                },
                                [=](const caf::error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    (*count)--;
                                    if (not(*count))
                                        rp.deliver(*results);
                                });
                    }
                } else {
                    spdlog::warn(
                        "Invalid shotgun action {}, requires type, id", to_string(uri));
                }
            } else {
                spdlog::warn(
                    "Invalid shotgun action {} {}", to_string(uri.authority()), to_string(uri));
            }

            return UuidActorVector();
        },

        [=](use_data_atom,
            const utility::JsonStore &js,
            const caf::actor &session) -> result<UuidActor> {
            try {
                if (js.at("entity") == "Playlist" and js.count("playlist_id")) {
                    auto rp = make_response_promise<UuidActor>();
                    load_playlist(rp, js.at("playlist_id").get<int>(), session);
                    return rp;
                }
            } catch (const std::exception &err) {
                return make_error(
                    xstudio_error::error, std::string("Invalid operation.\n") + err.what());
            }
            return make_error(xstudio_error::error, "Invalid operation.");
        },

        // just use the default with jsonstore ?
        [=](use_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
            try {
                if (js.at("entity") == "Playlist" and js.count("playlist_id")) {
                    scoped_actor sys{system()};
                    auto session = request_receive<caf::actor>(
                        *sys,
                        system().registry().template get<caf::actor>(global_registry),
                        session::session_atom_v);

                    auto rp = make_response_promise<JsonStore>();
                    request(
                        caf::actor_cast<caf::actor>(this),
                        infinite,
                        use_data_atom_v,
                        js,
                        session)
                        .then(
                            [=](const UuidActor &) mutable {
                                rp.deliver(
                                    JsonStore(R"({"data": {"status": "successful"}})"_json));
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(
                                    JsonStore(R"({"data": {"status": "successful"}})"_json));
                            });

                    return rp;
                } else if (
                    js.at("entity") == "Playlist" and js.at("relationship") == "Version") {
                    auto rp = make_response_promise<JsonStore>();
                    refresh_playlist_versions(rp, Uuid(js.at("playlist_uuid")));
                    return rp;
                }
            } catch (const std::exception &err) {
                return make_error(
                    xstudio_error::error, std::string("Invalid operation.\n") + err.what());
            }
            return make_error(xstudio_error::error, "Invalid operation.");
        },

        // just use the default with jsonstore ?

        [=](post_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
            try {
                if (js["entity"] == "Note") {
                    auto rp = make_response_promise<JsonStore>();
                    create_playlist_notes(rp, js["payload"], JsonStore(js["playlist_uuid"]));
                    return rp;
                }
                if (js["entity"] == "Playlist") {
                    auto rp = make_response_promise<JsonStore>();
                    create_playlist(rp, js);
                    return rp;
                }
            } catch (const std::exception &err) {
                return make_error(
                    xstudio_error::error, std::string("Invalid operation.\n") + err.what());
            }

            return make_error(xstudio_error::error, "Invalid operation.");
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

        /*[=](playlist::add_media_atom,
            caf::actor playlist,
            JsonStore versions_to_add,
            const utility::Uuid job_uuid,
            const utility::Uuid before,
            const FrameRate media_rate,
            const bool only_movies) {

            if (versions_to_add.empty()) {
                // versions_to_add is now empty - deliver on the job uuid
                job_response_promise_[job_uuid].deliver(job_result_[job_uuid]);
                job_result_.erase(job_result_.find(job_uuid));
                job_response_promise_.erase(job_response_promise_.find(job_uuid));
                return;
            }

            while (job_inflight_[job_uuid] < 10) {
                // the version to add (first item in 'versions')
                JsonStore version(versions_to_add.begin().value());
                // erase it from 'versions', which is used to recursively calling this message
        handler versions_to_add.erase(versions_to_add.begin()); job_inflight_[job_uuid]++;
                request(pool_, caf::infinite, playlist::add_media_atom_v, version, media_rate,
        only_movies).then(
                    [=](const UuidActor &res) mutable {
                        request(
                            playlist,
                            infinite,
                            playlist::add_media_atom_v,
                            res,
                            before).then(
                                [=](const UuidActor& r) mutable {
                                    job_result_[job_uuid].push_back(r);
                                    job_inflight_[job_uuid]--;
                                    // continue processing this job
                                },
                                [=](const caf::error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    job_inflight_[job_uuid]--;
                                });
                    },
                    [=](const caf::error &err) mutable {
                        job_inflight_[job_uuid]--;
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
                if (versions_to_add.empty()) break;
            }
            send(this, playlist::add_media_atom_v, playlist, versions_to_add, job_uuid, before,
        media_rate, only_movies);

        },*/

        // not used.
        [=](get_data_atom,
            const std::string &entity,
            const utility::JsonStore &conditions) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();
            request(
                shotgun_,
                infinite,
                shotgun_entity_search_atom_v,
                entity,
                conditions,
                VersionFields,
                std::vector<std::string>(),
                1,
                30)
                .then(
                    [=](const JsonStore &proj) mutable { rp.deliver(proj); },
                    [=](error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_data_atom, const utility::JsonStore &js) -> result<utility::JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            try {
                if (js.at("operation") == "VersionFromIvy") {
                    find_ivy_version(
                        rp,
                        js.at("ivy_uuid").get<std::string>(),
                        js.at("job").get<std::string>());
                } else if (js.at("operation") == "GetShotFromId") {
                    find_shot(rp, js.at("shot_id").get<int>());
                } else if (js.at("operation") == "LinkMedia") {
                    link_media(rp, utility::Uuid(js.at("playlist_uuid")));
                } else if (js.at("operation") == "DownloadMedia") {
                    download_media(rp, utility::Uuid(js.at("media_uuid")));
                } else if (js.at("operation") == "MediaCount") {
                    get_valid_media_count(rp, utility::Uuid(js.at("playlist_uuid")));
                } else if (js.at("operation") == "PrepareNotes") {
                    UuidVector media_uuids;
                    for (const auto &i : js.value("media_uuids", std::vector<std::string>()))
                        media_uuids.push_back(Uuid(i));

                    prepare_playlist_notes(
                        rp,
                        utility::Uuid(js.at("playlist_uuid")),
                        media_uuids,
                        js.value("notify_owner", false),
                        js.value("notify_group_ids", std::vector<int>()),
                        js.value("combine", false),
                        js.value("add_time", false),
                        js.value("add_playlist_name", false),
                        js.value("add_type", false),
                        js.value("anno_requires_note", true),
                        js.value("skip_already_published", false),
                        js.value("default_type", ""));
                } else {
                    rp.deliver(
                        make_error(xstudio_error::error, std::string("Invalid operation.")));
                }
            } catch (const std::exception &err) {
                rp.deliver(make_error(
                    xstudio_error::error, std::string("Invalid operation.\n") + err.what()));
            }
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
void ShotgunDataSourceActor<T>::update_playlist_versions(
    caf::typed_response_promise<JsonStore> rp,
    const utility::Uuid &playlist_uuid,
    const int playlist_id) {
    // src should be a playlist actor..
    // and we want to update it..
    // retrieve shotgun metadata from playlist, and media items.
    try {

        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        auto pl_id = playlist_id;
        if (not pl_id) {
            auto plsg = request_receive<JsonStore>(
                *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

            pl_id = plsg["id"].template get<int>();
        }

        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // foreach medai actor get it's shogtun metadata.
        auto jsn = R"({"versions":[]})"_json;
        auto ver = R"({"id": 0, "type": "Version"})"_json;

        std::map<int, int> version_order_map;
        // get media detail
        int sort_order = 1;
        for (const auto &i : media) {
            try {
                auto mjson = request_receive<JsonStore>(
                    *sys,
                    i.actor(),
                    json_store::get_json_atom_v,
                    utility::Uuid(),
                    ShotgunMetadataPath + "/version");
                auto id   = mjson["id"].template get<int>();
                ver["id"] = id;
                jsn["versions"].push_back(ver);
                version_order_map[id] = sort_order;

                sort_order++;
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }

        // update playlist
        request(
            shotgun_,
            infinite,
            shotgun_update_entity_atom_v,
            "Playlists",
            pl_id,
            utility::JsonStore(jsn))
            .then(
                [=](const JsonStore &result) mutable {
                    // spdlog::warn("{}", JsonStore(result["data"]).dump(2));
                    // update playorder..
                    // get PlaylistVersionConnections
                    scoped_actor sys{system()};

                    auto order_filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["playlist", "is", {"type":"Playlist", "id":0}]
                    ]
                })"_json;

                    order_filter["conditions"][0][2]["id"] = pl_id;

                    try {
                        auto order = request_receive<JsonStore>(
                            *sys,
                            shotgun_,
                            shotgun_entity_search_atom_v,
                            "PlaylistVersionConnection",
                            JsonStore(order_filter),
                            std::vector<std::string>({"sg_sort_order", "version"}),
                            std::vector<std::string>({"sg_sort_order"}),
                            1,
                            4999);
                        // update all PlaylistVersionConnection's with new sort_order.
                        for (const auto &i : order["data"]) {
                            auto version_id = i.at("relationships")
                                                  .at("version")
                                                  .at("data")
                                                  .at("id")
                                                  .get<int>();
                            auto sort_order =
                                i.at("attributes").at("sg_sort_order").is_null()
                                    ? 0
                                    : i.at("attributes").at("sg_sort_order").get<int>();
                            // spdlog::warn("{} {}", std::to_string(sort_order),
                            // std::to_string(version_order_map[version_id]));
                            if (sort_order != version_order_map[version_id]) {
                                auto so_jsn             = R"({"sg_sort_order": 0})"_json;
                                so_jsn["sg_sort_order"] = version_order_map[version_id];
                                try {
                                    request_receive<JsonStore>(
                                        *sys,
                                        shotgun_,
                                        shotgun_update_entity_atom_v,
                                        "PlaylistVersionConnection",
                                        i.at("id").get<int>(),
                                        utility::JsonStore(so_jsn));
                                } catch (const std::exception &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                }
                            }
                        }

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }


                    if (pl_id != playlist_id)
                        anon_send(
                            playlist,
                            json_store::set_json_atom_v,
                            JsonStore(result["data"]),
                            ShotgunMetadataPath + "/playlist");
                    rp.deliver(result);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });

        // need to update/add PlaylistVersionConnection's
        // on creation the sort_order will be null.
        // PlaylistVersionConnection are auto created when adding to playlist. (I think)
        // so all we need to do is update..


        // get shotgun metadata
        // get media actors.
        // get media shotgun metadata.
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
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
void ShotgunDataSourceActor<T>::create_playlist(
    caf::typed_response_promise<JsonStore> rp, const utility::JsonStore &js) {
    // src should be a playlist actor..
    // and we want to update it..
    // retrieve shotgun metadata from playlist, and media items.
    try {

        scoped_actor sys{system()};

        auto playlist_uuid = Uuid(js["playlist_uuid"]);
        auto project_id    = js["project_id"].template get<int>();
        auto code          = js["code"].template get<std::string>();
        auto loc           = js["location"].template get<std::string>();
        auto playlist_type = js["playlist_type"].template get<std::string>();

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        auto jsn = R"({
            "project":{ "type": "Project", "id":null },
            "code": null,
            "sg_location": "unknown",
            "sg_type": "Dailies",
            "sg_date_and_time": null
         })"_json;

        jsn["project"]["id"]    = project_id;
        jsn["code"]             = code;
        jsn["sg_location"]      = loc;
        jsn["sg_type"]          = playlist_type;
        jsn["sg_date_and_time"] = date_time_and_zone();

        // "2021-08-18T19:00:00Z"

        // need to capture result to embed in playlist and add any media..
        request(
            shotgun_,
            infinite,
            shotgun_create_entity_atom_v,
            "playlists",
            utility::JsonStore(jsn))
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        // get new playlist id..
                        auto playlist_id = result.at("data").at("id").template get<int>();
                        // update shotgun versions from our source playlist.
                        // return the result..
                        update_playlist_versions(rp, playlist_uuid, playlist_id);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, result.dump(2));
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
            return rp.deliver(make_error(xstudio_error::error, "Invalid JSON"));
            ;
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
            playlist = spawn<playlist::PlaylistActor>("Shotgun Media", playlist_uuid, session);
        } catch (const std::exception &err) {
            spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    if (not playlist_uuid.is_null()) {
        event_msg = std::make_shared<event::Event>(
            "Loading Shotgun Playlist Media {}",
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
        if (not data.value("flag_text", "").empty() and
            not data.value("flag_colour", "").empty()) {
            flag_colour = data.value("flag_colour", "");
            flag_text   = data.value("flag_text", "");
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
                data.value("preferred_visual_source", ""),
                data.value("preferred_audio_source", ""),
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
void ShotgunDataSourceActor<T>::get_valid_media_count(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid) {
    try {
        // find playlist
        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist =
            request_receive<caf::actor>(*sys, session, session::get_playlist_atom_v, uuid);

        // get media..
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        if (not media.empty()) {
            fan_out_request<policy::select_all>(
                vector_to_caf_actor_vector(media),
                infinite,
                json_store::get_json_atom_v,
                utility::Uuid(),
                "")
                .then(
                    [=](std::vector<JsonStore> json) mutable {
                        int count = 0;
                        for (const auto &i : json) {
                            try {
                                if (i["metadata"].count("shotgun"))
                                    count++;
                            } catch (...) {
                            }
                        }

                        JsonStore result(R"({"result": {"valid":0, "invalid":0}})"_json);
                        result["result"]["valid"]   = count;
                        result["result"]["invalid"] = json.size() - count;
                        rp.deliver(result);
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
                    });
        } else {
            rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
        }
    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}


template <typename T>
void ShotgunDataSourceActor<T>::download_media(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid) {
    try {
        // find media
        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto media =
            request_receive<caf::actor>(*sys, session, playlist::get_media_atom_v, uuid);

        // get metadata, we need version id..
        auto media_metadata = request_receive<JsonStore>(
            *sys,
            media,
            json_store::get_json_atom_v,
            utility::Uuid(),
            "/metadata/shotgun/version");

        // spdlog::warn("{}", media_metadata.dump(2));

        auto name = media_metadata.at("attributes").at("code").template get<std::string>();
        auto job =
            media_metadata.at("attributes").at("sg_project_name").template get<std::string>();
        auto shot = media_metadata.at("relationships")
                        .at("entity")
                        .at("data")
                        .at("name")
                        .template get<std::string>();
        auto filepath = download_cache_.target_string() + "/" + name + "-" + job + "-" + shot +
                        ".dneg.webm";


        // check it doesn't already exist..
        if (fs::exists(filepath)) {
            // create source and add to media
            auto uuid   = Uuid::generate();
            auto source = spawn<media::MediaSourceActor>(
                "Shotgun Preview",
                utility::posix_path_to_uri(filepath),
                FrameList(),
                FrameRate(),
                uuid);
            request(media, infinite, media::add_media_source_atom_v, UuidActor(uuid, source))
                .then(
                    [=](const Uuid &u) mutable {
                        auto jsn          = JsonStore(R"({})"_json);
                        jsn["actor_uuid"] = uuid;
                        jsn["actor"]      = actor_to_string(system(), source);

                        rp.deliver(jsn);
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore((R"({})"_json)["error"] = to_string(err)));
                    });
        } else {
            request(
                shotgun_,
                infinite,
                shotgun_attachment_atom_v,
                "version",
                media_metadata.at("id").template get<int>(),
                "sg_uploaded_movie_webm")
                .then(
                    [=](const std::string &data) mutable {
                        if (data.size() > 1024 * 15) {
                            // write to file
                            std::ofstream o(filepath);
                            try {
                                o.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                                o << data << std::endl;
                                o.close();

                                // file written add to media as new source..
                                auto uuid   = Uuid::generate();
                                auto source = spawn<media::MediaSourceActor>(
                                    "Shotgun Preview",
                                    utility::posix_path_to_uri(filepath),
                                    FrameList(),
                                    FrameRate(),
                                    uuid);
                                request(
                                    media,
                                    infinite,
                                    media::add_media_source_atom_v,
                                    UuidActor(uuid, source))
                                    .then(
                                        [=](const Uuid &u) mutable {
                                            auto jsn          = JsonStore(R"({})"_json);
                                            jsn["actor_uuid"] = uuid;
                                            jsn["actor"] = actor_to_string(system(), source);

                                            rp.deliver(jsn);
                                        },
                                        [=](error &err) mutable {
                                            spdlog::error(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                            rp.deliver(JsonStore(
                                                (R"({})"_json)["error"] = to_string(err)));
                                        });

                            } catch (const std::exception &) {
                                // remove failed file
                                if (o.is_open()) {
                                    o.close();
                                    fs::remove(filepath);
                                }
                                spdlog::warn("Failed to open file");
                            }
                        } else {
                            rp.deliver(
                                JsonStore((R"({})"_json)["error"] = "Failed to download"));
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore((R"({})"_json)["error"] = to_string(err)));
                    });
        }
        // "content_type": "video/webm",
        // "id": 88463162,
        // "link_type": "upload",
        // "name": "b&#39;tmp_upload_webm_0okvakz6.webm&#39;",
        // "type": "Attachment",
        // "url": "http://shotgun.dneg.com/file_serve/attachment/88463162"

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(JsonStore((R"({})"_json)["error"] = err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::link_media(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid) {
    try {
        // find playlist
        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist =
            request_receive<caf::actor>(*sys, session, session::get_playlist_atom_v, uuid);

        // get media..
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // scan media for shotgun version / ivy uuid
        if (not media.empty()) {
            fan_out_request<policy::select_all>(
                vector_to_caf_actor_vector(media),
                infinite,
                json_store::get_json_atom_v,
                utility::Uuid(),
                "",
                true)
                .then(
                    [=](std::vector<std::pair<UuidActor, JsonStore>> json) mutable {
                        // ivy uuid is stored on source not media.. balls.
                        auto left    = std::make_shared<int>(0);
                        auto invalid = std::make_shared<int>(0);
                        for (const auto &i : json) {
                            try {
                                if (i.second.is_null() or
                                    not i.second["metadata"].count("shotgun")) {
                                    // request current media source metadata..
                                    scoped_actor sys{system()};
                                    auto source_meta = request_receive<JsonStore>(
                                        *sys,
                                        i.first.actor(),
                                        json_store::get_json_atom_v,
                                        "/metadata/external/DNeg");
                                    // we has got it..
                                    auto ivy_uuid = source_meta.at("Ivy").at("dnuuid");
                                    auto job      = source_meta.at("show");
                                    auto shot     = source_meta.at("shot");
                                    (*left) += 1;
                                    // spdlog::warn("{} {} {} {}", job, shot, ivy_uuid, *left);
                                    // call back into self ?
                                    // but we need to wait for the final result..
                                    // maybe in danger of deadlocks...
                                    // now we need to query shotgun..
                                    // to try and find version from this information.
                                    // this is then used to update the media actor.
                                    auto jsre        = JsonStore(GetVersionIvyUuidJSON);
                                    jsre["ivy_uuid"] = ivy_uuid;
                                    jsre["job"]      = job;

                                    request(
                                        caf::actor_cast<caf::actor>(this),
                                        infinite,
                                        get_data_atom_v,
                                        jsre)
                                        .then(
                                            [=](const JsonStore &ver) mutable {
                                                // got ver from uuid
                                                (*left)--;
                                                if (ver["payload"].empty()) {
                                                    (*invalid)++;
                                                } else {
                                                    // push version to media object
                                                    scoped_actor sys{system()};
                                                    try {
                                                        request_receive<bool>(
                                                            *sys,
                                                            i.first.actor(),
                                                            json_store::set_json_atom_v,
                                                            utility::Uuid(),
                                                            JsonStore(ver["payload"]),
                                                            ShotgunMetadataPath + "/version");
                                                    } catch (const std::exception &err) {
                                                        spdlog::warn(
                                                            "{} {}",
                                                            __PRETTY_FUNCTION__,
                                                            err.what());
                                                    }
                                                }

                                                if (not(*left)) {
                                                    JsonStore result(
                                                        R"({"result": {"valid":0, "invalid":0}})"_json);
                                                    result["result"]["valid"] =
                                                        json.size() - (*invalid);
                                                    result["result"]["invalid"] = (*invalid);
                                                    rp.deliver(result);
                                                }
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                (*left)--;
                                                (*invalid)++;
                                                if (not(*left)) {
                                                    JsonStore result(
                                                        R"({"result": {"valid":0, "invalid":0}})"_json);
                                                    result["result"]["valid"] =
                                                        json.size() - (*invalid);
                                                    result["result"]["invalid"] = (*invalid);
                                                    rp.deliver(result);
                                                }
                                            });
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        if (not(*left)) {
                            JsonStore result(R"({"result": {"valid":0, "invalid":0}})"_json);
                            result["result"]["valid"]   = json.size();
                            result["result"]["invalid"] = 0;
                            rp.deliver(result);
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
                    });
        } else {
            rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
        }


    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::find_ivy_version(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &uuid,
    const std::string &job) {
    // find version from supplied details.

    auto version_filter =
        FilterBy().And(Text("project.Project.name").is(job), Text("sg_ivy_dnuuid").is(uuid));

    request(
        shotgun_,
        std::chrono::seconds(static_cast<int>(data_source_.timeout_->value())),
        shotgun_entity_search_atom_v,
        "Version",
        JsonStore(version_filter),
        VersionFields,
        std::vector<std::string>(),
        1,
        1)
        .then(
            [=](const JsonStore &jsn) mutable {
                auto result = JsonStore(R"({"payload":[]})"_json);
                if (jsn.count("data") and jsn.at("data").size()) {
                    result["payload"] = jsn.at("data")[0];
                }
                rp.deliver(result);
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(JsonStore(R"({"payload":[]})"_json));
            });
}

template <typename T>
void ShotgunDataSourceActor<T>::find_shot(
    caf::typed_response_promise<utility::JsonStore> rp, const int shot_id) {
    // find version from supplied details.
    if (shot_cache_.count(shot_id))
        rp.deliver(shot_cache_.at(shot_id));

    request(
        shotgun_,
        std::chrono::seconds(static_cast<int>(data_source_.timeout_->value())),
        shotgun_entity_atom_v,
        "Shot",
        shot_id,
        ShotFields)
        .then(
            [=](const JsonStore &jsn) mutable {
                shot_cache_[shot_id] = jsn;
                rp.deliver(jsn);
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(JsonStore(R"({"data":{}})"_json));
            });
}

template <typename T>
void ShotgunDataSourceActor<T>::prepare_playlist_notes(
    caf::typed_response_promise<utility::JsonStore> rp,
    const utility::Uuid &playlist_uuid,
    const utility::UuidVector &media_uuids,
    const bool notify_owner,
    const std::vector<int> notify_group_ids,
    const bool combine,
    const bool add_time,
    const bool add_playlist_name,
    const bool add_type,
    const bool anno_requires_note,
    const bool skip_already_pubished,
    const std::string &default_type) {

    auto playlist_name = std::string();
    auto playlist_id   = int(0);
    auto payload       = R"({"payload":[], "valid": 0, "invalid": 0})"_json;

    try {
        scoped_actor sys{system()};

        // get session
        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        // get playlist
        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        // get shotgun info from playlist..
        try {
            auto sgpl = request_receive<JsonStore>(
                *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

            playlist_name = sgpl.at("attributes").at("code").template get<std::string>();
            playlist_id   = sgpl.at("id").template get<int>();

        } catch (const std::exception &err) {
            spdlog::info("No shotgun playlist information");
        }

        // get media for playlist.
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // no media so no point..
        // nothing to publish.
        if (media.empty())
            return rp.deliver(JsonStore(payload));

        std::vector<caf::actor> media_actors;

        if (not media_uuids.empty()) {
            auto lookup = uuidactor_vect_to_map(media);
            for (const auto &i : media_uuids) {
                if (lookup.count(i))
                    media_actors.push_back(lookup[i]);
            }
        } else {
            media_actors = vector_to_caf_actor_vector(media);
        }

        // get media shotgun json..
        // we can only publish notes for media that has version information
        fan_out_request<policy::select_all>(
            media_actors,
            infinite,
            json_store::get_json_atom_v,
            utility::Uuid(),
            ShotgunMetadataPath,
            true)
            .then(
                [=](std::vector<std::pair<UuidActor, JsonStore>> version_meta) mutable {
                    auto result = JsonStore(payload);

                    scoped_actor sys{system()};

                    std::map<Uuid, std::pair<UuidActor, JsonStore>> media_map;
                    UuidVector valid_media;

                    // get valid media.
                    // get all the shotgun info we need to publish
                    for (const auto &i : version_meta) {
                        try {
                            // spdlog::warn("{}", i.second.dump(2));
                            const auto &version = i.second.at("version");
                            auto jsn            = JsonStore(PublishNoteTemplateJSON);

                            // project
                            jsn["payload"]["project"]["id"] = version.at("relationships")
                                                                  .at("project")
                                                                  .at("data")
                                                                  .at("id")
                                                                  .get<int>();


                            // playlist link
                            jsn["payload"]["note_links"][0]["id"] = playlist_id;

                            if (version.at("relationships")
                                    .at("entity")
                                    .at("data")
                                    .value("type", "") == "Sequence")
                                // shot link
                                jsn["payload"]["note_links"][1]["id"] =
                                    version.at("relationships")
                                        .at("entity")
                                        .at("data")
                                        .value("id", 0);
                            else if (
                                version.at("relationships")
                                    .at("entity")
                                    .at("data")
                                    .value("type", "") == "Shot")
                                // sequence link
                                jsn["payload"]["note_links"][2]["id"] =
                                    version.at("relationships")
                                        .at("entity")
                                        .at("data")
                                        .value("id", 0);

                            // version link
                            jsn["payload"]["note_links"][3]["id"] = version.value("id", 0);

                            if (jsn["payload"]["note_links"][3]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(3);
                            if (jsn["payload"]["note_links"][2]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(2);
                            if (jsn["payload"]["note_links"][1]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(1);
                            if (jsn["payload"]["note_links"][0]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(0);

                            // we don't pass these to shotgun..
                            jsn["shot"] = version.at("relationships")
                                              .at("entity")
                                              .at("data")
                                              .at("name")
                                              .get<std::string>();
                            jsn["playlist_name"] = playlist_name;

                            if (notify_owner) // 1068
                                jsn["payload"]["addressings_to"][0]["id"] =
                                    version.at("relationships")
                                        .at("user")
                                        .at("data")
                                        .at("id")
                                        .get<int>();
                            else
                                jsn["payload"].erase("addressings_to");

                            if (not notify_group_ids.empty()) {
                                auto grp = R"({ "type": "Group", "id": null})"_json;
                                for (const auto g : notify_group_ids) {
                                    if (g <= 0)
                                        continue;

                                    grp["id"] = g;
                                    jsn["payload"]["addressings_cc"].push_back(grp);
                                }
                            }

                            if (jsn["payload"]["addressings_cc"].empty())
                                jsn["payload"].erase("addressings_cc");


                            media_map[i.first.uuid()] = std::make_pair(i.first, jsn);
                            valid_media.push_back(i.first.uuid());
                        } catch (const std::exception &err) {
                            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                    // get bookmark manager.
                    auto bookmarks = request_receive<caf::actor>(
                        *sys, session, bookmark::get_bookmark_atom_v);

                    // // collect media notes if they have shotgun metadata on the media
                    auto existing_bookmarks =
                        request_receive<std::vector<std::pair<Uuid, UuidActorVector>>>(
                            *sys, bookmarks, bookmark::get_bookmarks_atom_v, valid_media);

                    // get bookmark detail..
                    for (const auto &i : existing_bookmarks) {
                        // grouped by media item.
                        // we may want to collapse to unique note_types
                        std::map<std::string, std::map<int, std::vector<JsonStore>>>
                            notes_by_type;

                        for (const auto &j : i.second) {
                            try {
                                if (skip_already_pubished) {
                                    auto already_published = false;
                                    try {
                                        // check for shotgun metadata on note.
                                        request_receive<JsonStore>(
                                            *sys,
                                            j.actor(),
                                            json_store::get_json_atom_v,
                                            ShotgunMetadataPath + "/note");
                                        already_published = true;
                                    } catch (...) {
                                    }

                                    if (already_published)
                                        continue;
                                }

                                auto detail = request_receive<bookmark::BookmarkDetail>(
                                    *sys, j.actor(), bookmark::bookmark_detail_atom_v);
                                // skip notes with no text unless annotated and
                                // only_with_annotation is true
                                auto has_note = detail.note_ and not(*(detail.note_)).empty();
                                auto has_anno =
                                    detail.has_annotation_ and *(detail.has_annotation_);

                                if (not(has_note or (has_anno and not anno_requires_note)))
                                    continue;

                                auto [ua, jsn] = media_map[detail.owner_->uuid()];
                                // push to shotgun client..
                                jsn["bookmark_uuid"] = j.uuid();
                                if (not jsn.count("has_annotation"))
                                    jsn["has_annotation"] = R"([])"_json;

                                if (has_anno) {
                                    auto item =
                                        R"({"media_uuid": null, "media_name": null, "media_frame": 0, "timecode_frame": 0})"_json;
                                    item["media_uuid"]  = i.first;
                                    item["media_name"]  = jsn["shot"];
                                    item["media_frame"] = detail.start_frame();
                                    item["timecode_frame"] =
                                        detail.start_timecode_tc().total_frames();
                                    // requires media actor and first frame of annotation.
                                    jsn["has_annotation"].push_back(item);
                                }
                                auto cat = detail.category_ ? *(detail.category_) : "";
                                if (not default_type.empty())
                                    cat = default_type;

                                jsn["payload"]["sg_note_type"] = cat;
                                jsn["payload"]["subject"] =
                                    detail.subject_ ? *(detail.subject_) : "";
                                // format note content
                                std::string content;

                                if (add_time)
                                    content += std::string("Frame : ") +
                                               std::to_string(
                                                   detail.start_timecode_tc().total_frames()) +
                                               " / " + detail.start_timecode() + " / " +
                                               detail.duration_timecode() + "\n";

                                content += *(detail.note_);

                                jsn["payload"]["content"] = content;

                                // yeah this is a bit convoluted.
                                if (not notes_by_type.count(cat)) {
                                    notes_by_type.insert(std::make_pair(
                                        cat,
                                        std::map<int, std::vector<JsonStore>>(
                                            {{detail.start_frame(), {{jsn}}}})));
                                } else {
                                    if (notes_by_type[cat].count(detail.start_frame())) {
                                        notes_by_type[cat][detail.start_frame()].push_back(jsn);
                                    } else {
                                        notes_by_type[cat].insert(std::make_pair(
                                            detail.start_frame(),
                                            std::vector<JsonStore>({jsn})));
                                    }
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        try {
                            auto merged = JsonStore();

                            // category
                            for (auto &k : notes_by_type) {
                                auto category = k.first;
                                // frame
                                for (const auto &j : k.second) {
                                    // entry
                                    for (const auto &notepayload : j.second) {
                                        // spdlog::warn("{}",notepayload.dump(2));

                                        if (not merged.is_null() and
                                            (not combine or
                                             merged["payload"]["sg_note_type"] !=
                                                 notepayload["payload"]["sg_note_type"])) {
                                            // spdlog::warn("{}", merged.dump(2));
                                            result["payload"].push_back(merged);
                                            merged = JsonStore();
                                        }

                                        if (merged.is_null()) {
                                            merged       = notepayload;
                                            auto content = std::string();
                                            if (add_playlist_name and
                                                not merged["playlist_name"]
                                                        .get<std::string>()
                                                        .empty())
                                                content +=
                                                    "Playlist : " +
                                                    std::string(merged["playlist_name"]) + "\n";
                                            if (add_type)
                                                content += "Note Type : " +
                                                           merged["payload"]["sg_note_type"]
                                                               .get<std::string>() +
                                                           "\n\n";
                                            else
                                                content += "\n\n";

                                            merged["payload"]["content"] =
                                                content +
                                                merged["payload"]["content"].get<std::string>();

                                            merged.erase("shot");
                                            merged.erase("playlist_name");
                                        } else {
                                            merged["payload"]["content"] =
                                                merged["payload"]["content"]
                                                    .get<std::string>() +
                                                "\n\n" +
                                                notepayload["payload"]["content"]
                                                    .get<std::string>();
                                            merged["has_annotation"].insert(
                                                merged["has_annotation"].end(),
                                                notepayload["has_annotation"].begin(),
                                                notepayload["has_annotation"].end());
                                        }
                                    }
                                }
                            }

                            if (not merged.is_null())
                                result["payload"].push_back(merged);

                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }

                    result["valid"] = result["payload"].size();

                    // spdlog::warn("{}", result.dump(2));
                    rp.deliver(result);
                },
                [=](error &err) mutable {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(JsonStore(payload));
                });

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::create_playlist_notes(
    caf::typed_response_promise<utility::JsonStore> rp,
    const utility::JsonStore &notes,
    const utility::Uuid &playlist_uuid) {

    const std::string ui(R"(
        import xStudio 1.0
        import QtQuick 2.14
        XsLabel {
            anchors.fill: parent
            font.pixelSize: XsStyle.popupControlFontSize*1.2
            verticalAlignment: Text.AlignVCenter
            font.weight: Font.Bold
            color: palette.highlight
            text: "SG"
        }
    )");

    try {
        scoped_actor sys{system()};

        // get session
        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto bookmarks =
            request_receive<caf::actor>(*sys, session, bookmark::get_bookmark_atom_v);

        auto tags = request_receive<caf::actor>(*sys, session, xstudio::tag::get_tag_atom_v);

        auto count   = std::make_shared<int>(notes.size());
        auto failed  = std::make_shared<int>(0);
        auto succeed = std::make_shared<int>(0);

        auto offscreen_renderer =
            system().registry().template get<caf::actor>(offscreen_viewport_registry);
        auto thumbnail_manager =
            system().registry().template get<caf::actor>(thumbnail_manager_registry);

        for (const auto &j : notes) {
            // need to capture result to embed in playlist and add any media..
            // spdlog::warn("{}", j["payload"].dump(2));
            request(
                shotgun_,
                infinite,
                shotgun_create_entity_atom_v,
                "notes",
                utility::JsonStore(j["payload"]))
                .then(
                    [=](const JsonStore &result) mutable {
                        (*count)--;
                        try {
                            // "errors": [
                            //   {
                            //     "status": null
                            //   }
                            // ]
                            if (not result.at("errors")[0].at("status").is_null())
                                throw std::runtime_error(result["errors"].dump(2));

                            // get new playlist id..
                            auto note_id = result.at("data").at("id").template get<int>();
                            // we have a note...
                            if (not j["has_annotation"].empty()) {
                                for (const auto &anno : j["has_annotation"]) {
                                    request(
                                        session,
                                        infinite,
                                        playlist::get_media_atom_v,
                                        utility::Uuid(anno["media_uuid"]))
                                        .then(
                                            [=](const caf::actor &media_actor) mutable {
                                                // spdlog::warn("render annotation {}",
                                                // anno["media_frame"].get<int>());
                                                request(
                                                    offscreen_renderer,
                                                    infinite,
                                                    ui::viewport::
                                                        render_viewport_to_image_atom_v,
                                                    media_actor,
                                                    anno["media_frame"].get<int>(),
                                                    thumbnail::THUMBNAIL_FORMAT::TF_RGB24,
                                                    0,
                                                    true,
                                                    true)
                                                    .then(
                                                        [=](const thumbnail::ThumbnailBufferPtr
                                                                &tnail) {
                                                            // got buffer. convert to jpg..
                                                            request(
                                                                thumbnail_manager,
                                                                infinite,
                                                                media_reader::
                                                                    get_thumbnail_atom_v,
                                                                tnail)
                                                                .then(
                                                                    [=](const std::vector<
                                                                        std::byte>
                                                                            &jpgbuf) mutable {
                                                                        // final step...
                                                                        auto title = std::
                                                                            string(fmt::format(
                                                                                "{}_{}.jpg",
                                                                                anno["media_"
                                                                                     "name"]
                                                                                    .get<
                                                                                        std::
                                                                                            string>(),
                                                                                anno["timecode_"
                                                                                     "frame"]
                                                                                    .get<
                                                                                        int>()));
                                                                        request(
                                                                            shotgun_,
                                                                            infinite,
                                                                            shotgun_upload_atom_v,
                                                                            "note",
                                                                            note_id,
                                                                            "",
                                                                            title,
                                                                            jpgbuf,
                                                                            "image/jpeg")
                                                                            .then(
                                                                                [=](const bool) {
                                                                                },
                                                                                [=](const error &
                                                                                        err) mutable {
                                                                                    spdlog::warn(
                                                                                        "{} "
                                                                                        "Failed"
                                                                                        " uploa"
                                                                                        "d of "
                                                                                        "annota"
                                                                                        "tion "
                                                                                        "{}",
                                                                                        __PRETTY_FUNCTION__,
                                                                                        to_string(
                                                                                            err));
                                                                                }

                                                                            );
                                                                    },
                                                                    [=](const error
                                                                            &err) mutable {
                                                                        spdlog::warn(
                                                                            "{} Failed jpeg "
                                                                            "conversion {}",
                                                                            __PRETTY_FUNCTION__,
                                                                            to_string(err));
                                                                    });
                                                        },
                                                        [=](const error &err) mutable {
                                                            spdlog::warn(
                                                                "{} Failed render annotation "
                                                                "{}",
                                                                __PRETTY_FUNCTION__,
                                                                to_string(err));
                                                        });
                                            },
                                            [=](const error &err) mutable {
                                                spdlog::warn(
                                                    "{} Failed get media {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                            });
                                }
                            }

                            // spdlog::warn("note {}", result.dump(2));
                            // send json to note..
                            anon_send(
                                bookmarks,
                                json_store::set_json_atom_v,
                                utility::Uuid(j["bookmark_uuid"]),
                                utility::JsonStore(result.at("data")),
                                ShotgunMetadataPath + "/note");

                            xstudio::tag::Tag t;
                            t.set_type("Decorator");
                            t.set_data(ui);
                            t.set_link(utility::Uuid(j["bookmark_uuid"]));
                            t.set_unique(to_string(t.link()) + t.type() + t.data());

                            anon_send(tags, xstudio::tag::add_tag_atom_v, t);

                            // update shotgun versions from our source playlist.
                            // return the result..
                            // update_playlist_versions(rp, playlist_uuid, playlist_id);
                            (*succeed)++;
                        } catch (const std::exception &err) {
                            (*failed)++;
                            spdlog::warn(
                                "{} {} {}", __PRETTY_FUNCTION__, err.what(), result.dump(2));
                        }

                        if (not(*count)) {
                            auto jsn = JsonStore(R"({"data": {"status": ""}})"_json);
                            jsn["data"]["status"] = std::string(fmt::format(
                                "Successfully published {} / {} notes.",
                                *succeed,
                                (*failed) + (*succeed)));
                            rp.deliver(jsn);
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn(
                            "Failed create note entity {} {}",
                            __PRETTY_FUNCTION__,
                            to_string(err));
                        (*count)--;
                        (*failed)++;

                        if (not(*count)) {
                            auto jsn = JsonStore(R"({"data": {"status": ""}})"_json);
                            jsn["data"]["status"] = std::string(fmt::format(
                                "Successfully published {} / {} notes.",
                                *succeed,
                                (*failed) + (*succeed)));
                            rp.deliver(jsn);
                        }
                    });
        }

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

// template <typename T> void
// ShotgunDataSourceActor<T>::refresh_playlist_notes(caf::typed_response_promise<utility::JsonStore>
// rp, const utility::Uuid &playlist_uuid) {
//     try {
//         // find playlist
//         scoped_actor sys{system()};

//         // get session
//         auto session = request_receive<caf::actor>(
//                 *sys,
//                 system().registry().template get<caf::actor>(global_registry),
//                 session::session_atom_v);

//         // get playlist
//         auto playlist = request_receive<caf::actor>(
//                 *sys,
//                 session,
//                 session::get_playlist_atom_v,
//                 playlist_uuid);


//         // get media..
//         auto media = request_receive<std::vector<UuidActor>>(
//                 *sys,
//                 playlist,
//                 playlist::get_media_atom_v);

//         // no media so no point..
//         if(media.empty()) {
//             rp.deliver(JsonStore(R"({"data": {"status": "successful"}})"_json));
//             return;
//         }

//         auto bookmarks = request_receive<caf::actor>(
//                 *sys,
//                 session,
//                 bookmark::get_bookmark_atom_v);

//         // get shotgun playlist id..
//         auto sgpl = request_receive<JsonStore>(
//             *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath+"/playlist");
//         auto sgpl_id = sgpl["id"].template get<int>();

//         // get shotgun data..
//         // this calls ourself so need dispatch..
//         auto note_filter = R"(
//         {
//             "logical_operator": "and",
//             "conditions": [
//                 ["note_links", "in", {"type":"Playlist", "id":0}]
//             ]
//         })"_json;

//         note_filter["conditions"][0][2]["id"] = sgpl_id;

//         request(caf::actor_cast<caf::actor>(this),
//             SHOTGUN_TIMEOUT,
//             shotgun_entity_search_atom_v, "Notes",
//             JsonStore(note_filter),
//             std::vector<std::string>({"*"}),
//             std::vector<std::string>(),
//             1, 4999).then(
//                 [=](const JsonStore &jsn) mutable {
//                     // get metadata to see if they are tagged with version id's
//                     fan_out_request<policy::select_all>(
//                         vpair_second_to_v(media), infinite, json_store::get_json_atom_v,
//                         utility::Uuid(), ShotgunMetadataPath, true) .then(
//                             [=](std::vector<std::pair<UuidActor, JsonStore>> vmeta) mutable {
//                                 std::map<Uuid, caf::actor> media_map;
//                                 std::map<int, UuidActor> ver_media_map;
//                                 std::map<int, JsonStore> ver_note_map;

//                                 // map shot gun versions to media actors.
//                                 for(const auto &i: vmeta){
//                                     auto ver_id = i.second.value("version",
//                                     R"({})"_json).value<int>("id", 0);
//                                     // spdlog::warn("{} {} {}", ver_id,
//                                     to_string(i.first.first), i.second.dump(2)); if(ver_id){
//                                         ver_media_map[ver_id] = i.first;
//                                         // add media to map
//                                         media_map[i.first.first] = i.first.second;
//                                     }
//                                 }

//                                 // map notes to versions, maybe more than one note.
//                                 for(const auto &i : jsn["data"]) {
//                                     for(const auto &j :
//                                     i["relationships"]["note_links"]["data"] ){
//                                         auto ver_id = j.value<int>("id", 0);
//                                         if(j.value("type", "") == "Version") {
//                                             if(ver_media_map.count(ver_id)) {
//                                                 if(not ver_note_map.count(ver_id)) {
//                                                     ver_note_map[ver_id] = R"([])"_json;
//                                                 }
//                                                 ver_note_map[ver_id].push_back(i);
//                                                 // spdlog::warn("pushed to {}", ver_id);
//                                             } else {
//                                                 // spdlog::warn("No match {}", j.dump(2));
//                                             }
//                                             break;
//                                         }
//                                     }
//                                 }

//                                 scoped_actor sys{system()};
//                                 // collect all note metadata on all media.
//                                 // even if the note exists it's state might have changed..
//                                 // so we always update.
//                                 auto existing_bookmarks =
//                                 request_receive<std::vector<std::pair<Uuid,UuidActorVector>>>(
//                                     *sys, bookmarks, bookmark::get_bookmarks_atom_v,
//                                     map_key_to_vec(media_map)
//                                 );

//                                 // get metadata from existing bookmarks..
//                                 // do group query on bookmark json..
//                                 UuidVector meta_bookmarks;
//                                 for(const auto &i: existing_bookmarks) {
//                                     for(const auto &j: i.second) {
//                                         meta_bookmarks.push_back(j.first);
//                                     }
//                                 }
//                                 std::set<int> existing_notes;
//                                 auto bookmark_json =
//                                 request_receive<std::vector<std::pair<UuidActor,
//                                 JsonStore>>>(*sys, bookmarks, json_store::get_json_atom_v,
//                                 meta_bookmarks, ShotgunMetadataPath+"/note/id"); for(const
//                                 auto &i: bookmark_json) {
//                                     existing_notes.insert(i.second.get<int>());
//                                     // spdlog::warn("bookmark sg js {} {}",
//                                     i.second.dump(2),to_string(i.first.first));
//                                 }


//                                 // Create new notes and link to media
//                                 for(const auto &i: ver_note_map) {
//                                     for(const auto &j: i.second) {
//                                         if(existing_notes.count(j["id"].get<int>())) {
//                                             // spdlog::warn("Existing note skipping {}",
//                                             j["id"].get<int>()); continue;
//                                         }

//                                         // spdlog::warn("{}", j.dump(2));
//                                         // create bookmark
//                                         auto ba = request_receive<UuidActor>(
//                                             *sys, bookmarks, bookmark::add_bookmark_atom_v,
//                                             ver_media_map[i.first]
//                                         );
//                                         // set json data
//                                         anon_send(ba.second, json_store::set_json_atom_v,
//                                         JsonStore(j), ShotgunMetadataPath+"/note");

//                                         bookmark::BookmarkDetail detail;
//                                         try {
//                                             detail.author_ =
//                                             j["relationships"]["created_by"]["data"].value<std::string>("name","Anonymous");
//                                         } catch(...){
//                                             detail.author_ = "Anonymous";
//                                         }
//                                         detail.category_ =
//                                         j["attributes"].value<std::string>("sg_note_type",
//                                         "default"); detail.colour_ =
//                                         category_colours_.count(*(detail.category_)) ?
//                                         category_colours_[*(detail.category_)] : "";
//                                         detail.subject_ =
//                                         j["attributes"].value<std::string>("subject", "");
//                                         detail.note_ =
//                                         j["attributes"].value<std::string>("content", "");
//                                         detail.created_ =
//                                         to_sys_time_point(j["attributes"].value<std::string>("created_at",
//                                         "1972-03-19T00:00:00Z"));

//                                         // set detail
//                                         anon_send(ba.second,
//                                         bookmark::bookmark_detail_atom_v, detail);

//                                         // spdlog::warn("{} {} {}", i.first,
//                                         j.value<int>("id",0),
//                                         j["attributes"].value<std::string>("created_at",
//                                         ""));
//                                     }
//                                 }

//                                 rp.deliver(JsonStore(R"({"data": {"status":
//                                 "successful"}})"_json));
//                             },
//                             [=](error &err) mutable {
//                                 spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
//                                 rp.deliver(JsonStore(R"({"data": {"status":
//                                 "unsuccessful"}})"_json));
//                             });

//                 },
//                 [=](error &err) mutable {
//                     spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
//                     rp.deliver(JsonStore(R"({"data": {"status": "unsuccessful"}})"_json));
//                 }
//         );


//     } catch(const std::exception &err) {
//         spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
//         rp.deliver(make_error(xstudio_error::error, err.what()));
//     }
// }

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
    request(
        ivy,
        infinite,
        use_data_atom_v,
        ivy_media_task_data->sg_data_.at("attributes").at("sg_project_name").get<std::string>(),
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
                                    send_exit(source.actor(), caf::exit_reason::user_shutdown);

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
}


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<DataSourcePlugin<ShotgunDataSourceActor<ShotgunDataSource>>>(
                Uuid("33201f8d-db32-4278-9c40-8c068372a304"),
                "Shotgun",
                "DNeg",
                "Shotgun Data Source",
                semver::version("1.0.0"),
                "import Shotgun 1.0; ShotgunRoot {}"
                // "import Shotgun 1.0; ShotgunMenu {}"
                )}));
}
}
