// SPDX-License-Identifier: Apache-2.0
#include <caf/sec.hpp>
#include <caf/policy/select_all.hpp>
#include <caf/policy/select_any.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/media_hook/media_hook_actor.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::global_store;
using namespace xstudio::media_hook;
using namespace caf;

MediaHookWorkerActor::MediaHookWorkerActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {

    std::vector<caf::actor> hooks;

    // get hooks
    {
        auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
        scoped_actor sys{system()};
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_HOOK));

        for (const auto &i : details) {
            if (i.enabled_) {
                auto actor = request_receive<caf::actor>(
                    *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_);
                link_to(actor);
                hooks.push_back(actor);
            }
        }
    }

    // distribute to all hooks.

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](gather_media_sources_atom,
            const caf::actor &media_actor,
            const FrameRate &media_rate,
            const std::vector<MediaReference> &source_refs,
            std::vector<std::string> &source_names) -> result<UuidActorVector> {
            if (not hooks.empty()) {
                auto rp = make_response_promise<UuidActorVector>();
                fan_out_request<policy::select_all>(
                    hooks,
                    infinite,
                    media_hook::gather_media_sources_atom_v,
                    media_actor,
                    source_refs,
                    source_names)
                    .then(
                        [=](const std::vector<std::vector<
                                std::tuple<std::string, caf::uri, xstudio::utility::FrameList>>>
                                results) mutable {
                            auto new_sources = UuidActorVector();
                            for (const auto &i : results) {
                                for (const auto &[name, uri, fl] : i) {
                                    auto source_uuid = utility::Uuid::generate();
                                    auto source =
                                        fl.empty()
                                            ? spawn<media::MediaSourceActor>(
                                                  name, uri, media_rate, source_uuid)
                                            : spawn<media::MediaSourceActor>(
                                                  name, uri, fl, media_rate, source_uuid);
                                    new_sources.emplace_back(UuidActor(source_uuid, source));
                                }
                            }

                            rp.deliver(new_sources);
                        },
                        [=](const error &err) mutable { rp.deliver(err); });
                return rp;
            }

            return UuidActorVector();
        },

        [=](get_media_hook_atom, caf::actor media_source) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (hooks.empty()) {
                rp.deliver(true);
                return rp;
            }

            request(media_source, infinite, json_store::get_json_atom_v, "")
                .then(
                    [=](const JsonStore &jsn) mutable {
                        request(media_source, infinite, media::media_reference_atom_v, Uuid())
                            .then(
                                [=](const std::pair<Uuid, MediaReference> &ref) mutable {
                                    // dispatch request to hooks..
                                    // merge collected metadata.
                                    const auto &[uuid, mr] = ref;

                                    fan_out_request<policy::select_all>(
                                        hooks,
                                        infinite,
                                        get_media_hook_atom_v,
                                        UuidActor(uuid, media_source),
                                        mr,
                                        jsn)
                                        .then(
                                            [=](std::vector<utility::JsonStore> cr) mutable {
                                                // merge Stores.
                                                JsonStore c(nlohmann::json::object());
                                                for (const auto &i : cr) {
                                                    if (not i.is_null())
                                                        c.update(i);
                                                }

                                                // push to source.
                                                if (not c.is_null()) {
                                                    anon_send(
                                                        media_source,
                                                        json_store::merge_json_atom_v,
                                                        c);
                                                    if (c.count("colour_pipeline")) {
                                                        anon_send(
                                                            media_source,
                                                            colour_pipeline::
                                                                set_colour_pipe_params_atom_v,
                                                            utility::JsonStore(
                                                                c["colour_pipeline"]));
                                                    }
                                                }

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

        [=](detect_display_atom,
            const std::string &name,
            const std::string &model,
            const std::string &manufacturer,
            const std::string &serialNumber,
            const utility::JsonStore &jsn) -> result<std::string> {
            auto rp = make_response_promise<std::string>();

            if (not hooks.empty()) {
                fan_out_request<policy::select_any>(
                    hooks,
                    infinite,
                    media_hook::detect_display_atom_v,
                    name,
                    model,
                    manufacturer,
                    serialNumber,
                    jsn)
                    .then(
                        [=](const std::string &display) mutable { rp.deliver(display); },
                        [=](const error &err) mutable { rp.deliver(err); });
                return rp;
            }

            return rp;
        });
}

GlobalMediaHookActor::GlobalMediaHookActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {
    size_t worker_count = 5;
    spdlog::debug("Created GlobalMediaHookActor.");
    print_on_exit(this, "GlobalMediaHookActor");

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        worker_count = preference_value<size_t>(j, "/core/media_hook/max_worker_count");
    } catch (...) {
    }

    spdlog::debug("GlobalMediaHookActor worker_count {}", worker_count);

    auto pool = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count,
        [&] { return system().spawn<MediaHookWorkerActor>(); },
        caf::actor_pool::round_robin());
    link_to(pool);

    system().registry().put(media_hook_registry, this);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // [=](gather_media_sources_atom, const std::vector<MediaReference> &existing_refs,
        // std::vector<std::string> &source_names, caf::actor media_actor)
        // {
        //     delegate(pool, gather_media_sources_atom_v, existing_refs, source_names,
        //     media_actor);
        // },

        [=](utility::serialise_atom) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();
            // this makes a dict of the media hook plugin(s) uuid vs versions
            // which lets us know if we need to re-reun media hook plugins
            auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
            request(
                pm,
                infinite,
                utility::detail_atom_v,
                plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_HOOK))
                .then(
                    [=](const std::vector<plugin_manager::PluginDetail> &details) mutable {
                        utility::JsonStore result;
                        for (const auto &i : details) {
                            if (i.enabled_) {
                                result[to_string(i.uuid_)] = i.version_.to_string();
                            }
                        }
                        rp.deliver(result);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](check_media_hook_plugin_versions_atom,
            const utility::JsonStore &ref_hook_versions) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // this makes a dict of the media hook plugin(s) uuid vs versions
            // which lets us know if we need to re-reun media hook plugins
            auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
            request(
                pm,
                infinite,
                utility::detail_atom_v,
                plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_HOOK))
                .then(
                    [=](const std::vector<plugin_manager::PluginDetail> &details) mutable {
                        bool matched = true;
                        for (const auto &i : details) {
                            if (i.enabled_) {
                                const std::string plugin_uuid = to_string(i.uuid_);
                                if (ref_hook_versions.contains(plugin_uuid) &&
                                    ref_hook_versions[plugin_uuid].get<std::string>() ==
                                        i.version_.to_string()) {
                                } else {
                                    matched = false;
                                }
                            }
                        }
                        rp.deliver(matched);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](gather_media_sources_atom,
            const caf::actor &media,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            // try to hide actor stuff..
            auto rp = make_response_promise<UuidActorVector>();
            request(media, infinite, media::media_reference_atom_v)
                .then(
                    [=](const std::vector<MediaReference> &existing_refs) mutable {
                        // get media references
                        request(media, infinite, media::get_media_source_names_atom_v)
                            .then(
                                [=](const std::vector<std::pair<utility::Uuid, std::string>>
                                        &source_names) mutable {
                                    // got refs and names.
                                    // fanout to all hooks..
                                    rp.delegate(
                                        pool,
                                        media_hook::gather_media_sources_atom_v,
                                        media,
                                        media_rate,
                                        existing_refs,
                                        vpair_second_to_v(source_names));
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_hook_atom _get_media_hook_atom, caf::actor media_source) {
            delegate(pool, _get_media_hook_atom, media_source);
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                auto count = preference_value<size_t>(j, "/core/media_hook/max_worker_count");
                if (count > worker_count) {
                    spdlog::debug("hook workers changed old {} new {}", worker_count, count);
                    while (worker_count < count) {
                        anon_send(
                            pool,
                            sys_atom_v,
                            put_atom_v,
                            system().spawn<MediaHookWorkerActor>());
                        worker_count++;
                    }
                } else if (count < worker_count) {
                    spdlog::debug("hook workers changed old {} new {}", worker_count, count);
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
        },

        [=](detect_display_atom,
            const std::string &name,
            const std::string &model,
            const std::string &manufacturer,
            const std::string &serialNumber,
            const utility::JsonStore &jsn) -> result<std::string> {
            auto rp = make_response_promise<std::string>();

            rp.delegate(
                pool,
                media_hook::detect_display_atom_v,
                name,
                model,
                manufacturer,
                serialNumber,
                jsn);

            return rp;
        });
}

void GlobalMediaHookActor::on_exit() { system().registry().erase(media_hook_registry); }
