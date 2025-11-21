// SPDX-License-Identifier: Apache-2.0
#include <caf/io/all.hpp>
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

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
#include "xstudio/thumbnail/thumbnail_manager_actor.hpp"
#include "xstudio/ui/model_data/model_data_actor.hpp"
#include "xstudio/ui/viewport/keypress_monitor.hpp"
#include "xstudio/ui/viewport/viewport_layout_plugin.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

// include for system (soundcard) audio output
#ifdef __linux__
#include "xstudio/audio/linux_audio_output_device.hpp"
#elif defined(__apple__)
#include "xstudio/audio/macos_audio_output_device.hpp"
#elif defined(_WIN32)
#include "xstudio/audio/windows_audio_output_device.hpp"
#endif

using namespace caf;
using namespace xstudio;
using namespace xstudio::global;
using namespace xstudio::utility;
using namespace xstudio::global_store;

APIActor::APIActor(caf::actor_config &cfg, const caf::actor &global)
    : caf::event_based_actor(cfg), global_(global) {

    spdlog::debug("Created APIActor");
    print_on_exit(this, "APIActor");

    behavior_.assign(
        make_get_version_handler(),
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_application_mode_atom) -> result<std::string> {
            // don't expose global..
            auto rp = make_response_promise<std::string>();

            mail(get_application_mode_atom_v)
                .request(global_, infinite)
                .then(
                    [=](const std::string &result) mutable { rp.deliver(result); },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](authenticate_atom) -> result<caf::actor> {
            if (allow_unauthenticated_ or
                (current_sender() and current_sender()->node() == node()))
                return global_;

            return make_error(xstudio_error::error, "Authentication required.");
        },

        [=](authenticate_atom, const std::string &key) -> result<caf::actor> {
            if (allow_unauthenticated_)
                return global_;
            else
                for (const auto &i : authentication_keys_)
                    if (key == i.get<std::string>())
                        return global_;

            return make_error(xstudio_error::error, "Authentication failed.");
        },

        [=](authenticate_atom,
            const std::string &user,
            const std::string &pass) -> result<caf::actor> {
            if (allow_unauthenticated_)
                return global_;
            else
                for (const auto &i : authentication_passwords_)
                    if (user == i.at("user").get<std::string>() and
                        pass == i.at("password").get<std::string>())
                        return global_;

            return make_error(xstudio_error::error, "Authentication failed.");
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            return mail(json_store::update_atom_v, full).delegate(actor_cast<caf::actor>(this));
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            try {
                allow_unauthenticated_ =
                    preference_value<bool>(j, "/core/api/authentication/allow_unauthenticated");

                authentication_passwords_ =
                    preference_value<JsonStore>(j, "/core/api/authentication/passwords");

                authentication_keys_ =
                    preference_value<JsonStore>(j, "/core/api/authentication/keys");

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        });
}

GlobalActor::GlobalActor(
    caf::actor_config &cfg,
    const utility::JsonStore &prefs,
    const bool embedded_python,
    const bool read_only)
    : caf::event_based_actor(cfg), rsm_(remote_session_path()) {
    init(prefs, embedded_python, read_only);
}

GlobalActor::~GlobalActor() {}

int GlobalActor::publish_port(
    const int minimum, const int maximum, const std::string &bind_address, caf::actor a) {
    int port = -1;
    if (system().has_middleman()) {
        for (auto i = minimum; i <= maximum; i++) {
#ifdef _WIN32
            // note: the re-use flag here must be false on Windows, otherwise multiple xstudio
            // sessions somehow use the same port where i==minimum.
            auto acquired_port =
                system().middleman().publish(a, i, bind_address.c_str(), false);
#else
            auto acquired_port = system().middleman().publish(a, i, bind_address.c_str(), true);
#endif
            if (acquired_port) {
                port = *acquired_port;
                break;
            }
        }
    }

    return port;
}

void GlobalActor::init(
    const utility::JsonStore &prefs, const bool embedded_python, const bool read_only) {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created GlobalActor");
    print_on_exit(this, "GlobalActor");

    system().registry().put(global_registry, this);

    // spawning the 'GlobalModuleAttrEventsActor' first because subsequent
    // actors might want to connect with it on creation .. see Module::connect_to_ui()
    if (prefs.is_null()) {
        gsa_ = spawn<global_store::GlobalStoreActor>(
            "GlobalStore",
            global_store::global_store_builder(
                std::vector<std::string>{xstudio_resources_dir("preference")}),
            read_only);
    } else {
        gsa_ = spawn<global_store::GlobalStoreActor>("GlobalStore", prefs, read_only);
    }

    auto phev            = spawn<playhead::PlayheadGlobalEventsActor>();
    auto keyboard_events = spawn<ui::keypress_monitor::KeypressMonitor>();
    auto ui_models       = spawn<ui::model_data::GlobalUIModelData>();
    auto metadata_mgr    = spawn<media::GlobalMetadataManager>();
    auto audio           = spawn<audio::GlobalAudioOutputActor>();
    auto pm              = spawn<plugin_manager::PluginManagerActor>();
    auto colour          = spawn<colour_pipeline::GlobalColourPipelineActor>();
    auto gmma            = spawn<media_metadata::GlobalMediaMetadataActor>();
    auto gica            = spawn<media_cache::GlobalImageCacheActor>();
    auto gaca            = spawn<media_cache::GlobalAudioCacheActor>();
    auto gmra            = spawn<media_reader::GlobalMediaReaderActor>();
    auto gcca            = spawn<colour_pipeline::GlobalColourCacheActor>();
    auto gmha            = spawn<media_hook::GlobalMediaHookActor>();
    auto thumbnail       = spawn<thumbnail::ThumbnailManagerActor>();
    auto pa =
        embedded_python ? spawn<embedded_python::EmbeddedPythonActor>("Python") : caf::actor();
    auto scanner = spawn<scanner::ScannerActor>();
    auto conform = spawn<conform::ConformManagerActor>();
    auto vpmgr   = spawn<ui::viewport::ViewportLayoutManager>();

    link_to(audio);
    link_to(colour);
    link_to(conform);
    link_to(gaca);
    link_to(gcca);
    link_to(gica);
    link_to(gmha);
    link_to(gmma);
    link_to(gmra);
    link_to(keyboard_events);
    if (embedded_python)
        link_to(pa);
    link_to(phev);
    link_to(pm);
    link_to(scanner);
    link_to(metadata_mgr);
    link_to(thumbnail);
    link_to(ui_models);
    link_to(vpmgr);

    // Make default audio output
#ifdef __linux__
    auto audio_out = spawn_audio_output_actor<audio::LinuxAudioOutputDevice>(prefs);
#endif

#ifdef _WIN32
    auto audio_out = spawn_audio_output_actor<audio::WindowsAudioOutputDevice>(prefs);
#endif

#ifdef __apple__
    auto audio_out = spawn_audio_output_actor<audio::MacOSAudioOutputDevice>(prefs);
#endif

    system().registry().put(pc_audio_output_registry, audio_out);
    link_to(audio_out);

    python_enabled_ = false;
    connected_      = false;
    api_enabled_    = false;
    port_           = -1;
    port_minimum_   = 12345;
    port_maximum_   = 12345;
    bind_address_   = "127.0.0.1";

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    apia_ = spawn<APIActor>(this);

    try {
        JsonStore j;
        auto prefs = GlobalStoreHelper(system());
        join_broadcast(this, prefs.get_group(j));
        // spawn resident plugins (application level)
        anon_mail(plugin_manager::spawn_plugin_atom_v, j).send(pm);
        anon_mail(json_store::update_atom_v, j).send(apia_);
        anon_mail(json_store::update_atom_v, j).send(this);

        // here we set-up filepath remapping (only once here, at start-up)
        file_map_regex_["/core/media_reader/filepath_map_regex_replace"] =
            prefs.value<utility::JsonStore>("/core/media_reader/filepath_map_regex_replace");

        file_map_regex_["/core/media_reader/user_filepath_map_regex_replace"] =
            prefs.value<utility::JsonStore>(
                "/core/media_reader/user_filepath_map_regex_replace");

        utility::setup_filepath_remap_regex(file_map_regex_);

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
                return mail(status_atom_v, StatusType::ST_BUSY, true)
                    .delegate(actor_cast<caf::actor>(this));
            } else {
                busy_.erase(caf::actor_cast<caf::actor_addr>(current_sender()));
                if (busy_.empty()) {
                    return mail(status_atom_v, StatusType::ST_BUSY, false)
                        .delegate(actor_cast<caf::actor>(this));
                } else {
                    return mail(status_atom_v, StatusType::ST_BUSY, true)
                        .delegate(actor_cast<caf::actor>(this));
                }
            }
        },

        [=](history::log_atom, const spdlog::level::level_enum level, const std::string &log) {
            spdlog::log(level, log);
        },

        [=](get_actor_from_registry_atom, const std::string &actor_name) -> result<caf::actor> {
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
            mail(utility::event_atom_v, status_atom_v, status_).send(event_group_);
            return status_;
        },

        [=](global_store::autosave_atom) -> bool { return session_autosave_; },

        [=](global_store::autosave_atom, const bool enable) {
            // if (enable != session_autosave_)
            //     mail(utility::event_atom_v, autosave_atom_v, enable).send(event_group_);
            if (session_autosave_ != enable) {
                session_autosave_ = enable;
                if (session_autosave_)
                    anon_mail(global_store::do_autosave_atom_v)
                        .delay(std::chrono::seconds(session_autosave_interval_))
                        .send(actor_cast<caf::actor>(this), weak_ref);
            }
        },

        [=](global_store::do_autosave_atom) {
            // trigger next autosave
            if (session_autosave_)
                anon_mail(global_store::do_autosave_atom_v)
                    .delay(std::chrono::seconds(session_autosave_interval_))
                    .send(actor_cast<caf::actor>(this), weak_ref);
            if (session_autosave_path_.empty()) {
                spdlog::warn("Autosave path not set, autosave skipped.");
            } else {
                if (status_ & ST_BUSY) {
                    spdlog::debug("Skipping autosave whilst playing.");
                } else {
                    // get session actor
                    mail(session::session_atom_v)
                        .request(studio_, infinite)
                        .then(
                            [=](caf::actor session) {
                                // request path from session
                                mail(session::path_atom_v)
                                    .request(session, infinite)
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

                                                mail(

                                                    global_store::save_atom_v,
                                                    posix_path_to_uri(fspath.string()),
                                                    session_autosave_hash_,
                                                    false)
                                                    .request(session, infinite)
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
                                                                    to_string(posix_path_to_uri(
                                                                        fspath.string())),
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

        [=](get_application_mode_atom) -> std::string {
            return (ui_studio_ ? "XSTUDIO_GUI" : "XSTUDIO");
        },

        [=](colour_pipeline::colour_pipeline_atom atom) {
            // 'colour' is the colour pipeline manager. To get to the
            // actual colour pipelin actor (OCIO plugin) we delegate to
            // the manager. Getting to the manager alon is not interesting.
            return mail(atom).delegate(colour);
        },

        [=](get_global_audio_cache_atom) -> caf::actor { return gaca; },
        [=](get_plugin_manager_atom) -> caf::actor { return pm; },

        [=](get_global_image_cache_atom) -> caf::actor { return gica; },

        [=](get_global_playhead_events_atom) -> caf::actor { return phev; },

        [=](get_global_store_atom) -> caf::actor { return gsa_; },

        [=](get_global_thumbnail_atom) -> caf::actor { return thumbnail; },

        [=](get_python_atom) -> caf::actor { return pa; },
        [=](get_scanner_atom) -> caf::actor { return scanner; },

        [&](get_studio_atom) -> caf::actor { return studio_; },

        [=](json_store::update_atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            // THe 'user_filepath_map_regex_replace' preference can change during the
            // session. Here we update the regex list that can re-map filepaths on-the-fly
            if (path == "/core/media_reader/user_filepath_map_regex_replace/value") {
                file_map_regex_["/core/media_reader/user_filepath_map_regex_replace"] = change;
                utility::setup_filepath_remap_regex(file_map_regex_);
            } else {
                std::ignore = mail(json_store::update_atom_v, full)
                                  .delegate(actor_cast<caf::actor>(this));
            }
        },

        [=](json_store::update_atom, const JsonStore &j) mutable {
            anon_mail(json_store::update_atom_v, j).send(apia_);

            try {
                python_enabled_ = preference_value<bool>(j, "/core/python/enabled");
                api_enabled_    = preference_value<bool>(j, "/core/api/enabled");
                port_minimum_   = preference_value<int>(j, "/core/api/port_minimum");
                port_maximum_   = preference_value<int>(j, "/core/api/port_maximum");
                bind_address_   = preference_value<std::string>(j, "/core/api/bind_address");

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
                    anon_mail(global_store::autosave_atom_v, session_autosave)
                        .send(actor_cast<caf::actor>(this));
                }

                disconnect_api(pa);
                connect_api(pa);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](last_changed_atom atom, const time_point &stp) {
            return mail(atom, stp).delegate(studio_);
        },

        [=](remote_session_name_atom, const std::string &session_name) {
            if (not session_name.empty() and session_name != remote_api_session_name_) {

                if (connected_) {
                    rsm_.remove_session(remote_api_session_name_);
                    remote_api_session_name_ = session_name;
                    rsm_.create_session_file(
                        port_, remote_api_session_name_, "localhost", true);

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

        [=](session::session_atom _atom) { return mail(_atom).delegate(studio_); },

        [=](session::session_atom _atom, caf::actor actor) {
            return mail(_atom, actor).delegate(studio_);
        },

        [=](session::session_request_atom,
            const std::string &path,
            const JsonStore &js) -> result<bool> {
            if (studio_) {
                auto rp = make_response_promise<bool>();
                // need to chat to UI ?
                rp.delegate(studio_, session::session_request_atom_v, path, js);
                return rp;
            }

            return make_error(xstudio_error::error, "studio actor not created.");
        },


        [=](bookmark::get_bookmark_atom atom) { return mail(atom).delegate(studio_); }

    );
}

void GlobalActor::on_exit() {
    // shutdown
    // clear autosave.
    mail(exit_atom_v).send(event_group_);

    // this will not work as global store is already gone.
    auto prefs = global_store::GlobalStoreHelper(system());
    prefs.save("APPLICATION");

    if (system().has_middleman()) {
        system().middleman().unpublish(apia_, port_);
    }
    send_exit(apia_, caf::exit_reason::user_shutdown);
    send_exit(gsa_, caf::exit_reason::user_shutdown);
    gsa_  = caf::actor();
    apia_ = caf::actor();

    system().registry().erase(global_registry);
    system().registry().erase(pc_audio_output_registry);
}

void GlobalActor::connect_api(const caf::actor &embedded_python) {
    if (not connected_ and api_enabled_) {
        port_ = publish_port(port_minimum_, port_maximum_, bind_address_, apia_);
        if (port_ != -1) {
            rsm_.remove_session(remote_api_session_name_);
            if (remote_api_session_name_.empty())
                remote_api_session_name_ = rsm_.create_session_file(port_);
            else
                rsm_.create_session_file(port_, remote_api_session_name_, "localhost", true);
            spdlog::info(
                "API enabled on {}:{}, session name {}",
                bind_address_,
                port_,
                remote_api_session_name_);
            connected_ = true;
            if (python_enabled_) {
                mail(connect_atom_v, port_)
                    .request(embedded_python, infinite)
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

void GlobalActor::disconnect_api(const caf::actor &embedded_python, const bool force) {
    if (connected_ and
        (force or not api_enabled_ or port_ > port_maximum_ or port_ < port_minimum_)) {
        connected_ = false;
        if (system().has_middleman() and python_enabled_) {
            mail(connect_atom_v, 0)
                .request(embedded_python, infinite)
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
            mail(api_exit_atom_v).send(event_group_);

            // wait..?
            system().middleman().unpublish(apia_, port_);
            rsm_.remove_session(remote_api_session_name_);
            spdlog::info("API disabled on port {}", port_);
        }
        port_ = -1;
    }
}
