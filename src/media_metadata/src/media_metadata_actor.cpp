// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <caf/sec.hpp>
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_metadata/media_metadata_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::global_store;
using namespace xstudio::media_metadata;
using namespace caf;

MediaMetadataWorkerActor::MediaMetadataWorkerActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {

    auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
    // get plugins
    {
        scoped_actor sys{system()};
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_METADATA));

        join_event_group(this, pm);

        for (const auto &i : details) {
            if (i.enabled_) {
                auto actor = request_receive<caf::actor>(
                    *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_);
                link_to(actor);
                plugins_.push_back(actor);
                name_plugin_[i.name_] = actor;
            }
        }
    }

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](utility::event_atom,
            utility::detail_atom,
            const std::vector<plugin_manager::PluginDetail> &detail) {
            for (const auto &i : detail) {
                if (i.type_ & plugin_manager::PluginFlags::PF_MEDIA_METADATA) {
                    if (not i.enabled_ and name_plugin_.count(i.name_)) {
                        // plugin has been disabled.
                        auto plugin = name_plugin_[i.name_];
                        name_plugin_.erase(i.name_);
                        plugins_.erase(std::find(plugins_.begin(), plugins_.end(), plugin));

                        unlink_from(plugin);

                        send_exit(plugin, caf::exit_reason::user_shutdown);

                    } else if (i.enabled_ and not name_plugin_.count(i.name_)) {
                        scoped_actor sys{system()};
                        auto actor = request_receive<caf::actor>(
                            *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_);
                        link_to(actor);
                        plugins_.push_back(actor);
                        name_plugin_[i.name_] = actor;
                    }
                }
            }
        },

        [=](get_metadata_atom atom,
            const caf::uri &_uri,
            const int frame) -> result<std::pair<JsonStore, int>> {
            auto rp = make_response_promise<std::pair<JsonStore, int>>();

            if (plugins_.empty())
                return make_error(xstudio_error::error, "Unsupported format");
            // supported ?
            // fanout and find best match.
            try {
                fan_out_request<policy::select_all>(
                    plugins_,
                    infinite,
                    media_reader::supported_atom_v,
                    _uri,
                    utility::get_signature(_uri))
                    .then(
                        [=](std::vector<std::pair<std::string, MMCertainty>> supt) mutable {
                            // find best match or fail..
                            std::string selected;
                            MMCertainty best_match = MMC_NO;

                            for (const auto &i : supt) {
                                if (i.second > best_match) {
                                    best_match = i.second;
                                    selected   = i.first;
                                }
                            }

                            if (best_match == MMC_NO) {
                                rp.deliver(
                                    make_error(xstudio_error::error, "Unsupported format"));
                            } else {
                                rp.delegate(name_plugin_.at(selected), atom, _uri, frame);
                            }
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } catch (std::exception &e) {
                rp.deliver(make_error(xstudio_error::error, e.what()));
            }

            return rp;
        },

        [=](get_metadata_atom atom,
            const caf::uri &_uri,
            const int frame,
            const std::string &force_plugin) -> result<std::pair<JsonStore, int>> {
            auto rp = make_response_promise<std::pair<JsonStore, int>>();

            if (force_plugin.empty()) {
                rp.delegate(actor_cast<caf::actor>(this), atom, _uri, frame);
            } else {
                // check we've got plugin..
                if (not name_plugin_.count(force_plugin))
                    return make_error(xstudio_error::error, "Invalid plugin");

                rp.delegate(name_plugin_.at(force_plugin), atom, _uri, frame);
            }

            return rp;
        });
}

GlobalMediaMetadataActor::GlobalMediaMetadataActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {
    size_t worker_count = 10;
    spdlog::debug("Created GlobalMediaMetadataActor.");
    print_on_exit(this, "GlobalMediaMetadataActor");

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        worker_count = preference_value<size_t>(j, "/core/media_metadata/max_worker_count");
    } catch (...) {
    }

    auto pool = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count,
        [&] { return system().spawn<MediaMetadataWorkerActor>(); },
        caf::actor_pool::round_robin());
    link_to(pool);

    system().registry().put(media_metadata_registry, this);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](get_metadata_atom _get_metadata_atom, const caf::uri &_uri) {
            delegate(pool, _get_metadata_atom, _uri, std::numeric_limits<int>::min());
        },

        [=](get_metadata_atom _get_metadata_atom,
            const caf::uri &_uri,
            const std::string &force_plugin) {
            delegate(
                pool, _get_metadata_atom, _uri, std::numeric_limits<int>::min(), force_plugin);
        },

        [=](get_metadata_atom _get_metadata_atom, const caf::uri &_uri, const int frame) {
            delegate(pool, _get_metadata_atom, _uri, frame);
        },

        [=](get_metadata_atom _get_metadata_atom,
            const caf::uri &_uri,
            const int frame,
            const std::string &force_plugin) {
            delegate(pool, _get_metadata_atom, _uri, frame, force_plugin);
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                auto count =
                    preference_value<size_t>(j, "/core/media_metadata/max_worker_count");

                if (count > worker_count) {
                    spdlog::debug(
                        "GlobalMediaMetadataActor worker_count changed old {} new {}",
                        worker_count,
                        count);
                    while (worker_count < count) {
                        anon_send(
                            pool,
                            sys_atom_v,
                            put_atom_v,
                            system().spawn<MediaMetadataWorkerActor>());
                        worker_count++;
                    }
                } else if (count < worker_count) {
                    // get actors..
                    spdlog::debug(
                        "GlobalMediaMetadataActor worker_count changed old {} new {}",
                        worker_count,
                        count);
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

void GlobalMediaMetadataActor::on_exit() { system().registry().erase(media_metadata_registry); }
