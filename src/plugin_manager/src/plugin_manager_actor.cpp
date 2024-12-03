// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/plugin_manager/plugin_manager_actor.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/plugin_manager/hud_plugin.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::plugin_manager;
using namespace nlohmann;
using namespace caf;

PluginManagerActor::PluginManagerActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    print_on_exit(this, "PluginManagerActor");

    system().registry().put(plugin_manager_registry, this);

    manager_.emplace_front_path(xstudio_root("/plugin"));

    // use env var 'XSTUDIO_PLUGIN_PATH' to extend the folders searched for
    // xstudio plugins
    char *plugin_path = std::getenv("XSTUDIO_PLUGIN_PATH");
    if (plugin_path) {
        for (const auto &p : xstudio::utility::split(plugin_path, ':')) {
            manager_.emplace_front_path(p);
        }
    }

    manager_.load_plugins();

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        join_broadcast(this, prefs.get_group(js));
        update_from_preferences(js);
    } catch (...) {
    }

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        // helper for dealing with URI's
        [=](data_source::use_data_atom,
            const caf::uri &uri,
            const FrameRate &media_rate,
            const bool create_playlist) -> result<UuidActorVector> {
            // send to resident enabled datasource plugins
            auto actors = std::vector<caf::actor>();

            for (const auto &i : manager_.factories()) {
                if (i.second.factory()->type() & PluginFlags::PF_DATA_SOURCE and
                    resident_.count(i.first))
                    actors.push_back(resident_[i.first]);
            }

            if (actors.empty())
                return UuidActorVector();

            auto rp = make_response_promise<UuidActorVector>();

            fan_out_request<policy::select_all>(
                actors,
                infinite,
                data_source::use_data_atom_v,
                uri,
                media_rate,
                create_playlist)
                .then(
                    [=](const std::vector<UuidActorVector> results) mutable {
                        for (const auto &i : results) {
                            if (not i.empty())
                                return rp.deliver(i);
                        }
                        rp.deliver(UuidActorVector());
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        // helper for dealing with Media sources back population's
        [=](data_source::use_data_atom,
            const caf::actor &media,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            // send to resident enabled datasource plugins
            auto actors = std::vector<caf::actor>();

            for (const auto &i : manager_.factories()) {
                if (i.second.factory()->type() & PluginFlags::PF_DATA_SOURCE and
                    resident_.count(i.first))
                    actors.push_back(resident_[i.first]);
            }

            if (actors.empty())
                return UuidActorVector();

            auto rp = make_response_promise<UuidActorVector>();

            fan_out_request<policy::select_all>(
                actors, infinite, data_source::use_data_atom_v, media, media_rate)
                .then(
                    [=](const std::vector<UuidActorVector> results) mutable {
                        auto result = UuidActorVector();
                        for (const auto &i : results)
                            result.insert(result.end(), i.begin(), i.end());

                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        // helper for dealing with URI's
        [=](data_source::use_data_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate,
            const bool drop) -> result<UuidActorVector> {
            // send to resident enabled datasource plugins
            auto actors = std::vector<caf::actor>();

            for (const auto &i : manager_.factories()) {
                if (i.second.factory()->type() & PluginFlags::PF_DATA_SOURCE and
                    resident_.count(i.first))
                    actors.push_back(resident_[i.first]);
            }

            if (actors.empty())
                return UuidActorVector();

            auto rp = make_response_promise<UuidActorVector>();

            fan_out_request<policy::select_all>(
                actors, infinite, data_source::use_data_atom_v, jsn, media_rate, true)
                .then(
                    [=](const std::vector<UuidActorVector> results) mutable {
                        for (const auto &i : results) {
                            if (not i.empty())
                                return rp.deliver(i);
                        }
                        rp.deliver(UuidActorVector());
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        // helper for dealing with URI's, auto adds results..
        [=](data_source::use_data_atom,
            const caf::uri &uri,
            const caf::actor &session,
            const caf::actor &playlist,
            const FrameRate &media_rate) -> result<UuidActorVector> {
            auto rp = make_response_promise<UuidActorVector>();

            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                data_source::use_data_atom_v,
                uri,
                media_rate,
                playlist ? false : true)
                .then(
                    [=](const UuidActorVector &results) mutable {
                        // uri can contain playlist or media currently.
                        // if it's a playlist add to session.
                        // if it's media add to playlist.
                        scoped_actor sys{system()};
                        for (const auto &i : results) {
                            try {
                                auto type =
                                    request_receive<std::string>(*sys, i.actor(), type_atom_v);
                                if (type == "Media" and playlist) {
                                    anon_send(
                                        playlist,
                                        playlist::add_media_atom_v,
                                        i,
                                        utility::Uuid());
                                } else if (type == "Playlist" && session) {
                                    anon_send(
                                        session,
                                        session::add_playlist_atom_v,
                                        i.actor(),
                                        utility::Uuid(),
                                        false);
                                }
                                // spdlog::warn("type {}", type);
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        rp.deliver(results);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); }

                );

            return rp;
        },

        [=](json_store::update_atom, const JsonStore &js) {
            try {
                // this will trash manually enabled/disabled plugins.
                // need to reflect changes to this dict..
                update_from_preferences(js);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](utility::detail_atom, const PluginType type) -> std::vector<PluginDetail> {
            std::vector<PluginDetail> details;
            for (const auto &i : manager_.factories()) {
                if (i.second.factory()->type() & type)
                    details.emplace_back(PluginDetail(i.second));
            }

            return details;
        },

        [=](utility::detail_atom, const utility::Uuid &uuid) -> result<PluginDetail> {
            if (not manager_.factories().count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");
            return PluginDetail(manager_.factories().at(uuid));
        },

        [=](utility::detail_atom) -> std::vector<PluginDetail> {
            return manager_.plugin_detail();
        },

        [=](get_resident_atom) -> UuidActorVector {
            UuidActorVector resident;
            for (const auto &i : resident_)
                resident.emplace_back(UuidActor(i.first, i.second));
            return resident;
        },

        [=](get_resident_atom, const utility::Uuid &uuid) -> result<caf::actor> {
            if (not resident_.count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");
            return resident_[uuid];
        },

        // spawn resident plugins
        [=](spawn_plugin_atom, const utility::JsonStore &json) {
            // find enabled / resident plugins
            update_from_preferences(json);
            // for(const auto &i : manager_.factories()) {
            //     if(i.second.factory()->resident() and i.second.enabled())
            //         enable_resident(i.first, true, json);
            // }
        },

        [=](spawn_plugin_atom atom, const utility::Uuid &uuid) {
            delegate(actor_cast<caf::actor>(this), atom, uuid, utility::JsonStore());
        },

        [=](spawn_plugin_atom,
            const utility::Uuid &uuid,
            const utility::JsonStore &json,
            bool make_resident) -> result<caf::actor> {
            if (resident_.count(uuid)) {
                return resident_[uuid];
            }

            if (not manager_.factories().count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");

            if (make_resident) {
                enable_resident(uuid, true, json);
                return resident_[uuid];
            }

            auto spawned = caf::actor();
            try {
                spawned = manager_.spawn(*scoped_actor(system()), uuid, json);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
            return spawned;
        },

        [=](spawn_plugin_atom,
            const utility::Uuid &uuid,
            const utility::JsonStore &json) -> result<caf::actor> {
            if (resident_.count(uuid)) {
                return resident_[uuid];
            }

            if (not manager_.factories().count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");

            auto spawned = caf::actor();
            try {
                spawned = manager_.spawn(*scoped_actor(system()), uuid, json);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
            return spawned;
        },

        [=](spawn_plugin_base_atom,
            const std::string name,
            const utility::JsonStore &json,
            const std::string class_name) -> result<caf::actor> {
            /*if (base_plugins_.find(name) == base_plugins_.end()) {
                base_plugins_[name] = spawn<plugin::StandardPlugin>(name, json);
                link_to(base_plugins_[name]);
            }*/
            auto rp = make_response_promise<caf::actor>();
            if (class_name == "HUDPlugin") {
                rp.deliver(spawn<plugin::HUDPluginBase>(name, json)); // base_plugins_[name];
            } else if (class_name == "ViewportLayoutPlugin") {
                // slightly awkward. We want to spawn an instance of
                // ViewportLayoutPlugin class to back a Python plugin for
                // managing viewport layouts. To avoid making the plugin_manager
                // component link-dependent on the ui::viewport component we
                // spawn via the viewport_layouts_manager
                std::vector<PluginDetail> details = manager_.plugin_detail();
                for (auto &detail: details) {
                    if (detail.name_ == "DefaultViewportLayout") {
                        try {
                            auto j = json;
                            j["name"] = name;
                            j["is_python"] = true;
                            rp.deliver(manager_.spawn(*scoped_actor(system()), detail.uuid_, j));
                        } catch (std::exception &e) {
                            rp.deliver(make_error(xstudio_error::error, e.what()));
                        }
                    }
                }
                if (rp.pending()) {
                    rp.deliver(make_error(xstudio_error::error, "Failed to spawn base ViewportLayoutPlugin"));
                }
                return rp;
            }
            rp.deliver(spawn<plugin::StandardPlugin>(name, json)); // base_plugins_[name];
            return rp;
        },

        [=](spawn_plugin_base_atom, const std::string name) -> result<caf::actor> {
            if (base_plugins_.find(name) != base_plugins_.end()) {
                return base_plugins_[name];
            }
            return caf::actor();
        },


        [=](spawn_plugin_ui_atom,
            const utility::Uuid &uuid) -> result<std::tuple<std::string, std::string>> {
            if (not manager_.factories().count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");
            return std::make_tuple(
                manager_.spawn_widget_ui(uuid), manager_.spawn_menu_ui(uuid));
        },

        [=](session::path_atom) -> std::vector<std::string> {
            return std::vector<std::string>(
                manager_.plugin_paths().begin(), manager_.plugin_paths().end());
        },

        [=](add_path_atom, const std::string &path) { manager_.emplace_front_path(path); },

        [=](enable_atom, const utility::Uuid &uuid) -> result<bool> {
            if (not manager_.factories().count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");

            return manager_.factories().at(uuid).enabled();
        },

        [=](enable_atom, const utility::Uuid &uuid, const bool enabled) -> result<bool> {
            if (not manager_.factories().count(uuid))
                return make_error(xstudio_error::error, "Invalid uuid");

            manager_.factories().at(uuid).set_enabled(enabled);

            // check if it's a resident
            if (manager_.factories().at(uuid).factory()->resident())
                enable_resident(uuid, enabled);

            send(
                event_group_,
                utility::event_atom_v,
                utility::detail_atom_v,
                manager_.plugin_detail());

            return true;
        },

        [=](json_store::update_atom) -> int {
            int result = manager_.load_plugins();
            send(
                event_group_,
                utility::event_atom_v,
                utility::detail_atom_v,
                manager_.plugin_detail());
            return result;
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}

void PluginManagerActor::on_exit() { system().registry().erase(plugin_manager_registry); }

void PluginManagerActor::enable_resident(
    const utility::Uuid &uuid, const bool enable, const utility::JsonStore &json) {
    if (enable and not resident_.count(uuid)) {
        auto actor = manager_.spawn(*scoped_actor(system()), uuid, json);
        system().registry().put(actor.id(), actor);
        link_to(actor);
        resident_[uuid] = actor;
    } else if (not enable and resident_.count(uuid)) {
        // check for resident
        system().registry().erase(resident_[uuid].id());
        unlink_from(resident_[uuid]);
        send_exit(resident_[uuid], caf::exit_reason::user_shutdown);
        resident_.erase(uuid);
    }
}

// only change initial enabled state don't acutally action it.
void PluginManagerActor::update_from_preferences(const utility::JsonStore &json) {
    try {
        auto prefs = preference_value<JsonStore>(json, "/core/plugin_manager/enable_plugin");

        for (auto &el : prefs.items()) {
            try {
                auto uuid = utility::Uuid(el.key());
                if (manager_.factories().count(uuid))
                    manager_.factories().at(uuid).set_enabled(el.value().get<bool>());
            } catch (...) {
            }
        }

        for (const auto &i : manager_.factories()) {
            if (i.second.factory()->resident() and i.second.enabled())
                enable_resident(i.first, true, json);
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}
