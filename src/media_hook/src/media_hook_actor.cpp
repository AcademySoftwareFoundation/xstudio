// SPDX-License-Identifier: Apache-2.0
#include <caf/sec.hpp>
#include <caf/policy/select_all.hpp>
#include <caf/policy/select_any.hpp>
#include <caf/actor_registry.hpp>

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
                hooks_.push_back(actor);
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
            if (not hooks_.empty()) {
                auto rp = make_response_promise<UuidActorVector>();
                fan_out_request<policy::select_all>(
                    hooks_,
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

        [=](get_clip_hook_atom, const caf::actor &clip, int retry_count) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (hooks_.empty()) {
                rp.deliver(true);
            } else {
                // we get the clip prop not the clip "metadata", though we might want both ?

                // get clip meta..
                mail(timeline::item_prop_atom_v)
                    .request(clip, infinite)
                    .then(
                        [=](const JsonStore &clip_meta) mutable {
                            // get media meta
                            mail(playlist::get_media_atom_v, get_json_atom_v, Uuid(), "")
                                .request(clip, infinite)
                                .then(
                                    [=](const JsonStore &media_meta) mutable {
                                        if (media_meta.is_null() and retry_count) {
                                            // try again later..
                                            mail(get_clip_hook_atom_v, clip, retry_count - 1)
                                                .delay(2s)
                                                .request(
                                                    caf::actor_cast<caf::actor>(this), infinite)
                                                .then(
                                                    [=](const bool result) mutable {
                                                        rp.deliver(result);
                                                    },
                                                    [=](error &err) mutable {
                                                        rp.deliver(std::move(err));
                                                    });
                                        } else
                                            do_clip_hook(rp, clip, clip_meta, media_meta);
                                    },
                                    [=](const error &) mutable {
                                        // error is valid if no media assiged
                                        do_clip_hook(rp, clip, clip_meta, JsonStore());
                                    });
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            }

            return rp;
        },


        // update clip metadat when media changes.
        [=](get_clip_hook_atom, const caf::actor &clip) -> result<bool> {
            auto rp = make_response_promise<bool>();
            rp.delegate(caf::actor_cast<caf::actor>(this), get_clip_hook_atom_v, clip, 5);
            return rp;
        },

        [=](get_media_hook_atom, caf::actor media_source) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (hooks_.empty()) {
                rp.deliver(true);
                return rp;
            }

            mail(json_store::get_json_atom_v, "")
                .request(media_source, infinite)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        mail(media::media_reference_atom_v, Uuid())
                            .request(media_source, infinite)
                            .then(
                                [=](const std::pair<Uuid, MediaReference> &ref) mutable {
                                    // dispatch request to hooks_..
                                    // merge collected metadata.
                                    const auto &[uuid, mr] = ref;

                                    fan_out_request<policy::select_all>(
                                        hooks_,
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
                                                    anon_mail(json_store::merge_json_atom_v, c)
                                                        .send(media_source);
                                                    if (c.count("colour_pipeline")) {
                                                        anon_mail(
                                                            colour_pipeline::
                                                                set_colour_pipe_params_atom_v,
                                                            utility::JsonStore(
                                                                c["colour_pipeline"]))
                                                            .send(media_source);
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

            if (not hooks_.empty()) {
                fan_out_request<policy::select_any>(
                    hooks_,
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

void MediaHookWorkerActor::do_clip_hook(
    caf::typed_response_promise<bool> rp,
    const caf::actor &clip_actor,
    const utility::JsonStore &clip_meta,
    const utility::JsonStore &media_meta) {

    fan_out_request<policy::select_all>(
        hooks_, infinite, get_clip_hook_atom_v, clip_meta, media_meta)
        .then(
            [=](std::vector<utility::JsonStore> cr) mutable {
                // merge Stores.
                JsonStore new_clip_meta(clip_meta);
                for (const auto &i : cr) {
                    if (not i.is_null())
                        new_clip_meta.update(i);
                }

                // push to source.
                if (not new_clip_meta.is_null()) {
                    anon_mail(timeline::item_prop_atom_v, new_clip_meta, true).send(clip_actor);
                }

                rp.deliver(true);
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    auto pool = caf::actor_pool::make(
        system(),
        worker_count,
        [&] { return system().spawn<MediaHookWorkerActor>(); },
        caf::actor_pool::round_robin());

#pragma GCC diagnostic pop

    link_to(pool);

    system().registry().put(media_hook_registry, this);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // [=](gather_media_sources_atom, const std::vector<MediaReference> &existing_refs,
        // std::vector<std::string> &source_names, caf::actor media_actor)
        // {
        //     mail(gather_media_sources_atom_v, existing_refs, source_names,
        //     media_actor).delegate(pool);
        // },

        [=](utility::serialise_atom) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();
            // this makes a dict of the media hook plugin(s) uuid vs versions
            // which lets us know if we need to re-reun media hook plugins
            auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
            mail(
                utility::detail_atom_v,
                plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_HOOK))
                .request(pm, infinite)
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
            mail(
                utility::detail_atom_v,
                plugin_manager::PluginType(plugin_manager::PluginFlags::PF_MEDIA_HOOK))
                .request(pm, infinite)
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
            mail(media::media_reference_atom_v)
                .request(media, infinite)
                .then(
                    [=](const std::vector<MediaReference> &existing_refs) mutable {
                        // get media references
                        mail(media::get_media_source_names_atom_v)
                            .request(media, infinite)
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
            return mail(_get_media_hook_atom, media_source).delegate(pool);
        },

        [=](get_clip_hook_atom _atom, const caf::actor &clip_actor) {
            return mail(_atom, clip_actor).delegate(pool);
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            return mail(json_store::update_atom_v, full).delegate(actor_cast<caf::actor>(this));
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                auto count = preference_value<size_t>(j, "/core/media_hook/max_worker_count");
                if (count > worker_count) {
                    spdlog::debug("hook workers changed old {} new {}", worker_count, count);
                    while (worker_count < count) {
                        anon_mail(
                            sys_atom_v, put_atom_v, system().spawn<MediaHookWorkerActor>())
                            .send(pool);
                        worker_count++;
                    }
                } else if (count < worker_count) {
                    spdlog::debug("hook workers changed old {} new {}", worker_count, count);
                    // get actors..
                    worker_count = count;
                    mail(sys_atom_v, get_atom_v)
                        .request(pool, infinite)
                        .await(
                            [=](const std::vector<actor> &ws) {
                                for (auto i = worker_count; i < ws.size(); i++) {
                                    anon_mail(sys_atom_v, delete_atom_v, ws[i]).send(pool);
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
