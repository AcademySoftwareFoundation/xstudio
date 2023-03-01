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
#include "xstudio/event/event.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/http_client/http_client_actor.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace std::chrono_literals;

namespace fs = std::filesystem;

class IvyMediaWorker : public caf::event_based_actor {
  public:
    IvyMediaWorker(caf::actor_config &cfg);
    ~IvyMediaWorker() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "IvyMediaWorker";
    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
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

IvyMediaWorker::IvyMediaWorker(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    behavior_.assign(
        // frames
        [=](media::add_media_source_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate) -> result<UuidActor> {
            // should have obj..
            auto rp = make_response_promise<UuidActor>();

            // get info on file...
            FrameList frame_list;
            auto uri = parse_cli_posix_path(jsn.at("path"), frame_list, false);

            const auto source_uuid = Uuid::generate();
            auto source            = frame_list.empty()
                                         ? spawn<media::MediaSourceActor>(
                                    jsn.at("name"), uri, media_rate, source_uuid)
                                         : spawn<media::MediaSourceActor>(
                                    jsn.at("name"), uri, frame_list, media_rate, source_uuid);
            anon_send(source, json_store::set_json_atom_v, jsn, "/metadata/ivy/file");

            rp.deliver(UuidActor(source_uuid, source));
            return rp;
        },

        [=](data_source::use_data_atom,
            const caf::actor &media) -> result<std::pair<utility::Uuid, std::string>> {
            auto rp = make_response_promise<std::pair<utility::Uuid, std::string>>();

            request(media, infinite, media::media_reference_atom_v)
                .then(
                    [=](const std::vector<MediaReference> &mr) mutable {
                        static std::regex res_re(R"(^\d+x\d+$)");
                        static std::regex show_re(
                            R"(^(?:/jobs|/hosts/[^/]+/user_data\d*)/(.+?)/.+$)");
                        std::cmatch m;
                        auto dnuuid = utility::Uuid();
                        auto show   = std::string("");

                        // turn paths into possible dir locations...
                        std::set<fs::path> paths;
                        for (const auto &i : mr) {
                            auto tmp = fs::path(uri_to_posix_path(i.uri())).parent_path();
                            if (std::regex_match(tmp.filename().string().c_str(), m, res_re))
                                paths.insert(tmp.parent_path());
                            else {
                                // movie path...
                                // extract
                                auto stalk = tmp.filename().string();
                                tmp        = tmp.parent_path();
                                auto type  = to_upper(tmp.filename().string());
                                if (type == "ELMR")
                                    type = "ELEMENT";
                                else if (type == "CGR")
                                    type = "CG";

                                tmp = tmp.parent_path();
                                tmp = tmp.parent_path();
                                tmp /= type;
                                tmp /= stalk;
                                paths.insert(tmp);
                            }
                        }

                        for (const auto &i : paths) {
                            try {
                                if (fs::is_directory(i)) {
                                    // iterate looking for stalk..
                                    for (auto const &de :
                                         std::filesystem::directory_iterator{i}) {
                                        auto fn = de.path().filename().string();
                                        if (starts_with(fn, ".stalk_")) {
                                            // extract uuid..
                                            if (std::regex_match(
                                                    i.string().c_str(), m, show_re)) {
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

                        rp.deliver(std::make_pair(dnuuid, show));
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(err);
                    });

            return rp;
        },


        [=](data_source::use_data_atom,
            const caf::actor &media,
            const std::string &project,
            const utility::Uuid &stalk_dnuuid,
            const bool /*add_shotgun_data*/) -> result<bool> {
            auto rp = make_response_promise<bool>();
            auto shotgun_actor =
                system().registry().template get<caf::actor>("SHOTGUNDATASOURCE");

            if (not shotgun_actor)
                rp.deliver(false);
            else {
                // check it's not already there..
                request(
                    media,
                    infinite,
                    json_store::get_json_atom_v,
                    utility::Uuid(),
                    "/metadata/shotgun/version")
                    .then(
                        [=](const JsonStore &jsn) mutable { rp.deliver(true); },
                        [=](const error &err) mutable {
                            // get from shotgun..
                            request(
                                shotgun_actor,
                                infinite,
                                data_source::use_data_atom_v,
                                project,
                                stalk_dnuuid)
                                .then(
                                    [=](const JsonStore &jsn) mutable {
                                        if (jsn.count("payload")) {
                                            request(
                                                media,
                                                infinite,
                                                json_store::set_json_atom_v,
                                                utility::Uuid(),
                                                JsonStore(jsn.at("payload")),
                                                "/metadata/shotgun/version")
                                                .then(
                                                    [=](const bool result) mutable {
                                                        rp.deliver(result);
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

            return rp;
        },


        [=](playlist::add_media_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate) -> result<UuidActor> {
            auto rp      = make_response_promise<UuidActor>();
            auto count   = std::make_shared<int>(jsn.at("files").size());
            auto sources = std::make_shared<UuidActorVector>();

            auto select_idx  = std::make_shared<int>(0);
            auto current_idx = 0;

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

                // spdlog::warn("{}", i.at("name"));

                if (not *select_idx and i.at("name") == "movie_dneg")
                    *select_idx = current_idx;
                else if (i.at("name") == "review_proxy0")
                    *select_idx = current_idx;

                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    media::add_media_source_atom_v,
                    JsonStore(i),
                    media_rate)
                    .then(
                        [=](const UuidActor &ua) mutable {
                            if (not ua.uuid().is_null())
                                sources->push_back(ua);
                            (*count)--;
                            if (not(*count)) {
                                auto uuid  = utility::Uuid::generate();
                                auto media = spawn<media::MediaActor>(
                                    jsn.at("name").get<std::string>(), uuid, *sources);
                                // spdlog::warn("SET IVY VERSION.");
                                anon_send(
                                    media,
                                    json_store::set_json_atom_v,
                                    utility::Uuid(),
                                    jsn,
                                    "/metadata/ivy/version");

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

                                if (*select_idx) {
                                    anon_send(
                                        media,
                                        media::current_media_source_atom_v,
                                        (*sources)[*select_idx].uuid());
                                }
                                rp.deliver(UuidActor(uuid, media));
                            }
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            (*count)--;
                            if (not(*count)) {
                                auto uuid  = utility::Uuid::generate();
                                auto media = spawn<media::MediaActor>(
                                    jsn.at("name").get<std::string>(), uuid, *sources);
                                anon_send(
                                    media,
                                    json_store::set_json_atom_v,
                                    utility::Uuid(),
                                    jsn,
                                    "/metadata/ivy/version");

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
                                if (*select_idx)
                                    anon_send(
                                        media,
                                        media::current_media_source_atom_v,
                                        (*sources)[*select_idx].uuid());
                                rp.deliver(UuidActor(uuid, media));
                            }
                        });
                current_idx++;
            }
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

    // billing_code_ = show_;
    billing_code_ = "costcentre";

    auto site = get_env("DNSITEDATA_SHORT_NAME");
    if (site)
        site_ = *site;
    else
        site_ = "gps";
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

template <typename T>
IvyDataSourceActor<T>::IvyDataSourceActor(caf::actor_config &cfg, const utility::JsonStore &)
    : caf::event_based_actor(cfg) {

    spdlog::debug("Created IvyDataSourceActor {}", name());

    data_source_.set_parent_actor_addr(actor_cast<caf::actor_addr>(this));

    http_ = spawn<http_client::HTTPClientActor>(
        CPPHTTPLIB_CONNECTION_TIMEOUT_SECOND, CPPHTTPLIB_READ_TIMEOUT_SECOND * 2);
    link_to(http_);


    size_t worker_count = 5;

    pool_ = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count,
        [&] { return system().template spawn<IvyMediaWorker>(); },
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
                                static std::regex show_re(
                                    R"(^(?:/jobs|/hosts/[^/]+/user_data\d*)/(.+?)/.+$)");
                                auto path = jsn.at("metadata")
                                                .at("ivy")
                                                .at("version")
                                                .at("files")
                                                .at(0)
                                                .at("path")
                                                .get<std::string>();
                                std::cmatch m;

                                if (std::regex_match(path.c_str(), m, show_re)) {
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
                                // last stand... ivy uuid from filesystem.
                                rp.delegate(pool_, data_source::use_data_atom_v, media);
                            }
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
                        // we've got a uuid
                        // get ivy data..
                        if (uuid_show.first.is_null())
                            return rp.deliver(UuidActorVector());

                        // delegate..
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this),
                            use_data_atom_v,
                            uuid_show.second,
                            uuid_show.first);
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(UuidActorVector());
                    });

            return rp;
        },

        [=](data_source::use_data_atom,
            const JsonStore &jsn,
            const bool) -> result<UuidActorVector> {
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
                            std::string("ivy://load?show=") + data_source_.show() +
                            "&type=" + type + "&ids=" + terms[terms.size() - 2]);
                        if (uri)
                            uris.push_back(*uri);
                        // anon_send(caf::actor_cast<caf::actor>(this), use_data_atom_v, *uri,
                        // playlist.second.second);
                    }
                }
            }

            if (not uris.empty()) {
                auto rp    = make_response_promise<UuidActorVector>();
                auto count = std::make_shared<int>(uris.size());
                auto media = std::make_shared<UuidActorVector>();

                for (const auto &i : uris) {
                    request(caf::actor_cast<caf::actor>(this), infinite, use_data_atom_v, i)
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

                return rp;
            }

            return UuidActorVector();
        },


        [=](use_data_atom atom, const std::string &show, const utility::Uuid &dnuuid) {
            delegate(caf::actor_cast<caf::actor>(this), atom, show, dnuuid, FrameRate());
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

        [=](use_data_atom, const caf::uri &uri) -> result<UuidActorVector> {
            if (uri.scheme() != "ivy")
                return UuidActorVector();

            if (to_string(uri.authority()) == "load") {
                auto rp = make_response_promise<UuidActorVector>();
                ivy_load(rp, uri);
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

template <typename T>
void IvyDataSourceActor<T>::ivy_load(
    caf::typed_response_promise<utility::UuidActorVector> rp, const caf::uri &uri) {

    auto query = uri.query();

    if (query.count("type") and query["type"] == "Version" and query.count("show") and
        query.count("ids")) {
        ivy_load_version(rp, uri);
    } else if (
        query.count("type") and query["type"] == "File" and query.count("show") and
        query.count("ids")) {
        ivy_load_file(rp, uri);
    } else {
        spdlog::warn("Invalid Ivy action {}, requires type, id", to_string(uri));
        rp.deliver(utility::UuidActorVector());
    }
}

// load stalk leaf as vector of sources.
template <typename T>
void IvyDataSourceActor<T>::ivy_load_version_sources(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const std::string &show,
    const utility::Uuid &stalk_dnuuid,
    const utility::FrameRate &media_rate) {
    auto httpquery = std::string(fmt::format(
        R"({{
            versions_by_id(show: "{}", ids: ["{}"]){{
                id, name, number{{major,minor,micro}}, kind{{id}},
                files{{
                    id,name,path,type,version{{id,name}}
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
                    if (jsn.count("errors")) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, jsn.dump(2));
                        rp.deliver(UuidActorVector());
                    } else {
                        // spdlog::warn("{}", jsn.dump(2));
                        // we got valid results..
                        // get number of leafs
                        const auto &ivy_files =
                            jsn.at("data").at("versions_by_id").at(0).at("files");

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
void IvyDataSourceActor<T>::ivy_load_version(
    caf::typed_response_promise<utility::UuidActorVector> rp, const caf::uri &uri) {

    // should come from session rate as the fallback ?

    auto media_rate = FrameRate();
    auto query      = uri.query();
    auto ids  = std::string("\"") + join_as_string(split(query["ids"], '|'), "\",\"") + "\"";
    auto show = query["show"];
    auto httpquery = std::string(fmt::format(
        R"({{
            versions_by_id(show: "{}", ids: [{}]){{
                id, name, number{{major,minor,micro}}, kind{{id}},
                files{{id,name,path,type,version{{id,name}}}},
            }}
        }})",
        show,
        ids));

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
                            auto payload    = JsonStore(i);
                            payload["show"] = show;

                            request(
                                pool_,
                                infinite,
                                playlist::add_media_atom_v,
                                payload,
                                media_rate)
                                .then(
                                    [=](const UuidActor &ua) mutable {
                                        (*count)--;
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
    caf::typed_response_promise<utility::UuidActorVector> rp, const caf::uri &uri) {
    // hardwired to 24fps, as we can't know where we're being added to.
    auto media_rate = FrameRate();
    // this code probably needs to move at some point.
    auto query = uri.query();
    auto ids   = std::string("\"") + join_as_string(split(query["ids"], '|'), "\",\"") + "\"";
    auto show  = query["show"];
    auto httpquery = std::string(fmt::format(
        R"({{
            files_by_id(show: "{}", ids: [{}]){{
                id, name, path, type, version{{id,name}},
            }}
        }})",
        show,
        ids));

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
                                                anon_send(
                                                    caf::actor_cast<caf::actor>(this),
                                                    data_source::use_data_atom_v,
                                                    media,
                                                    std::string(show),
                                                    utility::Uuid(i.at("version").at("id")),
                                                    true);
                                            } catch (...) {
                                            }

                                            results->push_back(UuidActor(uuid, media));
                                        }
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
void IvyDataSourceActor<T>::ivy_load_audio_sources(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const std::string &show,
    const utility::Uuid &stem_dnuuid) {
    auto media_rate = FrameRate();
    auto httpquery  = std::string(fmt::format(
        R"({{
  latest_versions(
    mode: VERSION_NUMBER
    show: "{}"
    scopes: {{
      id: "{}"
    }}
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
    name_tags {{name,value}}
    files {{
      id
      name
      path
      type
      version{{
        id
        name
        name_tags {{
            name
            value
        }}
      }}
    }}
  }}
        }})",
        show,
        to_string(stem_dnuuid)));

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
                        // spdlog::warn("{}", jsn.dump(2));
                        // we got valid results..
                        // get number of leafs
                        const auto &ivy_files =
                            jsn.at("data").at("versions_by_id").at(0).at("files");

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
