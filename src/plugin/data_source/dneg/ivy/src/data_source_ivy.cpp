// SPDX-License-Identifier: Apache-2.0

#include <fmt/format.h>
#include <filesystem>
#include <caf/policy/select_all.hpp>

#include "data_source_ivy.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/http_client/http_client_actor.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace std::chrono_literals;

namespace fs = std::filesystem;

const auto GetShotFromId       = R"({"shot_id": null, "operation": "GetShotFromId"})"_json;
const auto ShotgunMetadataPath = std::string("/metadata/shotgun");
const auto IvyMetadataPath     = std::string("/metadata/ivy");
const auto SHOW_REGEX = std::regex(R"(^(?:/jobs|/hosts/[^/]+/user_data\d*)/([A-Z0-9]+)/.+$)");
const auto VALID_SHOW_REGEX = std::regex(R"(^[A-Z0-9]+$)");

const auto GetVersionIvyUuid =
    R"({"operation": "VersionIvyUuid", "job":null, "ivy_uuid": null})"_json;

class IvyMediaWorker : public caf::event_based_actor {
  public:
    IvyMediaWorker(caf::actor_config &cfg, caf::actor ivyactor);
    ~IvyMediaWorker() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "IvyMediaWorker";
    caf::behavior make_behavior() override { return behavior_; }


    void get_shotgun_metadata(
        caf::typed_response_promise<bool> rp,
        const caf::actor &media,
        const std::string &project,
        const utility::Uuid &stalk_dnuuid);

    void get_shotgun_version(
        caf::typed_response_promise<bool> rp,
        const caf::actor &media,
        const std::string &project,
        const utility::Uuid &stalk_dnuuid);

    void get_shotgun_shot(
        caf::typed_response_promise<bool> rp, const caf::actor &media, const int shot_id);

    void get_show_stalk_uuid(
        caf::typed_response_promise<std::pair<utility::Uuid, std::string>> rp,
        const caf::actor &media);

    void add_media(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const JsonStore &jsn,
        const FrameRate &media_rate);

    void add_media_source(
        caf::typed_response_promise<utility::UuidActor> rp,
        const JsonStore &jsn,
        const FrameRate &media_rate);

    void add_sources_to_media(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const JsonStore &jsn,
        const FrameRate &media_rate,
        const std::vector<std::pair<std::string, UuidActor>> &sources);

  private:
    caf::behavior behavior_;
    caf::actor ivy_actor_;
};

// {
//    "id": "10cc3fa1-e5ac-441b-b036-dbf9fd806d1e",
//    "name": "variance_alpha_proxy0",
//    "path":
//    "/jobs/NSFL/ldev_pipe/CG/CG_ldev_pipe_lighting_sgco_test_L010_BEAUTY_v004/variance_alpha/1000x1000/CG_ldev_pipe_lighting_sgco_test_L010_BEAUTY_v004___variance_alpha.1-1####.exr",
//    "version": {
//      "id": "716c59f1-17b1-49c0-bf73-25fbbb02fe1f",
//      "name": "CG_ldev_pipe_lighting_sgco_test_L010_BEAUTY_v004"
//    }
//  }

IvyMediaWorker::IvyMediaWorker(caf::actor_config &cfg, caf::actor ivyactor)
    : caf::event_based_actor(cfg), ivy_actor_(std::move(ivyactor)) {

    behavior_.assign(
        // frames
        [=](media::add_media_source_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate) -> result<UuidActor> {
            // should have obj..
            auto rp = make_response_promise<UuidActor>();
            add_media_source(rp, jsn, media_rate);
            return rp;
        },

        [=](data_source::use_data_atom,
            const caf::actor &media) -> result<std::pair<utility::Uuid, std::string>> {
            auto rp = make_response_promise<std::pair<utility::Uuid, std::string>>();
            get_show_stalk_uuid(rp, media);
            return rp;
        },

        // get shotgun metadata
        [=](data_source::use_data_atom,
            const caf::actor &media,
            const std::string &project,
            const utility::Uuid &stalk_dnuuid,
            const bool /*add_shotgun_data*/) -> result<bool> {
            auto rp = make_response_promise<bool>();
            get_shotgun_metadata(rp, media, project, stalk_dnuuid);
            return rp;
        },


        [=](playlist::add_media_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            auto rp = make_response_promise<UuidActorVector>();
            add_media(rp, jsn, media_rate);
            return rp;
        });
}

IvyDataSource::IvyDataSource() : DataSource("Ivy"), module::Module("IvyDataSource") {
    /*add_attributes();*/
    app_name_    = XSTUDIO_GLOBAL_NAME;
    app_version_ = XSTUDIO_GLOBAL_VERSION;
    host_        = get_host_name();
    user_        = get_login_name();

    auto show = get_env("SHOW");
    if (show)
        show_ = *show;
    else
        show_ = "NSFL";

    billing_code_ = show ? *show : std::string("costcentre");
    // billing_code_ = "costcentre";

    auto site = get_env("DNSITEDATA_SHORT_NAME");
    if (site)
        site_ = *site;
    else
        site_ = "gps";
}

template <typename T> void IvyDataSourceActor<T>::update_preferences(const JsonStore &js) {
    try {
        enable_audio_autoload_ =
            preference_value<bool>(js, "/plugin/data_source/ivy/enable_audio_autoload");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

template <typename T>
IvyDataSourceActor<T>::IvyDataSourceActor(caf::actor_config &cfg, const utility::JsonStore &)
    : caf::event_based_actor(cfg) {

    spdlog::debug("Created IvyDataSourceActor {}", name());

    data_source_.set_parent_actor_addr(actor_cast<caf::actor_addr>(this));

    http_ = spawn<http_client::HTTPClientActor>(CPPHTTPLIB_CONNECTION_TIMEOUT_SECOND, 20, 20);
    link_to(http_);

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        update_preferences(j);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    size_t worker_count = 5;

    pool_ = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count,
        [&] { return system().template spawn<IvyMediaWorker>(actor_cast<caf::actor>(this)); },
        caf::actor_pool::round_robin());
    link_to(pool_);

    system().registry().put(ivy_registry, this);

    behavior_.assign(
        [=](utility::name_atom) -> std::string { return name(); },
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // get version uuid..
        [=](data_source::use_data_atom,
            const caf::actor &media) -> result<std::pair<utility::Uuid, std::string>> {
            auto rp = make_response_promise<std::pair<utility::Uuid, std::string>>();
            get_show_stalk_uuid(rp, media);
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
        },
        [=](data_source::use_data_atom,
            const std::string &show,
            const std::vector<caf::uri> &paths) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            std::vector<std::string> ppaths;
            for (const auto &i : paths)
                ppaths.emplace_back(uri_to_posix_path(i));

            if (not std::regex_match(show.c_str(), VALID_SHOW_REGEX)) {
                spdlog::warn("{} Invalid show {}", __PRETTY_FUNCTION__, show);
                rp.deliver(make_error(xstudio_error::error, "Invalid show" + show));
                return rp;
            }

            auto httpquery = std::string(fmt::format(
                R"({{
                    files_by_path(show: "{}", paths: ["{}"]){{
                        id, name, path, timeline_range
                        version{{
                            id, name, show{{id, name}}, scope {{id, name}}, kind{{id, name}}
                        }},
                    }}
                }})",
                show,
                join_as_string(ppaths, "\",\"")));

            request(
                http_,
                infinite,
                http_client::http_post_atom_v,
                data_source_.url(),
                data_source_.path(),
                data_source_.get_headers(),
                httpquery,
                data_source_.content_type())
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            // spdlog::warn("ivy_load_version_sources {}", jsn.dump(2));

                            if (jsn.count("errors")) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, jsn.dump(2));
                                rp.deliver(make_error(xstudio_error::error, jsn.dump(2)));
                            } else {
                                rp.deliver(JsonStore(jsn));
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn(
                                "{} Invalid drop data {}", __PRETTY_FUNCTION__, err.what());
                            rp.deliver(make_error(xstudio_error::error, err.what()));
                        }
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(err);
                    });

            return rp;
        },


        [=](data_source::use_data_atom,
            const caf::actor &media,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            // get media metadata.
            auto rp = make_response_promise<UuidActorVector>();

            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                data_source::use_data_atom_v,
                media)
                .then(
                    [=](const std::pair<utility::Uuid, std::string> &uuid_show) mutable {
                        // spdlog::warn("{} {}", to_string(uuid_show.first), uuid_show.second);
                        // we've got a uuid
                        // get ivy data..
                        if (uuid_show.first.is_null())
                            return rp.deliver(UuidActorVector());

                        // get shotgun data.
                        anon_send(
                            pool_,
                            use_data_atom_v,
                            media,
                            uuid_show.second,
                            uuid_show.first,
                            true);
                        // delegate.. get sources from ivy
                        ivy_load_version_sources(
                            rp, uuid_show.second, uuid_show.first, media_rate);
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(UuidActorVector());
                    });

            return rp;
        },

        // drop event, return list of media actors.
        [=](data_source::use_data_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate,
            const bool) -> result<UuidActorVector> {
            auto rp = make_response_promise<UuidActorVector>();
            handle_drop(rp, jsn, media_rate);
            return rp;
        },

        // create audio sources from stem..
        [=](use_data_atom,
            const std::string &show,
            const utility::Uuid &stem_uuid,
            const FrameRate &media_rate,
            const bool pointless) -> result<UuidActorVector> {
            // get pipequery data.
            auto rp = make_response_promise<UuidActorVector>();
            ivy_load_audio_sources(rp, show, stem_uuid, media_rate, utility::UuidActorVector());
            return rp;
        },

        // create media sources from stalk..
        [=](use_data_atom,
            const std::string &show,
            const utility::Uuid &dnuuid,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            // get pipequery data.
            auto rp = make_response_promise<UuidActorVector>();
            ivy_load_version_sources(rp, show, dnuuid, media_rate);
            return rp;
        },

        // handle ivy URI
        [=](use_data_atom,
            const caf::uri &uri,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            if (uri.scheme() != "ivy")
                return UuidActorVector();

            if (to_string(uri.authority()) == "load") {
                auto rp = make_response_promise<UuidActorVector>();
                ivy_load(rp, uri, media_rate);
                return rp;
            } else {
                spdlog::warn(
                    "Invalid Ivy action {} {}", to_string(uri.authority()), to_string(uri));
            }
            return UuidActorVector();
        },

        // handle ivy URI
        [=](use_data_atom,
            const caf::uri &uri,
            const FrameRate &media_rate,
            const bool create_playlist) -> result<UuidActorVector> {
            if (uri.scheme() != "ivy")
                return UuidActorVector();

            if (to_string(uri.authority()) == "load") {
                auto rp = make_response_promise<UuidActorVector>();
                ivy_load(rp, uri, media_rate);
                return rp;
            } else {
                spdlog::warn(
                    "Invalid Ivy action {} {}", to_string(uri.authority()), to_string(uri));
            }
            return UuidActorVector();
        });
}

template <typename T> void IvyDataSourceActor<T>::on_exit() {
    system().registry().erase(ivy_registry);
}

httplib::Headers IvyDataSource::get_headers() const {
    return httplib::Headers({// {"Host", host_},
                             {"Content-Type", "application/graphql"},
                             {"X-Client-App-Name", to_lower(app_name_)},
                             {"X-Client-App-Version", app_version_},
                             {"X-Client-Billing-Code", billing_code_},
                             {"X-Client-Site", site_},
                             {"X-Client-User", user_},
                             {"X-Client-host", host_}});
}


void IvyMediaWorker::add_sources_to_media(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const JsonStore &jsn,
    const FrameRate &media_rate,
    const std::vector<std::pair<std::string, UuidActor>> &sources) {
    // it's a beauty !
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, UuidActor>>>>
        media_list;
    bool decompose = false;
    auto name      = jsn.at("name").get<std::string>();

    utility::UuidActorVector result;

    try {
        const auto kind = jsn.at("kind").at("id").get<std::string>();
        if (kind == "psref")
            decompose = true;
    } catch (...) {
    }


    if (decompose) {
        std::map<std::string, std::vector<std::pair<std::string, UuidActor>>> bucket;
        auto strip_ext = std::regex(R"(_(EXR|TIFF|JPEG)$)");

        for (const auto &i : sources) {
            auto media_name = name + " - " + i.first;

            media_name = std::regex_replace(media_name, strip_ext, "");

            if (not bucket.count(media_name))
                bucket[media_name] = std::vector<std::pair<std::string, UuidActor>>();

            bucket[media_name].emplace_back(std::make_pair(i.first, i.second));
        }

        for (const auto &i : bucket)
            media_list.emplace_back(std::make_pair(i.first, i.second));
    } else {
        media_list.emplace_back(std::make_pair(name, sources));
    }

    for (const auto &i : media_list) {
        UuidActorVector media_sources;
        auto select_uuid = Uuid();

        for (const auto &j : i.second) {
            media_sources.push_back(j.second);
            if ((select_uuid.is_null() and j.first == "movie_dneg") or
                j.first == "review_proxy0")
                select_uuid = j.second.uuid();
        }

        auto uuid  = utility::Uuid::generate();
        auto media = spawn<media::MediaActor>(i.first, uuid, media_sources);

        if (not select_uuid.is_null())
            anon_send(media, media::current_media_source_atom_v, select_uuid);

        anon_send(
            media,
            json_store::set_json_atom_v,
            utility::Uuid(),
            jsn,
            IvyMetadataPath + "/version");


        result.emplace_back(UuidActor(uuid, media));

        // only bother with shotgun  / audio on single media
        if (media_list.size() == 1)
            try {
                anon_send(
                    caf::actor_cast<caf::actor>(this),
                    data_source::use_data_atom_v,
                    media,
                    jsn.at("show").get<std::string>(),
                    utility::Uuid(jsn.at("id")),
                    true);
            } catch (...) {
            }
        request(
            ivy_actor_,
            infinite,
            use_data_atom_v,
            jsn.at("show").get<std::string>(),
            jsn.at("scope").at("id").get<Uuid>(),
            media_rate,
            true)
            .then(
                [=](const UuidActorVector &uas) {
                    anon_send(media, media::add_media_source_atom_v, uas);
                },
                [=](const error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });
    }

    rp.deliver(result);
}

void IvyMediaWorker::add_media_source(
    caf::typed_response_promise<utility::UuidActor> rp,
    const JsonStore &jsn,
    const FrameRate &media_rate) {

    // get info on file...
    FrameList frame_list;
    auto uri = parse_cli_posix_path(jsn.at("path"), frame_list, false);

    auto name = jsn.at("name").get<std::string>();

    if (jsn.at("version").at("kind").at("name") == "Audio") {
        auto label = std::string();
        auto type  = std::string();
        for (const auto &nt : jsn.at("version").at("name_tags")) {
            if (nt.at("name") == "label") {
                label = nt.at("value");
                name  = label;
            } else if (nt.at("name") == "type") {
                type = nt.at("value");
                name = type;
            }
        }

        if (not label.empty() and not type.empty()) {
            name = label + "-" + type;
        }
    }

    const auto source_uuid = Uuid::generate();
    auto source =
        frame_list.empty()
            ? spawn<media::MediaSourceActor>(name, uri, media_rate, source_uuid)
            : spawn<media::MediaSourceActor>(name, uri, frame_list, media_rate, source_uuid);
    anon_send(source, json_store::set_json_atom_v, jsn, IvyMetadataPath + "/file");

    rp.deliver(UuidActor(source_uuid, source));
}

void IvyMediaWorker::add_media(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const JsonStore &jsn,
    const FrameRate &media_rate) {

    auto count   = std::make_shared<int>(jsn.at("files").size());
    auto sources = std::make_shared<std::vector<std::pair<std::string, UuidActor>>>();

    for (const auto &i : jsn.at("files")) {
        // check we want it..
        if (i.at("type") == "METADATA" or i.at("type") == "THUMBNAIL" or
            i.at("type") == "SOURCE") {
            (*count)--;
            continue;
        }

        // need to filter unsupported leafs..
        FrameList frame_list;
        auto uri = parse_cli_posix_path(i.at("path"), frame_list, false);
        if (not is_file_supported(uri)) {
            (*count)--;
            continue;
        }

        request(
            caf::actor_cast<caf::actor>(this),
            infinite,
            media::add_media_source_atom_v,
            JsonStore(i),
            media_rate)
            .then(
                [=](const UuidActor &ua) mutable {
                    if (not ua.uuid().is_null())
                        sources->emplace_back(std::make_pair(i.at("name"), ua));
                    (*count)--;
                    if (not(*count))
                        add_sources_to_media(rp, jsn, media_rate, *sources);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    (*count)--;
                    if (not(*count))
                        add_sources_to_media(rp, jsn, media_rate, *sources);
                });
    }
}


void IvyMediaWorker::get_show_stalk_uuid(
    caf::typed_response_promise<std::pair<utility::Uuid, std::string>> rp,
    const caf::actor &media) {
    request(media, infinite, media::media_reference_atom_v)
        .then(
            [=](const std::vector<MediaReference> &mr) mutable {
                static std::regex res_re(R"(^\d+x\d+$)");
                std::cmatch m;
                auto dnuuid = utility::Uuid();
                auto show   = std::string("");

                // turn paths into possible dir locations...
                std::set<fs::path> paths;
                for (const auto &i : mr) {
                    auto tmp            = fs::path(uri_to_posix_path(i.uri())).parent_path();
                    const auto filename = tmp.filename().string();

                    if (std::regex_match(filename.c_str(), m, res_re))
                        paths.insert(tmp.parent_path());
                    else {
                        // movie path...
                        // extract
                        auto stalk = filename;
                        tmp        = tmp.parent_path();
                        auto type  = to_upper(filename);
                        if (type == "ELMR")
                            type = "ELEMENT";
                        else if (type == "CGR")
                            type = "CG";

                        tmp = tmp.parent_path();
                        tmp = tmp.parent_path(); // FIXED bug in fs::filesystem.
                        tmp /= type;
                        tmp /= stalk;
                        paths.insert(tmp);
                    }
                }

                for (const auto &i : paths) {
                    try {
                        if (fs::is_directory(i)) {
                            // iterate looking for stalk..
                            for (auto const &de : std::filesystem::directory_iterator{i}) {
                                auto fn = de.path().filename().string();
                                if (starts_with(fn, ".stalk_")) {
                                    // extract uuid..
                                    const auto dirname = i.string();
                                    if (std::regex_match(dirname.c_str(), m, SHOW_REGEX)) {
                                        dnuuid = utility::Uuid(fn.substr(7));
                                        show   = m[1];
                                    }
                                    break;
                                }
                            }
                        }
                    } catch (...) {
                    }
                    if (not dnuuid.is_null())
                        break;
                }

                // spdlog::warn("{} {}", to_string(dnuuid), show);
                rp.deliver(std::make_pair(dnuuid, show));
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}


void IvyMediaWorker::get_shotgun_metadata(
    caf::typed_response_promise<bool> rp,
    const caf::actor &media,
    const std::string &project,
    const utility::Uuid &stalk_dnuuid) {

    get_shotgun_version(rp, media, project, stalk_dnuuid);
}

void IvyMediaWorker::get_shotgun_version(
    caf::typed_response_promise<bool> rp,
    const caf::actor &media,
    const std::string &project,
    const utility::Uuid &stalk_dnuuid) {

    auto shotgun_actor = system().registry().template get<caf::actor>("SHOTBROWSER");

    if (not shotgun_actor)
        rp.deliver(false);
    else {
        // check it's not already there..
        request(
            media,
            infinite,
            json_store::get_json_atom_v,
            utility::Uuid(),
            ShotgunMetadataPath + "/version")
            .then(
                [=](const JsonStore &jsn) mutable {
                    try {
                        get_shotgun_shot(
                            rp,
                            media,
                            jsn.at("relationships").at("entity").at("data").value("id", 0));
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        rp.deliver(false);
                    }
                },
                [=](const error &err) mutable {
                    // get from shotgun..
                    auto jsre        = JsonStore(GetVersionIvyUuid);
                    jsre["ivy_uuid"] = to_string(stalk_dnuuid);
                    jsre["job"]      = project;

                    request(shotgun_actor, infinite, data_source::get_data_atom_v, jsre)
                        .then(
                            [=](const JsonStore &jsn) mutable {
                                if (jsn.count("payload")) {
                                    request(
                                        media,
                                        infinite,
                                        json_store::set_json_atom_v,
                                        utility::Uuid(),
                                        JsonStore(jsn.at("payload")),
                                        ShotgunMetadataPath + "/version")
                                        .then(
                                            [=](const bool result) mutable {
                                                try {
                                                    get_shotgun_shot(
                                                        rp,
                                                        media,
                                                        jsn.at("payload")
                                                            .at("relationships")
                                                            .at("entity")
                                                            .at("data")
                                                            .value("id", 0));
                                                } catch (const std::exception &err) {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        err.what());
                                                    rp.deliver(false);
                                                }
                                            },
                                            [=](const error &err) mutable {
                                                rp.deliver(false);
                                            });
                                } else {
                                    rp.deliver(false);
                                }
                            },
                            [=](const error &err) mutable {
                                // get from shotgun..
                                rp.deliver(false);
                            });
                });
    }
}

void IvyMediaWorker::get_shotgun_shot(
    caf::typed_response_promise<bool> rp, const caf::actor &media, const int shot_id) {

    auto shotgun_actor = system().registry().template get<caf::actor>("SHOTBROWSER");

    if (not shotgun_actor)
        rp.deliver(false);
    else {
        // check it's not already there..
        request(
            media,
            infinite,
            json_store::get_json_atom_v,
            utility::Uuid(),
            ShotgunMetadataPath + "/shot")
            .then(
                [=](const JsonStore &jsn) mutable { rp.deliver(true); },
                [=](const error &err) mutable {
                    // get from shotgun..
                    try {
                        auto shotreq       = JsonStore(GetShotFromId);
                        shotreq["shot_id"] = shot_id;

                        request(shotgun_actor, infinite, get_data_atom_v, shotreq)
                            .then(
                                [=](const JsonStore &jsn) mutable {
                                    try {
                                        if (jsn.contains("data")) {
                                            anon_send(
                                                media,
                                                json_store::set_json_atom_v,
                                                utility::Uuid(),
                                                JsonStore(jsn.at("data")),
                                                ShotgunMetadataPath + "/shot");
                                            rp.deliver(true);
                                        } else {
                                            rp.deliver(false);
                                        }
                                    } catch (const std::exception &err) {
                                        rp.deliver(false);
                                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                    }
                                },
                                [=](const error &err) mutable {
                                    rp.deliver(false);
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                });
                    } catch (const std::exception &err) {
                        rp.deliver(false);
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                });
    }
}

template <typename T>
void IvyDataSourceActor<T>::ivy_load(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const caf::uri &uri,
    const utility::FrameRate &media_rate) {

    auto query = uri.query();

    if (query.count("type") and query["type"] == "Version" and query.count("show") and
        query.count("ids")) {
        ivy_load_version(rp, uri, media_rate);
    } else if (
        query.count("type") and query["type"] == "File" and query.count("show") and
        query.count("ids")) {
        ivy_load_file(rp, uri, media_rate);
    } else {
        spdlog::warn("Invalid Ivy action {}, requires type, id", to_string(uri));
        rp.deliver(utility::UuidActorVector());
    }
}

// load stalk as vector of sources.

template <typename T>
void IvyDataSourceActor<T>::ivy_load_version_sources(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const std::string &show,
    const utility::Uuid &stalk_dnuuid,
    const utility::FrameRate &media_rate) {

    if (not std::regex_match(show.c_str(), VALID_SHOW_REGEX)) {
        spdlog::warn("{} Invalid show {}", __PRETTY_FUNCTION__, show);
        return rp.deliver(make_error(xstudio_error::error, "Invalid show" + show));
    }

    auto httpquery = std::string(fmt::format(
        R"({{
            versions_by_id(show: "{}", ids: ["{}"]){{
                id, name, number{{major,minor,micro}}, kind{{id,name}},scope{{id,name}}
                files{{
                    id,name,path,timeline_range,type,version{{id,name,kind{{id,name}},scope{{id,name}}}}
                }},
            }}
        }})",
        show,
        to_string(stalk_dnuuid)));

    request(
        http_,
        infinite,
        http_client::http_post_atom_v,
        data_source_.url(),
        data_source_.path(),
        data_source_.get_headers(),
        httpquery,
        data_source_.content_type())
        .then(
            [=](const httplib::Response &response) mutable {
                try {
                    auto jsn = nlohmann::json::parse(response.body);

                    // spdlog::warn("ivy_load_version_sources {}", jsn.dump(2));

                    if (jsn.count("errors")) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, jsn.dump(2));
                        rp.deliver(UuidActorVector());
                    } else {
                        // spdlog::warn("{}", jsn.dump(2));
                        // we got valid results..
                        // get number of leafs
                        const auto &ivy_files =
                            jsn.at("data").at("versions_by_id").at(0).at("files");

                        const auto scope_uuid = jsn.at("data")
                                                    .at("versions_by_id")
                                                    .at(0)
                                                    .at("scope")
                                                    .at("id")
                                                    .get<Uuid>();

                        auto files = JsonStore(R"([])"_json);
                        for (const auto &i : ivy_files) {
                            // check we want it..
                            if (i.at("type") == "METADATA" or i.at("type") == "THUMBNAIL" or
                                i.at("type") == "SOURCE")
                                continue;

                            // need to filter unsupported leafs..
                            FrameList frame_list;
                            auto uri = parse_cli_posix_path(i.at("path"), frame_list, false);
                            if (is_file_supported(uri))
                                files.push_back(i);
                            // spdlog::warn("{}", i.at("name"));
                        }

                        auto count   = std::make_shared<int>(files.size());
                        auto results = std::make_shared<UuidActorVector>();
                        if (not *count)
                            return rp.deliver(UuidActorVector());

                        for (const auto &i : files) {
                            auto payload    = JsonStore(i);
                            payload["show"] = show;
                            request(
                                pool_,
                                infinite,
                                media::add_media_source_atom_v,
                                payload,
                                media_rate)
                                .then(
                                    [=](const UuidActor &ua) mutable {
                                        (*count)--;
                                        if (not ua.uuid().is_null())
                                            results->push_back(ua);
                                        if (not(*count)) {
                                            if (enable_audio_autoload_)
                                                ivy_load_audio_sources(
                                                    rp, show, scope_uuid, media_rate, *results);
                                            else
                                                rp.deliver(*results);
                                        }
                                    },
                                    [=](error &err) mutable {
                                        (*count)--;
                                        if (not(*count)) {
                                            if (enable_audio_autoload_)
                                                ivy_load_audio_sources(
                                                    rp, show, scope_uuid, media_rate, *results);
                                            else
                                                rp.deliver(*results);
                                        }
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), response.body);
                    rp.deliver(UuidActorVector());
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(UuidActorVector());
            });
}


// load as vector of media.

template <typename T>
void IvyDataSourceActor<T>::ivy_load_version(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const caf::uri &uri,
    const utility::FrameRate &media_rate) {

    auto query = uri.query();
    auto ids   = std::string("\"") + join_as_string(split(query["ids"], '|'), "\",\"") + "\"";
    auto show  = query["show"];
    auto httpquery = std::string(fmt::format(
        R"({{
            versions_by_id(show: "{}", ids: [{}]){{
                id, name, number{{major,minor,micro}}, kind{{id,name}},scope{{id,name}}
                files{{id,name,path,timeline_range,type,version{{id,name,kind{{id,name}},scope{{id,name}}}}}},
            }}
        }})",
        show,
        ids));

    if (not std::regex_match(show.c_str(), VALID_SHOW_REGEX)) {
        spdlog::warn("{} Invalid show {}", __PRETTY_FUNCTION__, show);
        return rp.deliver(make_error(xstudio_error::error, "Invalid show" + show));
    }


    request(
        http_,
        infinite,
        http_client::http_post_atom_v,
        data_source_.url(),
        data_source_.path(),
        data_source_.get_headers(),
        httpquery,
        data_source_.content_type())
        .then(
            [=](const httplib::Response &response) mutable {
                try {
                    auto jsn = nlohmann::json::parse(response.body);

                    if (jsn.count("errors")) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, jsn.dump(2));
                        rp.deliver(UuidActorVector());
                    } else {
                        // we got valid results..
                        auto count =
                            std::make_shared<int>(jsn.at("data").at("versions_by_id").size());
                        auto results = std::make_shared<UuidActorVector>();

                        if (not *count)
                            return rp.deliver(UuidActorVector());

                        for (const auto &i : jsn.at("data").at("versions_by_id")) {
                            auto payload = JsonStore(i);

                            // spdlog::warn("ivy_load_version {}", payload.dump(2));

                            payload["show"] = show;

                            request(
                                pool_,
                                infinite,
                                playlist::add_media_atom_v,
                                payload,
                                media_rate)
                                .then(
                                    [=](const UuidActorVector &uav) mutable {
                                        (*count)--;

                                        for (const auto &ua : uav)
                                            if (not ua.uuid().is_null())
                                                results->push_back(ua);

                                        if (not(*count))
                                            rp.deliver(*results);
                                    },
                                    [=](error &err) mutable {
                                        (*count)--;
                                        if (not(*count))
                                            rp.deliver(*results);
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), response.body);
                    rp.deliver(UuidActorVector());
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(UuidActorVector());
            });
}

template <typename T>
void IvyDataSourceActor<T>::ivy_load_file(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const caf::uri &uri,
    const utility::FrameRate &media_rate) {

    auto query = uri.query();
    auto ids   = std::string("\"") + join_as_string(split(query["ids"], '|'), "\",\"") + "\"";
    auto show  = query["show"];
    auto httpquery = std::string(fmt::format(
        R"({{
            files_by_id(show: "{}", ids: [{}]){{
                id, name, path,timeline_range, type, version{{id,name,kind{{id,name}},scope{{id,name}}}}
            }}
        }})",
        show,
        ids));

    if (not std::regex_match(show.c_str(), VALID_SHOW_REGEX)) {
        spdlog::warn("{} Invalid show {}", __PRETTY_FUNCTION__, show);
        return rp.deliver(make_error(xstudio_error::error, "Invalid show" + show));
    }

    request(
        http_,
        infinite,
        http_client::http_post_atom_v,
        data_source_.url(),
        data_source_.path(),
        data_source_.get_headers(),
        httpquery,
        data_source_.content_type())
        .then(
            [=](const httplib::Response &response) mutable {
                try {
                    auto jsn = nlohmann::json::parse(response.body);
                    // spdlog::warn("ivy_load_file {}", jsn.dump(2));
                    // check result and distribuite to loader..
                    if (jsn.count("errors")) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, jsn.dump(2));
                        rp.deliver(UuidActorVector());
                    } else {
                        auto count =
                            std::make_shared<int>(jsn.at("data").at("files_by_id").size());
                        auto results = std::make_shared<UuidActorVector>();

                        if (not *count)
                            return rp.deliver(UuidActorVector());

                        for (const auto &i : jsn.at("data").at("files_by_id")) {
                            auto payload    = JsonStore(i);
                            payload["show"] = show;

                            // process media files.
                            request(
                                pool_,
                                infinite,
                                media::add_media_source_atom_v,
                                payload,
                                media_rate)
                                .then(
                                    [=](const UuidActor &ua) mutable {
                                        // may have a new media item to add to playlist..
                                        (*count)--;
                                        if (not ua.uuid().is_null()) {
                                            auto uuid  = utility::Uuid::generate();
                                            auto media = spawn<media::MediaActor>(
                                                i.at("version").at("name"),
                                                uuid,
                                                utility::UuidActorVector({ua}));
                                            try {
                                                // add shotgun ?
                                                anon_send(
                                                    pool_,
                                                    data_source::use_data_atom_v,
                                                    media,
                                                    std::string(show),
                                                    utility::Uuid(i.at("version").at("id")),
                                                    true);
                                            } catch (...) {
                                            }

                                            results->push_back(UuidActor(uuid, media));
                                        }
                                        if (not(*count)) {
                                            rp.deliver(*results);
                                        }
                                    },
                                    [=](error &err) mutable {
                                        (*count)--;
                                        if (not(*count)) {
                                            rp.deliver(*results);
                                        }
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), response.body);
                    rp.deliver(UuidActorVector());
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(UuidActorVector());
            });
}

template <typename T>
void IvyDataSourceActor<T>::handle_drop(
    caf::typed_response_promise<UuidActorVector> rp,
    const JsonStore &jsn,
    const utility::FrameRate &media_rate) {

    auto uris = std::vector<caf::uri>();

    if (jsn.count("application/x-dneg-ivy-entities-v1") and
        not jsn.at("application/x-dneg-ivy-entities-v1").empty()) {
        try {
            auto obj = nlohmann::json::parse(
                jsn.at("application/x-dneg-ivy-entities-v1")[0].get<std::string>());
            // should be a json object...
            for (const auto &entry : obj) {
                try {
                    // name, and path are also available
                    auto uri = caf::make_uri(std::string(fmt::format(
                        "ivy://load?show={}&type={}&ids={}",
                        entry.at("show"),
                        entry.at("type"),
                        entry.at("id"))));

                    if (uri)
                        uris.push_back(*uri);
                } catch (const std::exception &err) {
                    spdlog::warn(
                        "{} Invalid drop data {} {}",
                        __PRETTY_FUNCTION__,
                        err.what(),
                        obj.dump(2));
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} Invalid drop data {}", __PRETTY_FUNCTION__, err.what());
        }
    } else if (
        jsn.count("text/plain") and jsn["text/plain"].size() and
        (ends_with(jsn["text/plain"][0].get<std::string>(), " Leaf") or
         ends_with(jsn["text/plain"][0].get<std::string>(), " Stalk"))) {

        for (const auto &i : jsn["text/plain"]) {
            // "movie_dneg (1920x1080) 01bb500c-2b22-416a-a4b3-f8e1b37fd615 Leaf"
            // CG_ldev_pipe_lighting_sgco_test_L010_BEAUTY_v005
            // 8ae34a08-b586-474a-b862-863e9c5bad98 Stalk
            auto terms = resplit(i);

            if (terms.size() >= 3) {
                auto type = "File";
                if (terms[terms.size() - 1] == "Stalk")
                    type = "Version";

                auto uri = caf::make_uri(
                    std::string("ivy://load?show=") + data_source_.show() + "&type=" + type +
                    "&ids=" + terms[terms.size() - 2]);
                if (uri)
                    uris.push_back(*uri);
                // anon_send(caf::actor_cast<caf::actor>(this), use_data_atom_v, *uri,
                // playlist.second.second);
            }
        }
    }

    if (not uris.empty()) {
        auto count = std::make_shared<int>(uris.size());
        auto media = std::make_shared<UuidActorVector>();

        for (const auto &i : uris) {
            request(caf::actor_cast<caf::actor>(this), infinite, use_data_atom_v, i, media_rate)
                .then(
                    [=](const UuidActorVector &uav) mutable {
                        (*count)--;
                        for (const auto &i : uav)
                            media->push_back(i);
                        if (not(*count))
                            rp.deliver(*media);
                    },
                    [=](error &err) mutable {
                        (*count)--;
                        if (not(*count))
                            rp.deliver(*media);
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }
    } else {
        rp.deliver(UuidActorVector());
    }
}


template <typename T>
void IvyDataSourceActor<T>::get_show_stalk_uuid(
    caf::typed_response_promise<std::pair<utility::Uuid, std::string>> rp,
    const caf::actor &media) {
    request(media, infinite, json_store::get_json_atom_v, utility::Uuid(), "")
        .then(
            [=](const JsonStore &jsn) mutable {
                // spdlog::warn("{}", jsn.dump(2));

                try {
                    return rp.deliver(std::make_pair(
                        utility::Uuid(jsn.at("metadata")
                                          .at("shotgun")
                                          .at("version")
                                          .at("attributes")
                                          .at("sg_ivy_dnuuid")
                                          .get<std::string>()),
                        jsn.at("metadata")
                            .at("shotgun")
                            .at("version")
                            .at("relationships")
                            .at("project")
                            .at("data")
                            .at("name")
                            .get<std::string>()));
                } catch (...) {
                    try {
                        // needs fixing.. purposely broke..
                        // so how do you get the show ????
                        auto path = jsn.at("metadata")
                                        .at("ivy")
                                        .at("version")
                                        .at("files")
                                        .at(0)
                                        .at("path")
                                        .get<std::string>();
                        std::cmatch m;

                        if (std::regex_match(path.c_str(), m, SHOW_REGEX)) {
                            return rp.deliver(std::make_pair(
                                utility::Uuid(jsn.at("metadata")
                                                  .at("ivy")
                                                  .at("version")
                                                  .at("id")
                                                  .get<std::string>()),
                                m[1]));
                        } else
                            throw XStudioError("Show not found in ivy metadata");
                    } catch (...) {
                        // try finding via media source paths...
                        // collect media source paths.
                        request(media, infinite, media::media_reference_atom_v)
                            .then(
                                [=](const std::vector<MediaReference> &refs) mutable {
                                    // collect paths.
                                    auto eshow       = get_env("SHOW");
                                    std::string show = eshow ? *eshow : std::string("");
                                    std::vector<caf::uri> paths;
                                    // spdlog::error("594 {}", show);

                                    for (const auto &mr : refs) {
                                        try {
                                            // extract show...
                                            std::cmatch m;
                                            auto ppath = uri_to_posix_path(mr.uri());
                                            if (std::regex_match(
                                                    ppath.c_str(), m, SHOW_REGEX)) {

                                                show = m[1].str();
                                                paths.push_back(mr.uri());
                                                // spdlog::error("606 {}", show);
                                            }
                                        } catch (const std::exception &err) {
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, err.what());
                                        }
                                    }

                                    if (not show.empty()) {
                                        // spdlog::error("615 {}", show);

                                        // get stalks from paths
                                        request(
                                            caf::actor_cast<caf::actor>(this),
                                            infinite,
                                            data_source::use_data_atom_v,
                                            show,
                                            paths)
                                            .then(
                                                [=](const JsonStore &data) mutable {
                                                    try {
                                                        if (data.at("data")
                                                                .at("files_by_path")
                                                                .size()) {
                                                            rp.deliver(std::make_pair(
                                                                data.at("data")
                                                                    .at("files_by_path")
                                                                    .at(0)
                                                                    .at("version")
                                                                    .at("id")
                                                                    .get<Uuid>(),
                                                                show));
                                                        } else {
                                                            rp.delegate(
                                                                pool_,
                                                                data_source::use_data_atom_v,
                                                                media);
                                                        }
                                                    } catch (const std::exception &err) {
                                                        spdlog::warn(
                                                            "{} {}",
                                                            __PRETTY_FUNCTION__,
                                                            err.what());
                                                        rp.delegate(
                                                            pool_,
                                                            data_source::use_data_atom_v,
                                                            media);
                                                    }
                                                },
                                                [=](const error &err) mutable {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                    rp.deliver(err);
                                                });
                                    } else {
                                        // last stand... ivy uuid from filesystem.
                                        rp.delegate(pool_, data_source::use_data_atom_v, media);
                                    }
                                },
                                [=](const error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(err);
                                });
                    }
                }
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}


template <typename T>
void IvyDataSourceActor<T>::ivy_load_audio_sources(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const std::string &show,
    const utility::Uuid &stem_dnuuid,
    const utility::FrameRate &media_rate,
    const utility::UuidActorVector &extend) {

    auto httpquery = std::string(fmt::format(
        R"({{
  latest_versions(
    mode: VERSION_NUMBER
    show: "{}"
    scopes: {{
      id: "{}"
    }}
    statuses: APPROVED
    kinds: "aud"
  ) {{
    id
    name
    number {{
      major
      minor
      micro
    }}
    kind {{
      id
    }}
    scope{{id,name}}
    name_tags {{name,value}}
    files {{
      id
      name
      path
      timeline_range
      type
      version{{
        id
        name
        kind {{id, name}}
        name_tags {{
            name
            value
        }}
        scope{{id,name}}
      }}
    }}
  }}
        }})",
        show,
        to_string(stem_dnuuid)));

    if (not std::regex_match(show.c_str(), VALID_SHOW_REGEX)) {
        spdlog::warn("{} Invalid show {}", __PRETTY_FUNCTION__, show);
        return rp.deliver(make_error(xstudio_error::error, "Invalid show" + show));
    }

    request(
        http_,
        infinite,
        http_client::http_post_atom_v,
        data_source_.url(),
        data_source_.path(),
        data_source_.get_headers(),
        httpquery,
        data_source_.content_type())
        .then(
            [=](const httplib::Response &response) mutable {
                try {
                    auto jsn = nlohmann::json::parse(response.body);
                    if (jsn.count("errors")) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, jsn.dump(2));
                        rp.deliver(extend);
                    } else {
                        // we got valid results..
                        // get number of leafs
                        auto files = JsonStore(R"([])"_json);
                        // scan audio versions

                        for (const auto &v : jsn.at("data").at("latest_versions")) {
                            for (const auto &i : v.at("files")) {
                                // check we want it..
                                if (i.at("type") == "METADATA" or i.at("type") == "THUMBNAIL" or
                                    i.at("type") == "SOURCE")
                                    continue;

                                // need to filter unsupported leafs..
                                FrameList frame_list;
                                auto uri =
                                    parse_cli_posix_path(i.at("path"), frame_list, false);
                                if (is_file_supported(uri))
                                    files.push_back(i);
                            }
                        }

                        auto count   = std::make_shared<int>(files.size());
                        auto results = std::make_shared<UuidActorVector>(extend);
                        if (not *count)
                            return rp.deliver(*results);

                        // spdlog::warn("{} {}", *count, (*results).size());

                        for (const auto &i : files) {
                            // spdlog::warn("{}", i.dump(2));
                            auto payload    = JsonStore(i);
                            payload["show"] = show;
                            request(
                                pool_,
                                infinite,
                                media::add_media_source_atom_v,
                                payload,
                                media_rate)
                                .then(
                                    [=](const UuidActor &ua) mutable {
                                        (*count)--;
                                        if (not ua.uuid().is_null())
                                            results->push_back(ua);
                                        if (not(*count)) {
                                            // spdlog::warn("{}", (*results).size());
                                            rp.deliver(*results);
                                        }
                                    },
                                    [=](error &err) mutable {
                                        (*count)--;
                                        if (not(*count)) {
                                            // spdlog::warn("{}", (*results).size());
                                            rp.deliver(*results);
                                        }
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), response.body);
                    rp.deliver(extend);
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(extend);
            });
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<DataSourcePlugin<IvyDataSourceActor<IvyDataSource>>>(
                Uuid("3dae2d56-8ac5-4b88-b923-5ac03eefe4c3"),
                "Ivy",
                "DNeg",
                "Ivy Data Source",
                semver::version("1.0.0"))}));
}
}
