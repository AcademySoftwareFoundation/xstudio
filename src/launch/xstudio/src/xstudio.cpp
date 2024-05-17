// SPDX-License-Identifier: Apache-2.0

#include <args/args.hxx>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#ifdef __linux__
#include <execinfo.h>
#endif
#include <functional>
#include <iostream>
#include <regex>
#include <thread>
#ifdef __linux__
#include <unistd.h>
#endif


#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#ifndef CPPHTTPLIB_ZLIB_SUPPORT
#define CPPHTTPLIB_ZLIB_SUPPORT
#endif
#include <cpp-httplib/httplib.h>

#include <caf/actor_system.hpp>
#include <caf/scoped_actor.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/caf_utility/caf_setup.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/studio/studio_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/sequence.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/remote_session_file.hpp"
#include "xstudio/utility/serialise_headers.hpp"
#include "xstudio/utility/string_helpers.hpp"

CAF_PUSH_WARNINGS
#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <QPalette>
#include <QString>
#include <QQuickView>
CAF_POP_WARNINGS

#include "xstudio/ui/mouse.hpp"
#include "xstudio/ui/qml/bookmark_model_ui.hpp"     //NOLINT
#include "xstudio/ui/qml/embedded_python_ui.hpp"    //NOLINT
#include "xstudio/ui/qml/event_ui.hpp"              //NOLINT
#include "xstudio/ui/qml/global_store_model_ui.hpp" //NOLINT
#include "xstudio/ui/qml/helper_ui.hpp"             //NOLINT
#include "xstudio/ui/qml/hotkey_ui.hpp"             //NOLINT
#include "xstudio/ui/qml/log_ui.hpp"                //NOLINT
#include "xstudio/ui/qml/model_data_ui.hpp"         //NOLINT
#include "xstudio/ui/qml/module_data_ui.hpp"        //NOLINT
#include "xstudio/ui/qml/module_menu_ui.hpp"        //NOLINT
#include "xstudio/ui/qml/module_ui.hpp"             //NOLINT
#include "xstudio/ui/qml/qml_viewport.hpp"          //NOLINT
#include "xstudio/ui/qml/session_model_ui.hpp"      //NOLINT
#include "xstudio/ui/qml/snapshot_model_ui.hpp"     //NOLINT
#include "xstudio/ui/qml/shotgun_provider_ui.hpp"
#include "xstudio/ui/qml/studio_ui.hpp" //NOLINT
#include "xstudio/ui/qml/thumbnail_provider_ui.hpp"
#include "xstudio/ui/qt/offscreen_viewport.hpp" //NOLINT

//TODO: Ahead Fix
#include "QuickFuture"

Q_DECLARE_METATYPE(QUrl)
Q_DECLARE_METATYPE(QList<QUuid>)
Q_DECLARE_METATYPE(QFuture<QUuid>)
Q_DECLARE_METATYPE(QFuture<QList<QUuid>>)
Q_DECLARE_METATYPE(QFuture<QUrl>)

using namespace std;
using namespace caf;
using namespace std::chrono_literals;

using namespace xstudio::caf_utility;
using namespace xstudio::global;
using namespace xstudio::studio;
using namespace xstudio::global_store;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;
using namespace xstudio;

bool shutdown_xstudio = false;

struct ExitTimeoutKiller {

    void start() {
#ifdef _WIN32
        spdlog::debug("ExitTimeoutKiller start ignored");
    }
#else


        // lock the mutex ...
        clean_actor_system_exit.lock();

        // .. and start a thread to watch the mutex
        exit_timeout = std::thread([&]() {
            // wait for stop() to be called - 10s
            if (!clean_actor_system_exit.try_lock_for(std::chrono::seconds(10))) {
                // stop() wasn't called! Probably failed to exit actor_system,
                // see main() function. Kill process.
                spdlog::critical("xSTUDIO has not exited cleanly: killing process now");
                kill(0, SIGKILL);
            } else {
                clean_actor_system_exit.unlock();
            }
        });
    }
#endif

    void stop() {
#ifdef _WIN32
            spdlog::debug("ExitTimeoutKiller stop ignored");
        }
#else
        // unlock the  mutex so exit_timeout won't time-out
        clean_actor_system_exit.unlock();
        if (exit_timeout.joinable())
            exit_timeout.join();

    }

    std::timed_mutex clean_actor_system_exit;
    std::thread exit_timeout;
#endif

} exit_timeout_killer;

#ifdef _WIN32
#include <DbgHelp.h>

void handler(int sig) {
    void *stack[10];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    // Capture the call stack
    WORD frames = CaptureStackBackTrace(0, 10, stack, nullptr);

    // Print out the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    for (int i = 0; i < frames; ++i) {
        DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        SYMBOL_INFO *symbol  = reinterpret_cast<SYMBOL_INFO *>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen   = MAX_SYM_NAME;

        if (SymFromAddr(process, address, nullptr, symbol)) {
            fprintf(stderr, "%d: %s\n", i, symbol->Name);
        } else {
            fprintf(stderr, "%d: [Unknown Symbol]\n", i);
        }
    }

    SymCleanup(process);
    exit(1);
}
#else
void handler(int sig) {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    exit(1);
}
#endif

void my_handler(int s) {
    spdlog::warn("Caught signal {}", s);
    if (shutdown_xstudio) {
        spdlog::warn("Kill with fire.");
        stop_logger();
        std::exit(EXIT_SUCCESS);
    }

    shutdown_xstudio = true;
}

caf::behavior connect_to_remote(caf::event_based_actor *self) {
    return {
        [=](caf::connect_atom, const std::string host, const int port) -> result<caf::actor> {
            auto a = self->system().middleman().remote_actor(host, port);
            if (a)
                return *a;
            return caf::make_error(
                xstudio_error::error, "Can't connect to " + host + ":" + to_string(port));
        },
    };
}


struct CLIArguments {
    args::ArgumentParser parser = {"xstudio. v" PROJECT_VERSION, "Launchs xstudio."};
    args::HelpFlag help         = {parser, "help", "Display this help menu", {'h', "help"}};

    args::PositionalList<std::string> media_paths = {parser, "PATH", "Path to media"};

    args::Flag headless   = {parser, "headless", "Headless mode, no UI", {'e', "headless"}};
    args::Flag player     = {parser, "player", "Player mode, minimal UI", {'p', "player"}};
    args::Flag quick_view = {
        parser,
        "quick-view",
        "Open a quick-view for each supplied media item",
        {'l', "quick-view"}};

    std::unordered_map<std::string, std::string> cmMapValues{
        {"none", "Off"},
        {"ab", "A/B"},
        {"string", "String"},
    };

    args::MapFlag<std::string, std::string> compare = {
        parser,
        "none,ab,string",
        "Put player into compare mode.",
        {'c', "compare"},
        cmMapValues};

    args::Group playhead               = {parser, "Playback options"};
    args::ValueFlag<double> play_rate  = {playhead, "RATE", "Playback rate", {'f', "fps"}};
    args::ValueFlag<double> media_rate = {playhead, "RATE", "Media rate", {"mfps"}};

    args::Group remote_session               = {parser, "Remote session options"};
    args::ValueFlag<std::string> remote_host = {
        remote_session, "HOSTNAME", "Hostname", {"host"}};
    args::ValueFlag<int> remote_port = {remote_session, "PORT", "Port number", {"port"}};
    args::ValueFlag<std::string> session_name = {
        remote_session, "SESSION", "Name of session", {"session"}};

    args::Group misc       = {parser, "Other options"};
    args::Flag new_session = {
        misc, "NEW", "Launch new session, instead of adding to current.", {'n', "new-session"}};
    args::ValueFlag<std::string> playlist_name = {
        misc, "PLAYLIST", "Name for new playist to add media to", {"playlist"}};

    args::ValueFlagList<std::string> cli_pref_paths = {
        misc, "PATH", "Path to preferences", {"pref"}};
    args::Flag debug                     = {misc, "debug", "Debugging mode", {"debug"}};
    args::ValueFlag<std::string> logfile = {
        misc, "PATH", "Write session log to file", {"log-file"}};
    args::Flag disable_vsync = {
        misc, "disable-vsync", "Disable sync to video refresh", {"disable-vsync"}};
    args::Flag reskin = {
        misc, "reskin", "Launch with the new user interface (under construction)", {"reskin"}};
    args::Flag share_opengl_contexts = {
        misc,
        "share-gl-context",
        "Share the same openGl context between main viewer and pop-out viewer.",
        {"share-gl-context"}};

    args::CompletionFlag completion = {parser, {"complete"}};
    void parse_args(int argc, char **argv) {
        try {
            parser.ParseCLI(argc, argv);
        } catch (const args::Completion &e) {
            std::cerr << e.what();
            std::exit(EXIT_FAILURE);
        } catch (const args::Help &) {
            std::cerr << parser;
            std::exit(EXIT_FAILURE);
        } catch (const args::ParseError &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            std::exit(EXIT_FAILURE);
        }
    }
};

struct Launcher {

    Launcher(int argc, char **argv, actor_system &a_system) : system(a_system) {

        cli_args.parse_args(argc, argv);
#ifdef _WIN32
        _putenv_s("QML_IMPORT_TRACE", "0");
#else
        setenv("QML_IMPORT_TRACE", "0", true);
#endif
        signal(SIGSEGV, handler);
        start_logger(
            cli_args.debug.Matched() ? spdlog::level::debug : spdlog::level::info,
            args::get(cli_args.logfile));
        prefs = load_preferences();
        scoped_actor self{system};

        //  uses commandline args.
        actions["new_instance"]          = cli_args.new_session.Matched();
        actions["headless"]              = cli_args.headless.Matched();
        actions["debug"]                 = cli_args.debug.Matched();
        actions["player"]                = cli_args.player.Matched();
        actions["quick_view"]            = cli_args.quick_view.Matched();
        actions["disable_vsync"]         = cli_args.disable_vsync.Matched();
        actions["reskin"]                = cli_args.reskin.Matched();
        actions["share_opengl_contexts"] = cli_args.share_opengl_contexts.Matched();
        actions["compare"] = static_cast<std::string>(args::get(cli_args.compare));

        // check for xstudio url..
        if (args::get(cli_args.media_paths).size() == 1 and
            check_uri_request(args::get(cli_args.media_paths)[0])) {
            if (uri_request == "open_session") {
                actions["open_session"] = true;
                auto p                  = uri_params.find("path");
                if (p != uri_params.end())
                    actions["open_session_path"] = p->second;
                else {
                    spdlog::error("Open session failed, path= required");
                    std::exit(EXIT_FAILURE);
                }
            } else if (uri_request == "add_media") {
                auto p = uri_params.find("compare");
                if (p != uri_params.end())
                    actions["compare"] = p->second;

                // check for unassigned media.
                auto media = uri_params.equal_range("media");
                if (media.first != uri_params.end()) {
                    actions["playlists"]["Untitled Playlist"] = nlohmann::json::array();
                    for (auto m = media.first; m != media.second; ++m) {
                        actions["playlists"]["Untitled Playlist"].push_back(m->second);
                    }
                }

                auto playlists = uri_params.equal_range("playlist");

                for (auto p = playlists.first; p != playlists.second; ++p) {
                    actions["playlists"][p->second] = nlohmann::json::array();
                    auto media = uri_params.equal_range(p->second + "_media");
                    for (auto m = media.first; m != media.second; ++m) {
                        actions["playlists"][p->second].push_back(m->second);
                    }
                }
            }
        } else {
            if (not args::get(cli_args.session_name).empty())
                actions["session_name"] =
                    static_cast<std::string>(args::get(cli_args.session_name));

            if (cli_args.media_rate.Matched())
                actions["set_media_rate"] = static_cast<double>(args::get(cli_args.media_rate));

            if (cli_args.play_rate.Matched())
                actions["set_play_rate"] = static_cast<double>(args::get(cli_args.play_rate));

            if (args::get(cli_args.media_paths).size() == 1 and
                is_session(args::get(cli_args.media_paths)[0])) {
                actions["open_session"]      = true;
                actions["open_session_path"] = args::get(cli_args.media_paths)[0];
            } else {
                // check for media.
                auto playlist_name                  = args::get(cli_args.playlist_name).empty()
                                                          ? "Untitled Playlist"
                                                          : args::get(cli_args.playlist_name);
                actions["playlists"][playlist_name] = nlohmann::json::array();
                if (not args::get(cli_args.media_paths).empty())
                    actions["playlists"][playlist_name] = args::get(cli_args.media_paths);
            }
        }

        // args configured..
        if (not actions["new_instance"]) {
            global_actor = try_reuse_session();
        }

        // failed reuse, spawn new actor..
        if (not global_actor) {
            // force player if single media..
            // auto count = 0;
            // for (const auto &p : actions["playlists"].items())
            //     count += p.value().size();
            // if(count == 1)
            //     actions["player"] = true;
            // DIRTY HACK.. We need a way of controling this from the backend..
            // if(actions.value("player",false))


            // If the presentation mode is enabled on startup, set it
            if (prefs.get("/ui/qml/enable_presentation_mode/value")) {
                prefs.set(
                    "presentation_layout", "/ui/qml/main_window_settings/value/layout_name");
            }

            prefs.set(0, "/ui/qml/second_window_settings/value/visibility");

            actions["new_instance"] = true;

            global_actor = self->spawn<GlobalActor>(static_cast<JsonStore>(prefs));

            // auto gsa                =
            // system.registry().get<caf::actor>(global_store_registry);

            // self->anon_send(gsa, json_store::set_json_atom_v, static_cast<JsonStore>(prefs));

            request_receive<caf::actor>(*self, global_actor, create_studio_atom_v, "XStudio");

            // this isn't great, the api is already running at this point..
            // so we have to toggle it..
            if (not actions["session_name"].empty())
                self->anon_send(
                    global_actor,
                    remote_session_name_atom_v,
                    static_cast<std::string>(actions["session_name"]));
        }

        // check for session file ..
        if (actions["open_session"]) {
            try {
                JsonStore js =
                    utility::open_session(actions["open_session_path"].get<std::string>());

                if (actions["new_instance"]) {
                    spdlog::stopwatch sw;
                    auto new_session = self->spawn<session::SessionActor>(
                        js,
                        posix_path_to_uri(
                            static_cast<std::string>(actions["open_session_path"])));

                    spdlog::info("Session loaded in {:.3} seconds.", sw);
                    request_receive<bool>(
                        *self, global_actor, session::session_atom_v, new_session);
                } else {
                    spdlog::info(
                        "Open in remote session {}",
                        static_cast<std::string>(actions["open_session_path"]));
                    request_receive<bool>(
                        *self,
                        global_actor,
                        session::session_request_atom_v,
                        static_cast<std::string>(actions["open_session_path"]),
                        js);
                }
            } catch (const std::exception &err) {
                spdlog::error("{}", err.what());
                std::exit(EXIT_FAILURE);
            }
        }

        // add playlists/media..
        auto session =
            request_receive<caf::actor>(*self, global_actor, session::session_atom_v);

        if (actions.count("set_play_rate"))
            self->anon_send(
                session,
                playhead::playhead_rate_atom_v,
                FrameRate(1.0 / static_cast<double>(actions["set_play_rate"])));

        if (actions.count("set_media_rate"))
            self->anon_send(
                session,
                session::media_rate_atom_v,
                FrameRate(1.0 / static_cast<double>(actions["set_media_rate"])));

        auto pm =
            request_receive<caf::actor>(*self, global_actor, global::get_plugin_manager_atom_v);


        auto media_sent = false;
        for (const auto &p : actions["playlists"].items()) {
            if (p.value().empty())
                continue;

            caf::actor playlist;

            // If playlist name is "Untitled Playlist" (in other words no playlist
            // was named to add media to) then try and get the current playlist
            if (p.key() == "Untitled Playlist" and not actions["new_instance"]) {
                try {
                    playlist = request_receive<caf::actor>(
                        *self, session, session::current_playlist_atom_v);
                } catch (...) {
                    try {
                        playlist = request_receive<caf::actor>(
                            *self, session, session::get_playlist_atom_v);
                    } catch (...) {
                    }
                }
            }

            if (!playlist)
                playlist = request_receive<caf::actor>(
                    *self, session, session::get_playlist_atom_v, p.key());

            // if not create it
            if (!playlist)
                playlist = request_receive<UuidUuidActor>(
                               *self, session, session::add_playlist_atom_v, p.key())
                               .second.actor();

            send_media(
                self,
                pm,
                session,
                playlist,
                p.value(),
                not actions["new_instance"],
                actions["compare"],
                actions["quick_view"]);

            media_sent = true;
        }

        // reset modified state..
        if (actions["new_instance"]) {
            self->delayed_anon_send(
                session, std::chrono::seconds(3), last_changed_atom_v, utility::time_point());
        } else if (not media_sent) {
            // No op.. warn user..
            spdlog::warn("You are already running xStudio, use -n to launch a new instance.");
        }
    }

    JsonStore load_preferences() {
        std::vector<std::string> pref_paths;
        for (const auto &p : args::get(cli_args.cli_pref_paths)) {
            pref_paths.push_back(p);
        }

        for (const auto &i : global_store::PreferenceContexts)
            pref_paths.push_back(preference_path_context(i));

        auto prefs = JsonStore();
        if (not preference_load_defaults(prefs, xstudio_root("/preference"))) {
            spdlog::error(
                "Failed to load application preferences {}", xstudio_root("/preference"));
            std::exit(EXIT_FAILURE);
        }

        // prefs files *might* be located in a 'preference' subfolder under XSTUDIO_PLUGIN_PATH
        // folders
        char * plugin_path = std::getenv("XSTUDIO_PLUGIN_PATH");
        if (plugin_path) {
            for (const auto &p : xstudio::utility::split(plugin_path, ':')) {
                if (fs::is_directory(p + "/preferences"))
                    preference_load_defaults(prefs, p + "/preferences");
            }
        }

        preference_load_overrides(prefs, pref_paths);
        return prefs;
    }

    bool check_uri_request(const std::string &request) {
        const static std::regex re("xstudio://([^?]+)(\\?(.+))?");

        if (not starts_with(request, "xstudio://"))
            return false;

        std::cmatch m;
        if (std::regex_match(request.c_str(), m, re)) {
            uri_request = httplib::detail::decode_url(m[1], true);
            auto args   = httplib::detail::decode_url(m[3], true);
            // Parse query text
            if (not args.empty())
                httplib::detail::parse_query_text(args, uri_params);
        } else {
            spdlog::error("Invalid request {}.", request);
            std::exit(EXIT_FAILURE);
        }
        return true;
    }

    CLIArguments cli_args;
    nlohmann::json actions = R"(
    {
        "headless": false,
        "new_instance": false,
        "quick_view": false,
        "session_name": "",
        "open_session": false,
        "debug": false,
        "playlists": {}
    })"_json;

    JsonStore prefs;
    actor_system &system;

    std::string uri_request;
    httplib::Params uri_params;

    bool open_session = {false};
    std::string open_session_path;
    caf::actor global_actor;


    std::vector<std::tuple<std::string, std::string, int, int>> build_targets() {
        std::vector<std::tuple<std::string, std::string, int, int>> targets;
        RemoteSessionManager rsm(remote_session_path());

        if (not args::get(cli_args.remote_host).empty()) {
            auto remote_port_tmp =
                (args::get(cli_args.remote_port)
                     ? args::get(cli_args.remote_port)
                     : preference_value<int>(prefs, "/core/api/port_minimum"));
            targets.emplace_back(std::make_tuple(
                std::string("undefined"),
                args::get(cli_args.remote_host),
                remote_port_tmp,
                remote_port_tmp));
        } else if (not static_cast<std::string>(actions["session_name"]).empty()) {
            // scan for session port files.
            // if session specified used that otherwise add local
            auto sname        = static_cast<std::string>(actions["session_name"]);
            auto find_session = rsm.find(sname);
            if (not find_session) {
                spdlog::error("Failed to find session {}", sname);
                stop_logger();
                std::exit(EXIT_FAILURE);
            }
            targets.emplace_back(std::make_tuple(
                sname, find_session->host(), find_session->port(), find_session->port()));
        } else {
            for (const auto &i : rsm.sessions()) {
                if (i.host() == "localhost")
                    targets.emplace_back(
                        std::make_tuple(i.session_name(), i.host(), i.port(), i.port()));
            }
        }

        // don't scan all the ports..
        // if (targets.empty()) {
        //     targets.emplace_back(std::make_tuple(
        //         "localhost",
        //         preference_value<int>(prefs, "/core/api/port_minimum"),
        //         preference_value<int>(prefs, "/core/api/port_maximum")));
        // }

        return targets;
    }

    void send_media(
        scoped_actor &self,
        caf::actor plugin_manager,
        caf::actor session,
        caf::actor playlist,
        const std::vector<std::string> &media,
        const bool remote,
        const std::string compare_mode,
        const bool open_quick_view) {

        std::vector<std::pair<caf::uri, FrameList>> uri_fl;
        std::vector<std::string> files;

        auto media_rate =
            request_receive<FrameRate>(*self, session, session::media_rate_atom_v);
        UuidActorVector added_media;

        for (const auto &p : media) {
            if (utility::check_plugin_uri_request(p)) {
                // send to plugin manager..
                auto uri = caf::make_uri(p);
                if (uri) {
                    try {
                        added_media = request_receive<UuidActorVector>(
                            *self,
                            plugin_manager,
                            data_source::use_data_atom_v,
                            *uri,
                            session,
                            playlist,
                            media_rate);
                    } catch (const std::exception &e) {
                        spdlog::error("Failed to load media '{}'", e.what());
                    
                    
                    }
            
                } else {
                    spdlog::warn("Invalid URI {}", p);
                }
            } else {
                // check for dir..
                try {
                    // test for dir..
                    try {
                        if (fs::is_directory(p)) {
                            auto items = scan_posix_path(p);
                            uri_fl.insert(uri_fl.end(), items.begin(), items.end());
                            continue;
                        } else if (fs::is_regular_file(p)) {
                            files.push_back(p);
                            continue;
                        }
                    } catch ([[maybe_unused]] const std::exception &err) {
                    }

                    // add to scan list..
                    FrameList fl;
                    caf::uri uri = parse_cli_posix_path(p, fl, true);
                    uri_fl.emplace_back(std::make_pair(uri, fl));

                } catch (const std::exception &e) {
                    spdlog::error("Failed to load media '{}'", e.what());
                }
            }
        }

        if (not files.empty()) {
            auto file_items = uri_from_file_list(files);
            uri_fl.insert(uri_fl.end(), file_items.begin(), file_items.end());
        }

        if (not open_quick_view && not compare_mode.empty()) {

            // To set compare mode, we must have a playhead (which is where
            // compare mode setting is held)

            // get the playlist's playhead
            caf::actor playhead =
                request_receive<UuidActor>(*self, playlist, playlist::get_playhead_atom_v)
                    .actor();

            // set the playhead to the given compare mode. The compare mode
            // attribute is called 'Compare' - we can set it using this handy
            // message handler inherited from the Module class.
            self->anon_send(
                playhead,
                module::change_attribute_value_atom_v,
                std::string("Compare"),
                utility::JsonStore(compare_mode),
                true);
        }

        for (const auto &i : uri_fl) {
            try {
                added_media.push_back(request_receive<UuidActor>(
                    *self,
                    playlist,
                    playlist::add_media_atom_v,
                    uri_to_posix_path(i.first),
                    i.first,
                    i.second,
                    Uuid()));

                if (remote)
                    spdlog::info("{} sent to running session.", uri_to_posix_path(i.first));

            } catch (const std::exception &e) {
                spdlog::error("Failed to load media '{}'", e.what());
            }
        }

        // get the actor that is responsible for selecting items from the playlist
        // for viewing
        if (not open_quick_view && not compare_mode.empty()) {
            auto playhead_selection_actor =
                request_receive<caf::actor>(*self, playlist, playlist::selection_actor_atom_v);

            if (playhead_selection_actor) {
                // Reset the current selection so that the added media is what is
                // selected.
                UuidList selection;
                for (auto &new_media : added_media) {
                    selection.push_back(new_media.uuid());
                }
                self->anon_send(
                    playhead_selection_actor, playlist::select_media_atom_v, selection);
            }
        }

        // to ensure what we've added appears on screen we need to
        // make the playlist the 'current' one - i.e. the one being viewer
        anon_send(session, session::current_playlist_atom_v, playlist);


        // even if 'open_quick_view' is false, we send a message to the session
        // because auto-opening of quickview can be controlled via a preference

        anon_send(
            session,
            ui::open_quickview_window_atom_v,
            added_media,
            compare_mode,
            open_quick_view);
    }

    caf::actor try_reuse_session() {
        // try and connect to running instance by connecting to API..
        auto targets = build_targets();
        scoped_actor self{system};

        if (system.has_middleman()) {
            for (const auto &[name, host, ports, porte] : targets) {
                for (auto port = ports; port <= porte; port++) {
                    // try open ?
                    auto connecting = system.spawn(connect_to_remote);
                    try {
                        auto a = request_receive_wait<caf::actor>(
                            *self,
                            connecting,
                            std::chrono::seconds(2),
                            caf::connect_atom_v,
                            host,
                            port);
                        spdlog::info("Connected to session '{}' at {}:{}", name, host, port);
                        return a;
                    } catch (const std::exception &err) {
                        spdlog::debug("Failed to connect '{}'", err.what());
                    }
                }
            }
        } else {
            spdlog::error("Failed to connect to middleman.");
            stop_logger();
            std::exit(EXIT_FAILURE);
        }

        if (not args::get(cli_args.remote_host).empty()) {
            spdlog::error(
                "Failed to connect to session at {}:{}",
                args::get(cli_args.remote_host),
                args::get(cli_args.remote_port));
            stop_logger();
            std::exit(EXIT_FAILURE);
        }

        if (not static_cast<std::string>(actions["session_name"]).empty()) {
            spdlog::error(
                "Failed to connect to session  {}",
                static_cast<std::string>(actions["session_name"]));
            stop_logger();
            std::exit(EXIT_FAILURE);
        }

        return caf::actor();
    }
};

int main(int argc, char **argv) {

    ACTOR_INIT_GLOBAL_META()
    core::init_global_meta_objects();
    io::middleman::init_global_meta_objects();

    // As far as I can tell caf only allows config to be modified
    // through cli args. We prefer the 'sharing' policy rather than
    // 'stealing'. The latter results in multiple threads spinning
    // aggressively pushing CPU load very high during playback.

    // const char *args[] = {argv[0],
    // "--caf.work-stealing.aggressive-poll-attempts=0","--caf.logger.console.verbosity=trace"};

    const char *args[] = {
        argv[0],
        "--caf.scheduler.max-threads=128",
        "--caf.scheduler.policy=sharing",
        "--caf.logger.console.verbosity=trace"};

    caf_config config{4, const_cast<char **>(args)};

    config.add_actor_type<timeline::GapActor, const std::string &>("Gap");
    config.add_actor_type<timeline::StackActor, const std::string &>("Stack");
    config.add_actor_type<timeline::ClipActor, const utility::UuidActor &, const std::string &>(
        "Clip");
    config.add_actor_type<timeline::TrackActor, const std::string &, const media::MediaType &>(
        "Track");

    {

        try {

            // create the actor system
            actor_system system(config);

            // store a reference to the actor system, so we can access it
            // via static method anywhere else we need to (mainly, the python
            // module instanced in the embedded python interpreter)
            utility::ActorSystemSingleton::actor_system_ref(system);

            scoped_actor self{system};
            Launcher l(argc, argv, system);

            // add to current session and exit
            if (not l.actions["new_instance"]) {
                stop_logger();
                std::exit(EXIT_SUCCESS);
            }

            if (l.actions["headless"]) {
                system.await_actors_before_shutdown(true);
                //TODO: Ahead Fix
                //struct sigaction sigIntHandler;

                //sigIntHandler.sa_handler = my_handler;
                //sigemptyset(&sigIntHandler.sa_mask);
                //sigIntHandler.sa_flags = 0;

                //sigaction(SIGINT, &sigIntHandler, nullptr);

                while (not shutdown_xstudio) {
                    // we should be able to shutdown via a API call..
                    try {
                        request_receive<StatusType>(*self, l.global_actor, status_atom_v);
                    } catch (...) {
                        // global actor is dead, probably shutdown via API
                        break;
                    }
                    std::this_thread::sleep_for(1s);
                }
                // cancel inflight events.
                system.clock().cancel_all();
                if (shutdown_xstudio)
                    self->send_exit(l.global_actor, caf::exit_reason::user_shutdown);
                std::this_thread::sleep_for(1s);
            } else {
                system.await_actors_before_shutdown(true);

                QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
                // QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
                // QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
                // QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);

                if (l.actions["disable_vsync"]) {
                    QSurfaceFormat format;
                    format.setSwapInterval(0);
                    QSurfaceFormat::setDefaultFormat(format);
                }

                if (l.actions["share_opengl_contexts"]) {
                    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
                }

                QApplication app(argc, argv);
                app.setOrganizationName("DNEG");
                app.setOrganizationDomain("dneg.com");
                app.setApplicationVersion(PROJECT_VERSION);
                app.setApplicationName("xStudio");
                app.setWindowIcon(QIcon(":images/xstudio_logo_256_v1.svg"));

                // auto palette =  app.palette();
                // palette.setColor(QPalette::Normal, QPalette::Highlight, QColor("Red"));
                // palette.setColor(QPalette::Normal, QPalette::HighlightedText, QColor("Red"));
                // app.setPalette(palette);

                qmlRegisterType<SemVer>("xstudio.qml.semver", 1, 0, "SemVer");
                qmlRegisterType<CursorPosProvider>(
                    "xstudio.qml.cursor_pos_provider", 1, 0, "CursorPosProvider");
                qmlRegisterType<HotkeyUI>("xstudio.qml.viewport", 1, 0, "XsHotkey");
                qmlRegisterType<HotkeysUI>("xstudio.qml.viewport", 1, 0, "XsHotkeysInfo");
                qmlRegisterType<HotkeyReferenceUI>(
                    "xstudio.qml.viewport", 1, 0, "XsHotkeyReference");

                qmlRegisterType<QMLViewport>("xstudio.qml.viewport", 1, 0, "Viewport");

                qmlRegisterType<BookmarkCategoryModel>(
                    "xstudio.qml.bookmarks", 1, 0, "XsBookmarkCategories");
                qmlRegisterType<BookmarkModel>(
                    "xstudio.qml.bookmarks", 1, 0, "XsBookmarkModel");
                qmlRegisterType<BookmarkFilterModel>(
                    "xstudio.qml.bookmarks", 1, 0, "XsBookmarkFilterModel");

                qmlRegisterType<EmbeddedPythonUI>(
                    "xstudio.qml.embedded_python", 1, 0, "EmbeddedPython");

                qmlRegisterType<QMLUuid>("xstudio.qml.uuid", 1, 0, "QMLUuid");
                qmlRegisterType<ClipboardProxy>("xstudio.qml.clipboard", 1, 0, "Clipboard");

                qmlRegisterType<ModuleAttrsModel>(
                    "xstudio.qml.module", 1, 0, "XsModuleAttributesModel");
                qmlRegisterType<OrderedModuleAttrsModel>(
                    "xstudio.qml.module", 1, 0, "XsOrderedModuleAttributesModel");
                qmlRegisterType<ModuleAttrsDirect>(
                    "xstudio.qml.module", 1, 0, "XsModuleAttributes");
                qmlRegisterType<ModuleMenusModel>("xstudio.qml.module", 1, 0, "XsModuleMenu");
                qmlRegisterType<EventAttrs>("xstudio.qml.event", 1, 0, "XsEvent");

                qmlRegisterType<GlobalStoreModel>(
                    "xstudio.qml.global_store_model", 1, 0, "XsGlobalStoreModel");
                qmlRegisterType<ModelProperty>("xstudio.qml.helpers", 1, 0, "XsModelProperty");
                qmlRegisterType<ModelRowCount>("xstudio.qml.helpers", 1, 0, "XsModelRowCount");
                qmlRegisterType<ModelPropertyMap>(
                    "xstudio.qml.helpers", 1, 0, "XsModelPropertyMap");
                qmlRegisterType<ModelNestedPropertyMap>(
                    "xstudio.qml.helpers", 1, 0, "XsModelNestedPropertyMap");
                qmlRegisterType<ModelPropertyTree>(
                    "xstudio.qml.helpers", 1, 0, "XsModelPropertyTree");

                qmlRegisterType<SessionModel>("xstudio.qml.session", 1, 0, "XsSessionModel");

                qmlRegisterType<SnapshotModel>("xstudio.qml.models", 1, 0, "XsSnapshotModel");

                qmlRegisterType<MenusModelData>("xstudio.qml.models", 1, 0, "XsMenusModel");
                qmlRegisterType<ModulesModelData>("xstudio.qml.models", 1, 0, "XsModuleData");
                qmlRegisterType<ReskinPanelsModel>(
                    "xstudio.qml.models", 1, 0, "XsReskinPanelsLayoutModel");
                qmlRegisterType<MediaListColumnsModel>(
                    "xstudio.qml.models", 1, 0, "XsMediaListColumnsModel");

                qmlRegisterType<ViewsModelData>("xstudio.qml.models", 1, 0, "XsViewsModel");

                qmlRegisterType<MenuModelItem>("xstudio.qml.models", 1, 0, "XsMenuModelItem");

                qRegisterMetaType<QQmlPropertyMap *>("QQmlPropertyMap*");


                QuickFuture::registerType<QUrl>();
                QuickFuture::registerType<QUuid>();
                QuickFuture::registerType<QList<QUuid>>();

                // Add a CafSystemObject to the application - this is QObject that simply
                // holds a reference to the actor system so that we can access the system
                // in Qt main loop
                new CafSystemObject(&app, system);

                const QUrl url(
                    l.actions["reskin"] ? QStringLiteral("qrc:/main_reskin.qml")
                                        : QStringLiteral("qrc:/main.qml"));

                QQmlApplicationEngine engine;
                engine.addImageProvider(QLatin1String("thumbnail"), new ThumbnailProvider);
                engine.addImageProvider(QLatin1String("shotgun"), new ShotgunProvider);
                engine.rootContext()->setContextProperty(
                    "applicationDirPath", QGuiApplication::applicationDirPath());

                auto helper = new Helpers(&engine);
                engine.rootContext()->setContextProperty("helpers", helper);

                auto studio = new StudioUI(system, &app);
                engine.rootContext()->setContextProperty("studio", studio);

                auto logger      = new LogModel(&engine);
                auto proxylogger = new LogFilterModel(&engine);
                proxylogger->setSourceModel(logger);

                engine.rootContext()->setContextProperty(
                    "CurrentDirPath", QString(QDir::currentPath()));
                engine.rootContext()->setContextProperty("logger", proxylogger);
                // connect logger.
                auto logsink = std::make_shared<spdlog::sinks::qtlog_sink_mt>(logger);
                spdlog::get("xstudio")->sinks().push_back(logsink);

                engine.addImportPath("qrc:///");
                engine.addImportPath("qrc:///extern");

                // gui plugins..
                engine.addImportPath(QStringFromStd(xstudio_root("/plugin/qml")));
                engine.addPluginPath(QStringFromStd(xstudio_root("/plugin/qml")));

                char * plugin_path = std::getenv("XSTUDIO_PLUGIN_PATH");
                if (plugin_path) {
                    for (const auto &p : xstudio::utility::split(plugin_path, ':')) {
                        engine.addPluginPath(QStringFromStd(p + "/qml"));
                        engine.addImportPath(QStringFromStd(p + "/qml"));
                    }
                }

                QObject::connect(
                    &engine,
                    &QQmlApplicationEngine::objectCreated,
                    &app,
                    [url](QObject *obj, const QUrl &objUrl) {
                        if (!obj && url == objUrl) {
                            QCoreApplication::exit(-1);
                        }
                    },
                    Qt::QueuedConnection);

                engine.load(url);
                spdlog::info("XStudio UI launched.");

                app.exec();
                // fingers crossed...
                // need to stop monitoring or we'll be sending events to a dead QtObject
                spdlog::get("xstudio")->sinks().pop_back();
                // save state.. BUT NOT WHEN USING PLAYER MODE.. (THIS needs work)
                // if we store prefs then we affect the normal mode.. (DUAL mode for prefs ?)
                if (not l.actions.value("player", false)) {
                    for (const auto &context : global_store::PreferenceContexts) {
                        try {
                            request_receive<bool>(
                                *self,
                                system.registry().get<caf::actor>(global_store_registry),
                                global_store::save_atom_v,
                                context);
                        } catch (const std::exception &err) {
                            spdlog::warn("Failed to save prefs {}", err.what());
                        }
                    }
                }
                // cancel actors talking to them selves.
                system.clock().cancel_all();
                self->send_exit(l.global_actor, caf::exit_reason::user_shutdown);
                std::this_thread::sleep_for(1s);
            }

            // in the case where ther are actors that are still 'alive'
            // we do not exit this scope as actor_system will block in
            // its destructor (due to await_actors_before_shutdown(true)).
            // The exit_timeout_killer will kill the process after some
            // delay so we don't have zombie xstudio instances running.
            exit_timeout_killer.start();

        } catch (const std::exception &err) {
            spdlog::critical("{} {}", __PRETTY_FUNCTION__, err.what());
            stop_logger();
            std::exit(EXIT_FAILURE);
        }

        exit_timeout_killer.stop();
    }
    stop_logger();

    std::exit(EXIT_SUCCESS);
}
