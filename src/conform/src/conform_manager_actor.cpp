// SPDX-License-Identifier: Apache-2.0
#include <caf/sec.hpp>
#include <caf/policy/select_all.hpp>
#include <caf/policy/select_any.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/conform/conformer.hpp"
#include "xstudio/conform/conform_manager_actor.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::global_store;
using namespace xstudio::conform;
using namespace caf;

ConformWorkerActor::ConformWorkerActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    std::vector<caf::actor> conformers;

    // get hooks
    {
        auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
        scoped_actor sys{system()};
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_CONFORM));

        for (const auto &i : details) {
            if (i.enabled_) {
                auto actor = request_receive<caf::actor>(
                    *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_);
                link_to(actor);
                conformers.push_back(actor);
            }
        }
    }

    // distribute to all conformers.

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](conform_tasks_atom) -> result<std::vector<std::string>> {
            if (not conformers.empty()) {
                auto rp = make_response_promise<std::vector<std::string>>();
                fan_out_request<policy::select_all>(conformers, infinite, conform_tasks_atom_v)
                    .then(
                        [=](const std::vector<std::vector<std::string>> all_results) mutable {
                            // compile results..
                            auto results = std::set<std::string>();

                            for (const auto &i : all_results) {
                                for (const auto &j : i) {
                                    results.insert(j);
                                }
                            }

                            rp.deliver(
                                std::vector<std::string>(results.begin(), results.end()));
                        },
                        [=](const error &err) mutable { rp.deliver(err); });
                return rp;
            }

            return std::vector<std::string>();
        },

        [=](conform_atom,
            const std::string &conform_task,
            const utility::JsonStore &conform_detail,
            const UuidActor &playlist,
            const UuidActorVector &media) -> result<ConformReply> {
            // make worker gather all the information
            auto rp = make_response_promise<ConformReply>();

            request(playlist.actor(), infinite, json_store::get_json_atom_v, "")
                .then(
                    [=](const JsonStore &playlist_json) mutable {
                        // get all media json..
                        fan_out_request<policy::select_all>(
                            vector_to_caf_actor_vector(media),
                            infinite,
                            json_store::get_json_atom_v,
                            utility::Uuid(),
                            "",
                            true)
                            .then(
                                [=](const std::vector<std::pair<UuidActor, JsonStore>>
                                        media_json_reply) mutable {
                                    // reorder into Conform request.
                                    auto media_json =
                                        std::vector<std::tuple<utility::JsonStore>>();
                                    std::map<Uuid, JsonStore> jsn_map;
                                    for (const auto &i : media_json_reply)
                                        jsn_map[i.first.uuid()] = i.second;

                                    for (const auto &i : media)
                                        media_json.emplace_back(
                                            std::make_tuple(jsn_map.at(i.uuid())));

                                    rp.delegate(
                                        caf::actor_cast<caf::actor>(this),
                                        conform_atom_v,
                                        conform_task,
                                        conform_detail,
                                        ConformRequest(playlist, playlist_json, media_json));
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](conform_atom,
            const std::string &conform_task,
            const utility::JsonStore &conform_detail,
            const ConformRequest &request) -> result<ConformReply> {
            if (not conformers.empty()) {
                auto rp = make_response_promise<ConformReply>();
                fan_out_request<policy::select_all>(
                    conformers, infinite, conform_atom_v, conform_task, conform_detail, request)
                    .then(
                        [=](const std::vector<ConformReply> all_results) mutable {
                            // compile results..
                            auto result = ConformReply();
                            result.items_.resize(request.items_.size());

                            for (const auto &i : all_results) {
                                if (not i.items_.empty()) {
                                    // insert values into result.
                                    auto count = 0;
                                    for (const auto &j : i.items_) {
                                        // replace, don't sum results, so we only expect one
                                        // result set in total from a plugin.
                                        if (j and not result.items_[count])
                                            result.items_[count] = j;
                                        count++;
                                    }
                                }
                            }

                            rp.deliver(result);
                        },
                        [=](const error &err) mutable { rp.deliver(err); });
                return rp;
            }

            return ConformReply();
        });
}

ConformManagerActor::ConformManagerActor(caf::actor_config &cfg, const utility::Uuid uuid)
    : caf::event_based_actor(cfg), uuid_(std::move(uuid)) {
    size_t worker_count = 5;
    spdlog::debug("Created ConformManagerActor.");
    print_on_exit(this, "ConformManagerActor");

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        worker_count = preference_value<size_t>(j, "/core/conform/max_worker_count");
    } catch (...) {
    }

    spdlog::debug("ConformManagerActor worker_count {}", worker_count);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    auto pool = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count,
        [&] { return system().spawn<ConformWorkerActor>(); },
        caf::actor_pool::round_robin());
    link_to(pool);

    system().registry().put(conform_registry, this);

    behavior_.assign(
        make_get_event_group_handler(event_group_),
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](conform_atom,
            const std::string &conform_task,
            const utility::JsonStore &conform_detail,
            const ConformRequest &request) {
            delegate(pool, conform_atom_v, conform_task, conform_detail, request);
        },

        [=](conform_atom,
            const std::string &conform_task,
            const utility::JsonStore &conform_detail,
            const UuidActor &playlist,
            const UuidActorVector &media) {
            delegate(pool, conform_atom_v, conform_task, conform_detail, playlist, media);
        },

        [=](conform_tasks_atom) -> result<std::vector<std::string>> {
            auto rp = make_response_promise<std::vector<std::string>>();

            request(pool, infinite, conform_tasks_atom_v)
                .then(
                    [=](const std::vector<std::string> &result) mutable {
                        if (tasks_ != result) {
                            tasks_ = result;
                            send(
                                event_group_,
                                utility::event_atom_v,
                                conform_tasks_atom_v,
                                tasks_);
                        }
                        rp.deliver(tasks_);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                auto count = preference_value<size_t>(j, "/core/conform/max_worker_count");
                if (count > worker_count) {
                    spdlog::debug("conform workers changed old {} new {}", worker_count, count);
                    while (worker_count < count) {
                        anon_send(
                            pool, sys_atom_v, put_atom_v, system().spawn<ConformWorkerActor>());
                        worker_count++;
                    }
                } else if (count < worker_count) {
                    spdlog::debug("conform workers changed old {} new {}", worker_count, count);
                    // get actors..
                    worker_count = count;
                    request(pool, infinite, sys_atom_v, get_atom_v)
                        .await(
                            [=](const std::vector<actor> &ws) {
                                for (auto i = worker_count; i < ws.size(); i++) {
                                    anon_send(pool, sys_atom_v, delete_atom_v, ws[i]);
                                }
                            },
                            [=](const error &err) {
                                throw std::runtime_error(
                                    "Failed to find pool " + to_string(err));
                            });
                }
            } catch (...) {
            }
        });
}

void ConformManagerActor::on_exit() { system().registry().erase(conform_registry); }
