// SPDX-License-Identifier: Apache-2.0

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/http_client/http_client_actor.hpp"
#include "xstudio/shotgun_client/shotgun_client_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::http_client;
using namespace xstudio::utility;
using namespace caf;


using sce = shotgun_client_error;

ShotgunClientActor::ShotgunClientActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    init();
}

void ShotgunClientActor::init() {
    spdlog::debug("Created ShotgunClientActor");
    print_on_exit(this, "ShotgunClientActor");

    http_ = spawn<HTTPClientActor>(CPPHTTPLIB_CONNECTION_TIMEOUT_SECOND, 20, 20);
    link_to(http_);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    secret_source_ = actor_cast<caf::actor_addr>(this);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },

        [=](shotgun_authentication_source_atom, caf::actor source) {
            secret_source_ = actor_cast<caf::actor_addr>(source);
        },
        [=](shotgun_authentication_source_atom) -> caf::actor {
            return actor_cast<caf::actor>(secret_source_);
        },

        [=](shotgun_acquire_authentication_atom) -> result<AuthenticateShotgun> {
            return make_error(sce::authentication_error, "No authentication source.");
        },

        [=](shotgun_acquire_token_atom) -> result<std::pair<std::string, std::string>> {
            // spdlog::warn("shotgun_acquire_token_atom");

            auto rp = make_response_promise<std::pair<std::string, std::string>>();

            if (not base_.refresh_token().empty()) {
                // spdlog::warn("not base_.refresh_token().empty()");
                request_refresh_queue_.push(rp);
                if (request_refresh_queue_.size() > 1)
                    return rp;
                request(actor_cast<caf::actor>(this), infinite, shotgun_refresh_token_atom_v)
                    .then(
                        [=](const std::pair<std::string, std::string> &tokens) mutable {
                            // spdlog::info("refresh from token");
                            base_.set_authenticated(true);
                            while (not request_refresh_queue_.empty()) {
                                request_refresh_queue_.front().deliver(tokens);
                                request_refresh_queue_.pop();
                            }
                        },
                        [=](error &) mutable {
                            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            // try using login
                            base_.expire_refresh_token();
                            while (not request_refresh_queue_.empty()) {
                                request_refresh_queue_.front().delegate(
                                    actor_cast<caf::actor>(this), shotgun_acquire_token_atom_v);
                                request_refresh_queue_.pop();
                            }
                        });
            } else {
                if (not base_.failed_authentication()) {
                    acquire_token(rp);
                } else {
                    // spdlog::info("refresh from login");
                    // request secret.
                    request(
                        actor_cast<caf::actor>(secret_source_),
                        infinite,
                        shotgun_acquire_authentication_atom_v,
                        base_.failed_authentication() ? "Authentication failed, try again."
                                                      : "")
                        .then(
                            [=](const AuthenticateShotgun &auth) mutable {
                                base_.set_credentials_method(auth);
                                rp.delegate(
                                    actor_cast<caf::actor>(this), shotgun_acquire_token_atom_v);
                            },
                            [=](error &err) mutable {
                                base_.set_authenticated(false);
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(std::move(err));
                            });
                }
            }
            return rp;
        },

        [=](shotgun_authenticate_atom, const AuthenticateShotgun &auth) {
            base_.set_credentials_method(auth);
            delegate(actor_cast<caf::actor>(this), shotgun_acquire_token_atom_v);
        },

        [=](shotgun_link_atom, const std::string &link) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            // spdlog::warn("shotgun_link_atom");
            if (not base_.authenticated()) {
                request(actor_cast<caf::actor>(this), infinite, shotgun_acquire_token_atom_v)
                    .then(
                        [=](const std::pair<std::string, std::string>) mutable {
                            rp.delegate(
                                actor_cast<caf::actor>(this), shotgun_link_atom_v, link);
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } else {
                request(
                    http_,
                    infinite,
                    http_get_atom_v,
                    base_.scheme_host_port(),
                    link,
                    base_.get_auth_headers())
                    .then(
                        [=](const httplib::Response &response) mutable {
                            try {
                                auto jsn = nlohmann::json::parse(response.body);

                                try {
                                    if (not jsn["errors"][0]["status"].is_null() and
                                        jsn["errors"][0]["status"].get<int>() == 401) {
                                        // try and authorise..
                                        request(
                                            actor_cast<caf::actor>(this),
                                            infinite,
                                            shotgun_acquire_token_atom_v)
                                            .then(
                                                [=](const std::pair<
                                                    std::string,
                                                    std::string>) mutable {
                                                    rp.delegate(
                                                        actor_cast<caf::actor>(this),
                                                        shotgun_link_atom_v,
                                                        link);
                                                },
                                                [=](error &err) mutable {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                    rp.deliver(JsonStore(std::move(jsn)));
                                                });
                                        return;
                                    }

                                } catch (const std::exception &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                }
                                rp.deliver(JsonStore(std::move(jsn)));
                            } catch (const std::exception &err) {
                                rp.deliver(make_error(sce::response_error, err.what()));
                            }
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            }


            return rp;
        },

        [=](shotgun_update_entity_atom,
            const std::string &entity,
            const int record_id,
            const JsonStore &body) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            // spdlog::warn("shotgun_update_entity_atom");
            request(
                http_,
                infinite,
                http_put_atom_v,
                base_.scheme_host_port(),
                std::string("/api/v1/entity/" + entity + "/" + std::to_string(record_id)),
                base_.get_auth_headers(),
                body.dump(),
                base_.content_type_json())
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_update_entity_atom_v,
                                                    entity,
                                                    record_id,
                                                    body);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_create_entity_atom,
            const std::string &entity,
            const JsonStore &body) -> result<JsonStore> {
            // spdlog::warn("shotgun_create_entity_atom");
            auto rp = make_response_promise<JsonStore>();
            request(
                http_,
                infinite,
                http_post_atom_v,
                base_.scheme_host_port(),
                std::string("/api/v1/entity/" + entity),
                base_.get_auth_headers(),
                body.dump(),
                base_.content_type_json())
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_create_entity_atom_v,
                                                    entity,
                                                    body);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_entity_atom atom, const std::string &entity, const int record_id) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                entity,
                record_id,
                std::vector<std::string>());
        },

        [=](shotgun_entity_atom,
            const std::string &entity,
            const int record_id,
            const std::vector<std::string> &fields) -> result<JsonStore> {
            // spdlog::warn("shotgun_entity_atom");
            auto rp = make_response_promise<JsonStore>();

            if (not base_.authenticated()) {
                request(actor_cast<caf::actor>(this), infinite, shotgun_acquire_token_atom_v)
                    .then(
                        [=](const std::pair<std::string, std::string>) mutable {
                            rp.delegate(
                                actor_cast<caf::actor>(this),
                                shotgun_entity_atom_v,
                                entity,
                                record_id,
                                fields);
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } else {
                // requires authentication..
                request(
                    http_,
                    infinite,
                    http_get_atom_v,
                    base_.scheme_host_port(),
                    std::string("/api/v1/entity/" + entity + "/" + std::to_string(record_id)),
                    base_.get_auth_headers(),
                    httplib::Params(
                        {{"fields", fields.empty() ? "*" : join_as_string(fields, ",")}}))
                    .then(
                        [=](const httplib::Response &response) mutable {
                            try {
                                auto jsn = nlohmann::json::parse(response.body);

                                try {
                                    if (not jsn["errors"][0]["status"].is_null() and
                                        jsn["errors"][0]["status"].get<int>() == 401) {
                                        // try and authorise..
                                        request(
                                            actor_cast<caf::actor>(this),
                                            infinite,
                                            shotgun_acquire_token_atom_v)
                                            .then(
                                                [=](const std::pair<
                                                    std::string,
                                                    std::string>) mutable {
                                                    rp.delegate(
                                                        actor_cast<caf::actor>(this),
                                                        shotgun_entity_atom_v,
                                                        entity,
                                                        record_id,
                                                        fields);
                                                },
                                                [=](error &err) mutable {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                    rp.deliver(JsonStore(std::move(jsn)));
                                                });
                                        return;
                                    }

                                } catch (const std::exception &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                }
                                rp.deliver(JsonStore(std::move(jsn)));
                            } catch (const std::exception &err) {
                                rp.deliver(make_error(sce::response_error, err.what()));
                            }
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            }
            return rp;
        },

        [=](shotgun_entity_filter_atom atom, const std::string &entity) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                entity,
                JsonStore(),
                std::vector<std::string>(),
                std::vector<std::string>(),
                1,
                1000);
        },

        [=](shotgun_entity_filter_atom atom,
            const std::string &entity,
            const JsonStore &filter) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                entity,
                filter,
                std::vector<std::string>(),
                std::vector<std::string>(),
                1,
                1000);
        },

        [=](shotgun_entity_filter_atom atom,
            const std::string &entity,
            const JsonStore &filter,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort) {
            delegate(actor_cast<caf::actor>(this), atom, entity, filter, fields, sort, 1, 1000);
        },

        [=](shotgun_entity_filter_atom,
            const std::string &entity,
            const JsonStore &filter,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort,
            const int page,
            const int page_size) -> result<JsonStore> {
            // spdlog::warn("shotgun_entity_filter_atom");

            auto rp = make_response_promise<JsonStore>();

            if (not base_.authenticated()) {
                request(actor_cast<caf::actor>(this), infinite, shotgun_acquire_token_atom_v)
                    .then(
                        [=](const std::pair<std::string, std::string>) mutable {
                            rp.delegate(
                                actor_cast<caf::actor>(this),
                                shotgun_entity_filter_atom_v,
                                entity,
                                filter,
                                fields,
                                sort,
                                page,
                                page_size);
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } else {
                try {
                    httplib::Params params;

                    if (page) {
                        params.insert(std::make_pair("number", std::to_string(page)));
                        params.insert(std::make_pair("size", std::to_string(page_size)));
                    }

                    if (fields.empty()) {
                        params.insert(std::make_pair("fields", "*"));
                    } else {
                        params.insert(std::make_pair("fields", join_as_string(fields, ",")));
                    }

                    if (not sort.empty()) {
                        params.insert(std::make_pair("sort", join_as_string(sort, ",")));
                    }

                    if (not filter.empty()) {
                        // should be an array..
                        for (const auto &[key, value] : filter.items()) {
                            if (value.is_array())
                                params.insert(std::make_pair(
                                    "filter[" + key + "]",
                                    join_as_string(
                                        value.get<std::vector<std::string>>(), ",")));
                            else
                                params.insert(std::make_pair(
                                    "filter[" + key + "]", value.get<std::string>()));
                        }
                    }

                    /*
                       curl -X GET
                       https://yoursite.shotgunstudio.com/api/v1/entity/assets?filter[sg_status_list]=ip&filter[sg_asset_type]=Prop,Character,Environment&filter[shots.Shot.code]=bunny_010_0020
                       \
                    */
                    // requires authentication..

                    request(
                        http_,
                        infinite,
                        http_get_atom_v,
                        base_.scheme_host_port(),
                        std::string("/api/v1/entity/" + entity),
                        base_.get_auth_headers(),
                        params)
                        .then(
                            [=](const httplib::Response &response) mutable {
                                try {
                                    auto jsn = nlohmann::json::parse(response.body);

                                    try {
                                        if (not jsn["errors"][0]["status"].is_null() and
                                            jsn["errors"][0]["status"].get<int>() == 401) {
                                            // try and authorise..
                                            request(
                                                actor_cast<caf::actor>(this),
                                                infinite,
                                                shotgun_acquire_token_atom_v)
                                                .then(
                                                    [=](const std::pair<
                                                        std::string,
                                                        std::string>) mutable {
                                                        rp.delegate(
                                                            actor_cast<caf::actor>(this),
                                                            shotgun_entity_filter_atom_v,
                                                            entity,
                                                            filter,
                                                            fields,
                                                            sort,
                                                            page,
                                                            page_size);
                                                    },
                                                    [=](error &err) mutable {
                                                        spdlog::warn(
                                                            "{} {}",
                                                            __PRETTY_FUNCTION__,
                                                            to_string(err));
                                                        rp.deliver(JsonStore(std::move(jsn)));
                                                    });
                                            return;
                                        }

                                    } catch (const std::exception &err) {
                                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                    }
                                    rp.deliver(JsonStore(std::move(jsn)));
                                } catch (const std::exception &err) {
                                    rp.deliver(make_error(
                                        sce::response_error, err.what() + response.body));
                                }
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                } catch (const std::exception &err) {
                    return make_error(sce::args_error, err.what());
                }
            }
            return rp;
        },

        [=](shotgun_entity_search_atom atom,
            const std::string &entity,
            const JsonStore &conditions,
            const std::vector<std::string> &fields) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                entity,
                conditions,
                fields,
                std::vector<std::string>(),
                1,
                1000);
        },

        [=](shotgun_entity_search_atom,
            const std::string &entity,
            const JsonStore &conditions,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort,
            const int page,
            const int page_size) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            // spdlog::warn("shotgun_entity_search_atom");

            if (not base_.authenticated()) {
                request(actor_cast<caf::actor>(this), infinite, shotgun_acquire_token_atom_v)
                    .then(
                        [=](const std::pair<std::string, std::string>) mutable {
                            rp.delegate(
                                actor_cast<caf::actor>(this),
                                shotgun_entity_search_atom_v,
                                entity,
                                conditions,
                                fields,
                                sort,
                                page,
                                page_size);
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } else {
                auto jsn              = R"({"page":{}})"_json;
                jsn["page"]["size"]   = page_size;
                jsn["page"]["number"] = page;
                jsn["fields"]         = fields;
                if (not sort.empty())
                    jsn["sort"] = sort;
                jsn["filters"] = conditions;

                // spdlog::warn("{}", jsn.dump(2));

                // requires authentication..
                request(
                    http_,
                    infinite,
                    http_post_atom_v,
                    base_.scheme_host_port(),
                    std::string("/api/v1/entity/" + entity + "/_search"),
                    base_.get_auth_headers(),
                    jsn.dump(),
                    base_.content_type_hash())
                    .then(
                        [=](const httplib::Response &response) mutable {
                            try {
                                // validate / authentication error.
                                auto jsn = nlohmann::json::parse(response.body);

                                try {
                                    if (not jsn["errors"][0]["status"].is_null() and
                                        jsn["errors"][0]["status"].get<int>() == 401) {
                                        // try and authorise..
                                        request(
                                            actor_cast<caf::actor>(this),
                                            infinite,
                                            shotgun_acquire_token_atom_v)
                                            .then(
                                                [=](const std::pair<
                                                    std::string,
                                                    std::string>) mutable {
                                                    rp.delegate(
                                                        actor_cast<caf::actor>(this),
                                                        shotgun_entity_search_atom_v,
                                                        entity,
                                                        conditions,
                                                        fields,
                                                        sort,
                                                        page,
                                                        page_size);
                                                },
                                                [=](error &err) mutable {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                    rp.deliver(JsonStore(std::move(jsn)));
                                                });
                                        return;
                                    }

                                } catch (const std::exception &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                }
                                rp.deliver(JsonStore(std::move(jsn)));

                            } catch (const std::exception &err) {
                                rp.deliver(make_error(sce::response_error, err.what()));
                            }
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            }
            return rp;
        },

        [=](shotgun_host_atom) -> std::string { return base_.scheme_host_port(); },

        [=](shotgun_host_atom, const std::string &scheme_host_port) {
            base_.set_scheme_host_port(scheme_host_port);
        },

        [=](shotgun_credential_atom, const shotgun_client::AuthenticateShotgun &auth) {
            base_.set_credentials_method(auth);
        },

        [=](shotgun_info_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            // spdlog::warn("shotgun_info_atom");
            request(
                http_,
                infinite,
                http_get_simple_atom_v,
                base_.scheme_host_port(),
                "/api/v1/",
                base_.get_headers())
                .then(
                    [=](const std::string &body) mutable {
                        try {
                            rp.deliver(JsonStore(nlohmann::json::parse(body)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_preferences_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            // spdlog::warn("shotgun_preferences_atom");

            // requires authentication..
            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                "/api/v1/preferences",
                base_.get_auth_headers())
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_preferences_atom_v);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));

                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_groups_atom, const int project_id) {
            auto filter = FilterBy().And(
                StatusList("sg_status").is_not("Archive"),
                RelationType("sg_project").is(R"({"type": "Project", "id": 0})"_json));
            auto jfilter                      = JsonStore(static_cast<nlohmann::json>(filter));
            jfilter["conditions"][1][2]["id"] = project_id;

            delegate(
                actor_cast<caf::actor>(this),
                shotgun_entity_search_atom_v,
                "groups",
                jfilter,
                std::vector<std::string>({"code", "id"}),
                std::vector<std::string>({"code"}),
                1,
                4999);
        },

        [=](shotgun_projects_atom) {
            auto filter = FilterBy().And(
                StatusList("sg_status").is_not("Archive"), Text("sg_type").is_not("Template"));
            delegate(
                actor_cast<caf::actor>(this),
                shotgun_entity_search_atom_v,
                "projects",
                JsonStore(static_cast<nlohmann::json>(filter)),
                std::vector<std::string>({"name"}),
                std::vector<std::string>({"name"}),
                1,
                4999);
        },

        [=](shotgun_refresh_token_atom atom) {
            delegate(actor_cast<caf::actor>(this), atom, false);
        },

        [=](shotgun_refresh_token_atom,
            bool force) -> result<std::pair<std::string, std::string>> {
            // check we need one..
            if (not base_.token().empty() and not force and
                base_.token_lifetime() > std::chrono::seconds(30)) {
                return std::make_pair(base_.token(), base_.refresh_token());
            }

            auto rp = make_response_promise<std::pair<std::string, std::string>>();
            // spdlog::warn("REFRESH TOKEN");
            request(
                http_,
                infinite,
                http_post_simple_atom_v,
                base_.scheme_host_port(),
                "/api/v1/auth/access_token",
                base_.get_headers(),
                base_.get_auth_refresh_params())
                .then(
                    [=](const std::string &body) mutable {
                        try {
                            auto j = nlohmann::json::parse(body);
                            // should have valid token
                            base_.set_token(
                                j["token_type"],
                                j["access_token"],
                                j["refresh_token"],
                                j["expires_in"]);
                            // auto refresh
                            // 			delayed_anon_send(
                            // 	actor_cast<caf::actor>(this),
                            // 	std::chrono::seconds(1),
                            // 	shotgun_acquire_token_atom_v
                            // );
                            // spdlog::error("REFRESH TOKEN SUCCESS");
                            send(
                                event_group_,
                                utility::event_atom_v,
                                shotgun_acquire_token_atom_v,
                                std::make_pair(base_.token(), base_.refresh_token()));
                            rp.deliver(std::make_pair(base_.token(), base_.refresh_token()));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](shotgun_schema_atom atom) { delegate(actor_cast<caf::actor>(this), atom, -1); },

        [=](shotgun_schema_atom, const int id) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            // requires authentication..
            // spdlog::warn("shotgun_schema_atom");
            httplib::Params params;
            if (id != -1)
                params.insert(std::make_pair("project_id", std::to_string(id)));

            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                "/api/v1/schema",
                base_.get_auth_headers(),
                params)
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_schema_atom_v,
                                                    id);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn(
                                    "{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_schema_entity_atom atom, const std::string &entity) {
            delegate(actor_cast<caf::actor>(this), atom, entity, -1);
        },

        [=](shotgun_schema_entity_atom,
            const std::string &entity,
            const int id) -> result<JsonStore> {
            // spdlog::warn("shotgun_schema_entity_atom");
            auto rp = make_response_promise<JsonStore>();

            httplib::Params params;
            if (id != -1)
                params.insert(std::make_pair("project_id", std::to_string(id)));

            // requires authentication..
            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                "/api/v1/schema/" + entity,
                base_.get_auth_headers(),
                params)
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_schema_entity_atom_v,
                                                    id);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_schema_entity_fields_atom atom, const std::string &entity) {
            delegate(actor_cast<caf::actor>(this), atom, entity, "", -1);
        },

        [=](shotgun_schema_entity_fields_atom atom,
            const std::string &entity,
            const std::string &field) {
            delegate(actor_cast<caf::actor>(this), atom, entity, field, -1);
        },

        [=](shotgun_schema_entity_fields_atom atom, const std::string &entity, const int id) {
            delegate(actor_cast<caf::actor>(this), atom, entity, "", id);
        },

        [=](shotgun_schema_entity_fields_atom,
            const std::string &entity,
            const std::string &field,
            const int id) -> result<JsonStore> {
            // spdlog::warn("shotgun_schema_entity_fields_atom");
            auto rp = make_response_promise<JsonStore>();

            httplib::Params params;
            if (id != -1)
                params.insert(std::make_pair("project_id", std::to_string(id)));

            auto path = std::string("/api/v1/schema/" + entity + "/fields");
            if (not field.empty())
                path += "/" + field;

            // requires authentication..
            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                path,
                base_.get_auth_headers(),
                params)
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_schema_entity_fields_atom_v,
                                                    entity,
                                                    field,
                                                    id);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_text_search_atom atom,
            const std::string &text,
            const JsonStore &conditions) {
            delegate(actor_cast<caf::actor>(this), atom, text, conditions, 1, 49);
        },

        [=](shotgun_text_search_atom,
            const std::string &text,
            const JsonStore &conditions,
            const int page,
            const int page_size) -> result<JsonStore> {
            // spdlog::warn("shotgun_text_search_atom");
            auto rp               = make_response_promise<JsonStore>();
            auto jsn              = R"({"page":{},"entity_types":{}})"_json;
            jsn["text"]           = text;
            jsn["page"]["size"]   = page_size;
            jsn["page"]["number"] = page;
            jsn["entity_types"]   = conditions;

            // requires authentication..
            request(
                http_,
                infinite,
                http_post_atom_v,
                base_.scheme_host_port(),
                std::string("/api/v1/entity/_text_search"),
                base_.get_auth_headers(),
                jsn.dump(),
                base_.content_type_hash())
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);

                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_text_search_atom_v,
                                                    text,
                                                    conditions,
                                                    page,
                                                    page_size);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }

                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        // upload request
        [=](shotgun_upload_atom,
            const std::string &entity,
            const int record_id,
            const std::string &field,
            const std::string &name) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            httplib::Params params({{"filename", name}});
            auto url = std::string(fmt::format("/api/v1/entity/{}/{}/", entity, record_id));
            if (not field.empty())
                url += field + "/";
            url += "_upload";

            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                url,
                base_.get_auth_headers(),
                params)
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            auto jsn = nlohmann::json::parse(response.body);
                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_upload_atom_v,
                                                    entity,
                                                    record_id,
                                                    field,
                                                    name);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(JsonStore(std::move(jsn)));
                                            });
                                    return;
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                            rp.deliver(JsonStore(std::move(jsn)));
                        } catch (const std::exception &err) {
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(err);
                    });

            return rp;
        },

        // upload transfer
        [=](shotgun_upload_atom,
            const std::string &url,
            const std::string &content_type,
            const std::vector<std::byte> &data) -> result<JsonStore> {
            auto rp  = make_response_promise<JsonStore>();
            auto uri = caf::make_uri(url);
            if (uri) {
                auto host_port = to_string(*(uri->authority_only()));
                auto path      = url.substr(host_port.size());
                auto headers   = httplib::Headers({{"Content-Type", content_type}});

                request(
                    http_,
                    infinite,
                    http_put_atom_v,
                    host_port,
                    path,
                    headers,
                    std::string(reinterpret_cast<const char *>(data.data()), data.size()),
                    content_type)
                    .then(
                        [=](const httplib::Response &response) mutable {
                            try {
                                auto jsn = nlohmann::json::parse(response.body);
                                if (response.status != 200) {
                                    return rp.deliver(
                                        make_error(sce::response_error, jsn.dump(2)));
                                }
                                rp.deliver(jsn);
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                rp.deliver(make_error(sce::response_error, err.what()));
                            }
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.deliver(make_error(sce::response_error, "Invalid uri"));
            }

            return rp;
        },

        // upload finalise
        [=](shotgun_upload_atom,
            const std::string &path,
            const JsonStore &info) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            // spdlog::warn("{}", info.dump(2));

            request(
                http_,
                infinite,
                http_post_atom_v,
                base_.scheme_host_port(),
                path,
                base_.get_auth_headers(),
                info.dump(2),
                "application/json")
                .then(
                    [=](const httplib::Response &response) mutable {
                        try {
                            if (response.status != 201) {
                                spdlog::warn(
                                    "{} {} {}",
                                    __PRETTY_FUNCTION__,
                                    response.status,
                                    response.body);
                                return rp.deliver(
                                    make_error(sce::response_error, response.body));
                            }
                            auto jsn = JsonStore(R"({"status": "Success", "body": ""})"_json);
                            jsn["body"] = response.body;
                            rp.deliver(jsn);
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(std::move(err));
                    });

            return rp;
        },

        [=](shotgun_upload_atom,
            const std::string &entity,
            const int record_id,
            const std::string &field,
            const std::string &name,
            const std::vector<std::byte> &data,
            const std::string &content_type) -> result<bool> {
            // request upload url.
            auto rp = make_response_promise<bool>();
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                shotgun_upload_atom_v,
                entity,
                record_id,
                field,
                name)
                .then(
                    [=](const JsonStore &upload_req) mutable {
                        // should have destination for upload and completion url.
                        auto upload_link =
                            upload_req.at("links").at("upload").get<std::string>();
                        // trim prefix..
                        // upload..
                        // spdlog::warn("{}", upload_req.dump(2));
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            shotgun_upload_atom_v,
                            upload_link,
                            content_type,
                            data)
                            .then(
                                [=](const JsonStore &upload_resp) mutable {
                                    // if good...
                                    // spdlog::warn("{}", upload_resp.dump(2));

                                    auto info                           = JsonStore();
                                    info["upload_info"]                 = upload_req.at("data");
                                    info["upload_data"]["display_name"] = name;
                                    if (upload_req["data"]["storage_service"] == "sg")
                                        info["upload_info"]["upload_id"] =
                                            upload_resp["data"]["upload_id"];

                                    // finalise upload..
                                    request(
                                        caf::actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_upload_atom_v,
                                        upload_resp.at("links")
                                            .at("complete_upload")
                                            .get<std::string>(),
                                        info)
                                        .then(
                                            [=](const JsonStore &final) mutable {
                                                // spdlog::warn("{}", final.dump(2));
                                                rp.deliver(true);
                                            },
                                            [=](error &err) mutable {
                                                rp.deliver(std::move(err));
                                            });
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](shotgun_attachment_atom,
            const std::string &entity,
            const int id,
            const std::string &property) -> result<std::string> {
            auto rp = make_response_promise<std::string>();

            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                std::string(
                    fmt::format("/api/v1/entity/{}/{}/{}?alt=original", entity, id, property)),
                base_.get_auth_headers())
                // base_.get_auth_headers("video/webm"))
                .then(
                    [=](const httplib::Response &response) mutable {
                        if (response.status == 200) {
                            return rp.deliver(response.body);
                        }

                        try {
                            auto jsn = nlohmann::json::parse(response.body);
                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_attachment_atom_v,
                                                    entity,
                                                    id,
                                                    property);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(std::move(err));
                                            });
                                } else {
                                    // missing thumbnail
                                    rp.deliver(make_error(sce::response_error, response.body));
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                rp.deliver(make_error(sce::response_error, err.what()));
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(std::move(err));
                    });

            return rp;
        },

        [=](shotgun_image_atom,
            const std::string &entity,
            const int record_id,
            const bool thumbnail) -> result<std::string> {
            auto rp = make_response_promise<std::string>();

            request(
                http_,
                infinite,
                http_get_atom_v,
                base_.scheme_host_port(),
                std::string(fmt::format(
                    "/api/v1/entity/{}/{}/image?alt={}",
                    entity,
                    record_id,
                    (thumbnail ? "thumbnail" : "original"))),
                base_.get_auth_headers())
                .then(
                    [=](const httplib::Response &response) mutable {
                        if (response.status == 200)
                            return rp.deliver(response.body);

                        try {
                            auto jsn = nlohmann::json::parse(response.body);
                            try {
                                if (not jsn["errors"][0]["status"].is_null() and
                                    jsn["errors"][0]["status"].get<int>() == 401) {
                                    // try and authorise..
                                    request(
                                        actor_cast<caf::actor>(this),
                                        infinite,
                                        shotgun_acquire_token_atom_v)
                                        .then(
                                            [=](const std::
                                                    pair<std::string, std::string>) mutable {
                                                rp.delegate(
                                                    actor_cast<caf::actor>(this),
                                                    shotgun_image_atom_v,
                                                    entity,
                                                    record_id,
                                                    thumbnail);
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                rp.deliver(std::move(err));
                                            });
                                } else {
                                    // missing thumbnail
                                    rp.deliver(make_error(sce::response_error, response.body));
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                rp.deliver(make_error(sce::response_error, err.what()));
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            rp.deliver(make_error(sce::response_error, err.what()));
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(std::move(err));
                    });

            return rp;
        },

        [=](shotgun_image_atom,
            const std::string &entity,
            const int record_id,
            const bool thumbnail,
            const bool as_buffer) -> result<thumbnail::ThumbnailBufferPtr> {
            auto rp = make_response_promise<thumbnail::ThumbnailBufferPtr>();

            request(
                actor_cast<caf::actor>(this),
                infinite,
                shotgun_image_atom_v,
                entity,
                record_id,
                thumbnail)
                .then(
                    [=](const std::string &data) mutable {
                        // request conversion..
                        auto thumbgen = system().registry().template get<caf::actor>(
                            thumbnail_manager_registry);
                        if (thumbgen) {
                            std::vector<std::byte> bytedata(data.size());
                            std::memcpy(bytedata.data(), data.data(), data.size());
                            rp.delegate(thumbgen, media_reader::get_thumbnail_atom_v, bytedata);
                        } else {
                            rp.deliver(make_error(
                                sce::response_error, "Thumbnail manager not available"));
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        });
}

void ShotgunClientActor::acquire_token(
    caf::typed_response_promise<std::pair<std::string, std::string>> rp) {
    request_acquire_queue_.push(rp);
    if (request_acquire_queue_.size() > 1)
        return;

    // spdlog::warn("acquire_token");
    request(
        http_,
        infinite,
        http_post_simple_atom_v,
        base_.scheme_host_port(),
        "/api/v1/auth/access_token",
        base_.get_headers(),
        base_.get_auth_request_params())
        .then(
            [=](const std::string &body) mutable {
                try {
                    auto j = nlohmann::json::parse(body);
                    // should have valid token
                    base_.set_token(
                        j["token_type"],
                        j["access_token"],
                        j["refresh_token"],
                        j["expires_in"]);
                    base_.set_authenticated(true);
                    send(
                        event_group_,
                        utility::event_atom_v,
                        shotgun_acquire_token_atom_v,
                        std::make_pair(base_.token(), base_.refresh_token()));

                    while (not request_acquire_queue_.empty()) {
                        request_acquire_queue_.front().deliver(
                            std::make_pair(base_.token(), base_.refresh_token()));
                        request_acquire_queue_.pop();
                    }
                } catch (const std::exception &err) {
                    // spurious result ?
                    while (not request_acquire_queue_.empty()) {
                        request_acquire_queue_.front().deliver(
                            make_error(sce::response_error, err.what()));
                        request_acquire_queue_.pop();
                    }
                    base_.set_authenticated(false);
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                base_.set_failed_authentication();
                base_.set_authenticated(false);
                while (not request_acquire_queue_.empty()) {
                    request_acquire_queue_.front().delegate(
                        actor_cast<caf::actor>(this), shotgun_acquire_token_atom_v);
                    request_acquire_queue_.pop();
                }
            });
}
