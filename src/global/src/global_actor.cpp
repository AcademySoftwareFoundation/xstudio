// SPDX-License-Identifier: Apache-2.0
#include <caf/io/all.hpp>
#include <caf/policy/select_all.hpp>
#include <tuple>

#include <fmt/chrono.h>
#include <fmt/format.h>


#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/colour_pipeline/colour_cache_actor.hpp"
#include "xstudio/colour_pipeline/colour_pipeline_actor.hpp"
#include "xstudio/conform/conform_manager_actor.hpp"
#include "xstudio/embedded_python/embedded_python_actor.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/global_store/global_store_actor.hpp"
#include "xstudio/media/media_metadata_manager_actor.hpp"
#include "xstudio/media_cache/media_cache_actor.hpp"
#include "xstudio/media_hook/media_hook_actor.hpp"
#include "xstudio/media_metadata/media_metadata_actor.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playhead/playhead_global_events_actor.hpp"
#include "xstudio/plugin_manager/plugin_manager_actor.hpp"
#include "xstudio/scanner/scanner_actor.hpp"
#include "xstudio/studio/studio_actor.hpp"
#include "xstudio/sync/sync_actor.hpp"
#include "xstudio/thumbnail/thumbnail_manager_actor.hpp"
#include "xstudio/ui/model_data/model_data_actor.hpp"
#include "xstudio/ui/viewport/keypress_monitor.hpp"
#include "xstudio/ui/viewport/viewport_layout_plugin.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

// include for system (soundcard) audio output
#ifdef __linux__
#include "xstudio/audio/linux_audio_output_device.hpp"
#elif __APPLE__
// TO DO
#elif _WIN32
#include "xstudio/audio/windows_audio_output_device.hpp"
#endif

using namespace caf;
using namespace xstudio;
using namespace xstudio::global;
using namespace xstudio::utility;
using namespace xstudio::global_store;

GlobalActor::GlobalActor(caf::actor_config &cfg, const utility::JsonStore &prefs)
    : caf::event_based_actor(cfg), rsm_(remote_session_path()) {
    init(prefs);
}

int GlobalActor::publish_port(
    const int minimum, const int maximum, const std::string &bind_address, caf::actor a) {
    int port = -1;
    if (system().has_middleman()) {
        for (auto i = minimum; i <= maximum; i++) {
            auto acquired_port = system().middleman().publish(a, i, bind_address.c_str(), true);
            if (acquired_port) {
                port = *acquired_port;
                break;
            }
        }
    }

    return port;
}


void GlobalActor::init(const utility::JsonStore &prefs) {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created GlobalActor");
    print_on_exit(this, "GlobalActor");

    system().registry().put(global_registry, this);

    // spawning the 'GlobalModuleAttrEventsActor' first because subsequent
    // actors might want to connect with it on creation .. see Module::connect_to_ui()
    auto gsa = caf::actor();

    if (prefs.is_null()) {
        gsa = spawn<global_store::GlobalStoreActor>(
            "GlobalStore",
            global_store::global_store_builder(
                std::vector<std::string>{xstudio_root("/preference")}));
    } else {
        gsa = spawn<global_store::GlobalStoreActor>("GlobalStore", prefs);
    }

    auto sga             = spawn<sync::SyncGatewayActor>();
    auto sgma            = spawn<sync::SyncGatewayManagerActor>();
    auto keyboard_events = spawn<ui::keypress_monitor::KeypressMonitor>();
    auto ui_models       = spawn<ui::model_data::GlobalUIModelData>();
    auto metadata_mgr    = spawn<media::GlobalMetadataManager>();
    auto pm              = spawn<plugin_manager::PluginManagerActor>();
    auto colour          = spawn<colour_pipeline::GlobalColourPipelineActor>();
    auto gmma            = spawn<media_metadata::GlobalMediaMetadataActor>();
    auto gica            = spawn<media_cache::GlobalImageCacheActor>();
    auto gaca            = spawn<media_cache::GlobalAudioCacheActor>();
    auto gmra            = spawn<media_reader::GlobalMediaReaderActor>();
    auto gcca            = spawn<colour_pipeline::GlobalColourCacheActor>();
    auto gmha            = spawn<media_hook::GlobalMediaHookActor>();
    auto thumbnail       = spawn<thumbnail::ThumbnailManagerActor>();
    auto audio           = spawn<audio::GlobalAudioOutputActor>();
    auto phev            = spawn<playhead::PlayheadGlobalEventsActor>();
    auto pa              = spawn<embedded_python::EmbeddedPythonActor>("Python");
    auto scanner         = spawn<scanner::ScannerActor>();
    auto conform         = spawn<conform::ConformManagerActor>();
    auto vpmgr           = spawn<ui::viewport::ViewportLayoutManager>();

    link_to(audio);
    link_to(colour);
    link_to(conform);
    link_to(gaca);
    link_to(gcca);
    link_to(gica);
    link_to(gmha);
    link_to(gmma);
    link_to(gmra);
    link_to(gsa);
    link_to(keyboard_events);
    link_to(pa);
    link_to(phev);
    link_to(pm);
    link_to(scanner);
    link_to(sga);
    link_to(sgma);
    link_to(metadata_mgr);
    link_to(thumbnail);
    link_to(ui_models);
    link_to(vpmgr);

    // Make default audio output
#ifdef __linux__
    auto audio_out = spawn_audio_output_actor<audio::LinuxAudioOutputDevice>(prefs);
    link_to(audio_out);
#elif __APPLE__
    // TO DO
#elif _WIN32
    auto audio_out = spawn_audio_output_actor<audio::WindowsAudioOutputDevice>(prefs);
    link_to(audio_out);
#endif

    if (audio_out) {
        system().registry().put(pc_audio_output_registry, audio_out);
    }

    python_enabled_ = false;
    connected_      = false;
    api_enabled_    = false;
    port_           = -1;
    port_minimum_   = 12345;
    port_maximum_   = 12345;
    bind_address_   = "127.0.0.1";

    sync_connected_    = false;
    sync_api_enabled_  = false;
    sync_port_         = -1;
    sync_port_minimum_ = 12346;
    sync_port_maximum_ = 12346;
    sync_bind_address_ = "127.0.0.1";

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        // spawn resident plugins (application level)
        anon_send(pm, plugin_manager::spawn_plugin_atom_v, j);
        anon_send(this, json_store::update_atom_v, j);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    behavior_.assign(
        make_get_version_handler(),
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },

        [=](exit_atom) -> bool {
            send_exit(this, caf::exit_reason::user_shutdown);
            return true;
        },

        [=](busy_atom, const bool value) mutable {
            if (value) {
                busy_.insert(caf::actor_cast<caf::actor_addr>(current_sender()));
                delegate(
                    actor_cast<caf::actor>(this), status_atom_v, StatusType::ST_BUSY, true);
            } else {
                busy_.erase(caf::actor_cast<caf::actor_addr>(current_sender()));
                if (busy_.empty()) {
                    delegate(
                        actor_cast<caf::actor>(this),
                        status_atom_v,
                        StatusType::ST_BUSY,
                        false);
                } else {
                    delegate(
                        actor_cast<caf::actor>(this), status_atom_v, StatusType::ST_BUSY, true);
                }
            }
        },

        [=](get_actor_from_registry_atom, std::string actor_name) -> result<caf::actor> {
            try {
                return system().registry().template get<caf::actor>(actor_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },

        [=](status_atom) -> StatusType { return status_; },

        [=](status_atom, const StatusType field, const bool set) mutable -> StatusType {
            if (set)
                status_ = status_ | field;
            else
                status_ = status_ & ~field;
            send(event_group_, utility::event_atom_v, status_atom_v, status_);
            return status_;
        },

        [=](global_store::autosave_atom) -> bool { return session_autosave_; },

        [=](global_store::autosave_atom, const bool enable) {
            // if (enable != session_autosave_)
            //     send(event_group_, utility::event_atom_v, autosave_atom_v, enable);
            if (session_autosave_ != enable) {
                session_autosave_ = enable;
                if (session_autosave_)
                    delayed_anon_send(
                        actor_cast<caf::actor>(this),
                        std::chrono::seconds(session_autosave_interval_),
                        global_store::do_autosave_atom_v);
            }
        },

        [=](global_store::do_autosave_atom) {
            // trigger next autosave
            if (session_autosave_)
                delayed_anon_send(
                    actor_cast<caf::actor>(this),
                    std::chrono::seconds(session_autosave_interval_),
                    global_store::do_autosave_atom_v);
            if (session_autosave_path_.empty()) {
                spdlog::warn("Autosave path not set, autosave skipped.");
            } else {
                if (status_ & ST_BUSY) {
                    spdlog::debug("Skipping autosave whilst playing.");
                } else {
                    // get session actor
                    request(studio_, infinite, session::session_atom_v)
                        .then(
                            [=](caf::actor session) {
                                // request path from session
                                request(session, infinite, session::path_atom_v)
                                    .then(
                                        [=](const std::pair<caf::uri, fs::file_time_type>
                                                &path_time) {
                                            // save session
                                            try {
                                                auto session_name =
                                                    fs::path(uri_to_posix_path(path_time.first))
                                                        .stem()
                                                        .string();
                                                if (session_name.empty())
                                                    session_name = "Unsaved";

                                                // add timestamp+ext
                                                auto session_fullname = std::string(fmt::format(
                                                    "{}_{:%Y%m%d_%H%M%S}.xsz",
                                                    session_name,
                                                    fmt::localtime(std::time(nullptr))));

                                                // build path to autosave store.
                                                auto fspath = fs::path(uri_to_posix_path(
                                                                  session_autosave_path_)) /
                                                              session_name / session_fullname;

                                                // create autosave dirs
                                                fs::create_directories(fspath.parent_path());

                                                // prune autosaves
                                                std::set<fs::path> saves;
                                                for (const auto &entry : fs::directory_iterator(
                                                         fspath.parent_path())) {
                                                    if (fs::is_regular_file(entry.status()))
                                                        saves.insert(entry.path());
                                                }

                                                while (saves.size() >= 10) {
                                                    fs::remove(*(saves.begin()));
                                                    saves.erase(saves.begin());
                                                }

                                                request(
                                                    session,
                                                    infinite,
                                                    global_store::save_atom_v,
                                                    posix_path_to_uri(fspath.string()),
                                                    session_autosave_hash_,
                                                    false)
                                                    .then(
                                                        [=](const size_t hash) {
                                                            if (hash !=
                                                                session_autosave_hash_) {
                                                                spdlog::info(
                                                                    "Session autosaved {}.",
                                                                    fspath.string());
                                                                auto prefs = global_store::
                                                                    GlobalStoreHelper(system());
                                                                prefs.set_value(
                                                                    fspath.string(),
                                                                    "/core/session/autosave/"
                                                                    "last_auto_save");
                                                                prefs.save("APPLICATION");

                                                                session_autosave_hash_ = hash;
                                                            }
                                                        },
                                                        [=](const error &err) {
                                                            auto msg = std::string(fmt::format(
                                                                "Failed to autosave session - "
                                                                "your "
                                                                "session is broken {}.\nCheck "
                                                                "{} "
                                                                "for last valid autosave",
                                                                to_string(err),
                                                                fspath.parent_path().string()));
                                                            spdlog::critical(msg);
                                                        });
                                            } catch (const std::exception &err) {
                                                spdlog::critical(
                                                    "Failed to autosave session path {}.",
                                                    err.what());
                                            }
                                        },
                                        [=](const error &err) {
                                            spdlog::critical(
                                                "Failed to get session path {}.",
                                                to_string(err));
                                        });
                            },
                            [=](const error &err) {
                                spdlog::critical(
                                    "Failed to get session actor {}.", to_string(err));
                            });
                }
            }
        },

        [&](create_studio_atom, const std::string &name) -> caf::actor {
            if (!studio_) {
                studio_ = spawn<studio::StudioActor>(name);
                link_to(studio_);
            }
            return studio_;
        },

        [=](get_api_mode_atom) -> std::string { return "FULL"; },

        [=](get_application_mode_atom) -> std::string {
            return (ui_studio_ ? "XSTUDIO_GUI" : "XSTUDIO");
        },

        [=](colour_pipeline::colour_pipeline_atom atom) {
            // 'colour' is the colour pipeline manager. To get to the
            // actual colour pipelin actor (OCIO plugin) we delegate to
            // the manager. Getting to the manager alon is not interesting.
            delegate(colour, atom);
        },

        [=](get_global_audio_cache_atom) -> caf::actor { return gaca; },
        [=](get_plugin_manager_atom) -> caf::actor { return pm; },

        [=](get_global_image_cache_atom) -> caf::actor { return gica; },

        [=](get_global_playhead_events_atom) -> caf::actor { return phev; },

        [=](get_global_store_atom) -> caf::actor { return gsa; },

        [=](get_global_thumbnail_atom) -> caf::actor { return thumbnail; },

        [=](get_python_atom) -> caf::actor { return pa; },
        [=](get_scanner_atom) -> caf::actor { return scanner; },

        [&](get_studio_atom) -> caf::actor { return studio_; },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },


        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                python_enabled_ = preference_value<bool>(j, "/core/python/enabled");
                api_enabled_    = preference_value<bool>(j, "/core/api/enabled");
                port_minimum_   = preference_value<int>(j, "/core/api/port_minimum");
                port_maximum_   = preference_value<int>(j, "/core/api/port_maximum");
                bind_address_   = preference_value<std::string>(j, "/core/api/bind_address");

                sync_api_enabled_  = preference_value<bool>(j, "/core/sync/enabled");
                sync_port_minimum_ = preference_value<int>(j, "/core/sync/port_minimum");
                sync_port_maximum_ = preference_value<int>(j, "/core/sync/port_maximum");
                sync_bind_address_ =
                    preference_value<std::string>(j, "/core/sync/bind_address");

                session_autosave_interval_ =
                    preference_value<int>(j, "/core/session/autosave/interval");
                try {
                    session_autosave_path_ = posix_path_to_uri(expand_envvars(
                        preference_value<std::string>(j, "/core/session/autosave/path")));
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }

                auto session_autosave =
                    preference_value<bool>(j, "/core/session/autosave/enabled");

                if (session_autosave != session_autosave_) {
                    anon_send(
                        actor_cast<caf::actor>(this),
                        global_store::autosave_atom_v,
                        session_autosave);
                }

                disconnect_api(pa);
                disconnect_sync_api();
                connect_api(pa);
                connect_sync_api();
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](last_changed_atom atom, const time_point &stp) { delegate(studio_, atom, stp); },

        [=](remote_session_name_atom, const std::string &session_name) {
            if (not session_name.empty() and session_name != remote_api_session_name_) {

                if (connected_) {
                    rsm_.remove_session(remote_api_session_name_);
                    remote_api_session_name_ = session_name;
                    rsm_.create_session_file(
                        port_, false, remote_api_session_name_, "localhost", true);

                    spdlog::info(
                        "API enabled on {}:{}, session name {}",
                        bind_address_,
                        port_,
                        remote_api_session_name_);
                } else {
                    disconnect_api(pa, true);
                    remote_api_session_name_ = session_name;
                    connect_api(pa);
                }
            }
        },

        [=](session::session_atom _atom) { delegate(studio_, _atom); },

        [=](session::session_atom _atom, caf::actor actor) { delegate(studio_, _atom, actor); },

        [=](session::session_request_atom _atom, const std::string &path, const JsonStore &js) {
            delegate(studio_, _atom, path, js);
        },

        [=](bookmark::get_bookmark_atom atom) { delegate(studio_, atom); },

        [=](sync::get_sync_atom _atm) { delegate(sgma, _atm); });
}

void GlobalActor::on_exit() {
    // shutdown
    // clear autosave.
    send(event_group_, exit_atom_v);
    auto prefs = global_store::GlobalStoreHelper(system());
    prefs.set_value("", "/core/session/autosave/last_auto_save", false);
    prefs.save("APPLICATION");
    if (system().has_middleman()) {
        system().middleman().unpublish(actor_cast<actor>(this), port_);
        system().middleman().unpublish(actor_cast<actor>(this), sync_port_);
    }
    system().registry().erase(global_registry);
    system().registry().erase(pc_audio_output_registry);
}

void GlobalActor::connect_api(const caf::actor &embedded_python) {
    if (not connected_ and api_enabled_) {
        port_ = publish_port(port_minimum_, port_maximum_, bind_address_, this);
        if (port_ != -1) {
            rsm_.remove_session(remote_api_session_name_);
            if (remote_api_session_name_.empty())
                remote_api_session_name_ = rsm_.create_session_file(port_, false);
            else
                rsm_.create_session_file(
                    port_, false, remote_api_session_name_, "localhost", true);
            spdlog::info(
                "API enabled on {}:{}, session name {}",
                bind_address_,
                port_,
                remote_api_session_name_);
            connected_ = true;
            if (python_enabled_) {
                // request(pa, infinite, connect_atom_v,
                // actor_cast<actor>(this)).then(
                request(embedded_python, infinite, connect_atom_v, port_)
                    .then(
                        [=](const bool result) {
                            if (result)
                                spdlog::debug("Connected {}", result);
                            else
                                spdlog::warn("Connected failed {}", result);
                        },
                        [=](const error &err) {
                            spdlog::warn("Connected failed {}.", to_string(err));
                        });
            }
        } else {
            spdlog::warn(
                "API failed to open port {}:{}-{}",
                bind_address_,
                port_minimum_,
                port_maximum_);
        }
    }
}

void GlobalActor::connect_sync_api() {
    if (not sync_connected_ and sync_api_enabled_) {
        sync_port_ =
            publish_port(sync_port_minimum_, sync_port_maximum_, sync_bind_address_, this);
        if (sync_port_ != -1) {
            rsm_.remove_session(remote_sync_session_name_);
            if (remote_sync_session_name_.empty())
                remote_sync_session_name_ = rsm_.create_session_file(sync_port_, true);
            else
                rsm_.create_session_file(
                    sync_port_, true, remote_sync_session_name_, "localhost", true);
            spdlog::info(
                "SYNC API enabled on {}:{}, session name {}",
                sync_bind_address_,
                sync_port_,
                remote_sync_session_name_);
            sync_connected_ = true;
        } else {
            spdlog::warn(
                "SYNC API failed to open  {}:{}-{}",
                sync_bind_address_,
                sync_port_minimum_,
                sync_port_maximum_);
        }
    }
}

void GlobalActor::disconnect_api(const caf::actor &embedded_python, const bool force) {
    if (connected_ and
        (force or not api_enabled_ or port_ > port_maximum_ or port_ < port_minimum_)) {
        connected_ = false;
        if (system().has_middleman() and python_enabled_) {
            request(embedded_python, infinite, connect_atom_v, 0)
                .then(
                    [=](const bool result) {
                        if (result)
                            spdlog::debug("Disconnected {}", result);
                        else
                            spdlog::warn("Disconnected failed {}", result);
                    },
                    [=](const error &err) {
                        spdlog::warn("Disconnected failed {}.", to_string(err));
                    });
            send(event_group_, api_exit_atom_v);

            // wait..?
            system().middleman().unpublish(actor_cast<actor>(this), port_);
            rsm_.remove_session(remote_api_session_name_);
            spdlog::info("API disabled on port {}", port_);
        }
        port_ = -1;
    }
}

void GlobalActor::disconnect_sync_api(const bool force) {
    if (sync_connected_ and
        (force or not sync_api_enabled_ or sync_port_ > sync_port_maximum_ or
         sync_port_ < sync_port_minimum_)) {
        sync_connected_ = false;
        if (system().has_middleman()) {
            system().middleman().unpublish(actor_cast<actor>(this), sync_port_);
            rsm_.remove_session(remote_sync_session_name_);
            spdlog::info("SYNC API disabled on port {}", sync_port_);
        }
        sync_port_ = -1;
    }
}
