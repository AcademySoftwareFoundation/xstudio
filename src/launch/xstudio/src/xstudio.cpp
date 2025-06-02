// SPDX-License-Identifier: Apache-2.0

#include <args/args.hxx>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <regex>
#include <thread>

#ifndef _WIN32
#include <execinfo.h>
#include <unistd.h>
#endif

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#ifndef CPPHTTPLIB_ZLIB_SUPPORT
#define CPPHTTPLIB_ZLIB_SUPPORT
#endif
#include <cpp-httplib/httplib.h>

#include <caf/actor_registry.hpp>
#include <caf/actor_system.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/io/middleman.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/caf_utility/caf_setup.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global/xstudio_actor_system.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/studio/studio_actor.hpp"
#include "xstudio/utility/caf_helpers.hpp"
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
#include <QQuickWindow>
#include <QOpenGLWidget>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/studio_ui.hpp" //NOLINT

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

#ifdef __apple__
// this is required to overcome a very odd link
// error on MaxOS (intel)
uint32_t OPENSSL_ia32cap_P[4] = {0};
#endif

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
        CafActorSystem::exit();
        stop_logger();
        std::exit(s);
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

void xstudioQtMessageHandler(
    QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg  = msg.toLocal8Bit();
    const char *file     = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";

    if (!strcmp("qml", context.category)) {
        // qml messages are type = QtDebugMsg but we always want to see these.
        spdlog::info("QML: {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        return;
    }

    switch (type) {
    case QtDebugMsg:
        spdlog::debug(
            "Qt -- {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        spdlog::info(
            "Qt -- {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        // for now, supressing Qt warnings as we get some spurious stuff from QML that has been
        // impossible to track down! Might be fixed with Qt6
        spdlog::debug(
            "Qt -- {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        spdlog::critical(
            "Qt -- {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        spdlog::error(
            "Qt -- {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    default:
        spdlog::info(
            "Qt -- {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    }
}

void execute_xstudio_ui(
    const bool disble_vsync,
    const float ui_scale_factor,
    const bool silence_qt_warnings,
    int argc,
    char **argv) {

    // apply global UI scaling preference here by setting the
    // QT_SCALE_FACTOR env var before creating the QApplication
    if (ui_scale_factor != 1.0f) {
        std::string fstr = fmt::format("{}", ui_scale_factor);
        qputenv("QT_SCALE_FACTOR", fstr.c_str());
    }

    QSurfaceFormat format;
#ifdef __OPENGL_4_1__
    // MacOS is limited to OpenGL 4.1, of course
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
#endif
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    if (disble_vsync) {
        format.setSwapInterval(0);
    } else {
        format.setSwapInterval(1);
    }
    QSurfaceFormat::setDefaultFormat(format);


    if (silence_qt_warnings) {
        qInstallMessageHandler(xstudioQtMessageHandler);
    }

    QApplication app(argc, argv);
    app.setOrganizationName("DNEG");
    app.setOrganizationDomain("dneg.com");
    app.setApplicationVersion(PROJECT_VERSION);
    app.setApplicationName("xStudio");
    app.setWindowIcon(QIcon(":images/xstudio_logo_256_v1.svg"));


    QQmlApplicationEngine engine;

    ui::qml::setup_xstudio_qml_emgine(
        static_cast<QQmlEngine *>(&engine), CafActorSystem::system());

    const QUrl url(QStringLiteral("qrc:/main.qml"));

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

#ifdef _WIN32
    // Note: this is horrible thing is here to force a direct dependency on QOpenGLWidget
    // for xstudio.exe. We need it for Windows packaging.
    // 'windeployqt.exe' otherwise does not recognise that Qt6OpenGLWidgets.dll is a
    // dependency of xstudio.exe and we get a runtime failure. I don't understand why
    // because studio_qml.dll is a direct dependency, viewport_widget.dll is a dependency
    // of that and Qt6OpenGLWidgets.dll is a dependency of viewport_widget.dll
    QOpenGLWidget *dummy = new QOpenGLWidget();
    delete dummy;
#endif

    app.exec();

    spdlog::get("xstudio")->sinks().pop_back();
}


struct CLIArguments {
    args::ArgumentParser parser = {"xstudio. v" PROJECT_VERSION, "Launchs xstudio."};
    args::HelpFlag help         = {parser, "help", "Display this help menu", {'h', "help"}};

    args::PositionalList<std::string> media_paths = {parser, "PATH", "Path to media"};

    args::Flag headless       = {parser, "headless", "Headless mode, no UI", {'e', "headless"}};
    args::Flag user_prefs_off = {
        parser,
        "user-prefs-off",
        "Skip loading and saving of user preferences.",
        {"user-prefs-off"}};

    args::Flag quick_view = {
        parser,
        "quick-view",
        "Open a quick-view for each supplied media item",
        {'l', "quick-view"}};

    args::Flag silence_qt_warnings = {
        parser,
        "silence-qt-warnings",
        "Silences QT warnings normally printed into the terminal. Some QT warnings are not "
        "consequential. Use this flag in a wrapper script to suppress all QT warnings.",
        {"silence-qt-warnings"}};

    std::unordered_map<std::string, std::string> cmMapValues{
        {"none", "Off"},
        {"ab", "A/B"},
        {"string", "String"},
        {"wipe", "Wipe"},
    };

    args::MapFlag<std::string, std::string> compare = {
        parser,
        "none,ab,string,wipe",
        "Put player into compare mode.",
        {'c', "compare"},
        cmMapValues};

    args::Group playhead               = {parser, "Playback options"};
    args::ValueFlag<double> play_rate  = {playhead, "RATE", "Playback rate", {'f', "fps"}};
    args::ValueFlag<double> media_rate = {playhead, "RATE", "Media rate", {"mfps"}};
    args::ValueFlag<int> in_point      = {playhead, "INPT", "In Point (frames)", {"in"}};
    args::ValueFlag<int> out_point     = {playhead, "OUTPT", "Out Point (frames)", {"out"}};

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

    args::ValueFlagList<std::string> cli_override_pref_paths = {
        misc, "PATH", "Path to preferences that override user preferences", {"override-pref"}};

    args::Flag debug                     = {misc, "debug", "Debugging mode", {"debug"}};
    args::ValueFlag<std::string> logfile = {
        misc, "PATH", "Write session log to file", {"log-file"}};
    args::Flag disable_vsync = {
        misc, "disable-vsync", "Disable sync to video refresh", {"disable-vsync"}};

    args::CompletionFlag completion = {parser, {"complete"}};
    void parse_args(int argc, char **argv) {
        try {
            parser.ParseCLI(argc, argv);
        } catch (const args::Completion &e) {
            std::cerr << e.what();
            CafActorSystem::exit();
            std::exit(EXIT_FAILURE);
        } catch (const args::Help &) {
            std::cerr << parser;
            CafActorSystem::exit();
            std::exit(EXIT_FAILURE);
        } catch (const args::ParseError &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            CafActorSystem::exit();
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
        actions["new_instance"]        = cli_args.new_session.Matched();
        actions["headless"]            = cli_args.headless.Matched();
        actions["debug"]               = cli_args.debug.Matched();
        actions["user_prefs_off"]      = cli_args.user_prefs_off.Matched();
        actions["quick_view"]          = cli_args.quick_view.Matched();
        actions["disable_vsync"]       = cli_args.disable_vsync.Matched();
        actions["compare"]             = static_cast<std::string>(args::get(cli_args.compare));
        actions["silence_qt_warnings"] = cli_args.silence_qt_warnings.Matched();

        // check for xstudio url..
        if (args::get(cli_args.media_paths).size() == 1 and
            check_uri_request(args::get(cli_args.media_paths)[0])) {
            if (uri_request == "open_session") {
                actions["open_session"] = true;
                auto p                  = uri_params.find("path");
                auto u                  = uri_params.find("uri");
                if (p != uri_params.end())
                    actions["open_session_path"] = p->second;
                else if (u != uri_params.end()) {
                    auto uri = caf::make_uri(u->second);
                    if (uri)
                        actions["open_session_path"] = uri_to_posix_path(*uri);
                    else {
                        throw std::runtime_error(
                            fmt::format("Open session failed, invalid URI {}", u->second)
                                .c_str());
                    }
                } else {
                    throw std::runtime_error(
                        fmt::format("Open session failed, path= or uri= required"));
                }
            } else if (uri_request == "add_media") {
                auto p = uri_params.find("compare");
                if (p != uri_params.end())
                    actions["compare"] = p->second;

                // check for unassigned media.
                auto media = uri_params.equal_range("media");
                if (media.first != uri_params.end()) {
                    actions["playlists"]["__DEFAULT_PUSH_PLAYLIST__"] = nlohmann::json::array();
                    for (auto m = media.first; m != media.second; ++m) {
                        actions["playlists"]["__DEFAULT_PUSH_PLAYLIST__"].push_back(m->second);
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

            if (cli_args.in_point.Matched())
                actions["in_frame"] = static_cast<int>(args::get(cli_args.in_point));

            if (cli_args.out_point.Matched())
                actions["out_frame"] = static_cast<int>(args::get(cli_args.out_point));

            if (args::get(cli_args.media_paths).size() == 1 and
                is_session(args::get(cli_args.media_paths)[0])) {
                actions["open_session"]      = true;
                actions["open_session_path"] = args::get(cli_args.media_paths)[0];
            } else {
                // check for media.
                auto playlist_name                  = args::get(cli_args.playlist_name).empty()
                                                          ? "__DEFAULT_PUSH_PLAYLIST__"
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

            actions["new_instance"] = true;
            global_actor =
                CafActorSystem::global_actor(true, "XStudio", static_cast<JsonStore>(prefs));

            // this isn't great, the api is already running at this point..
            // so we have to toggle it..
            if (not actions["session_name"].empty())
                anon_mail(
                    remote_session_name_atom_v,
                    static_cast<std::string>(actions["session_name"]))
                    .send(global_actor);
        }

        // check for session file ..
        if (actions["open_session"]) {
            spdlog::stopwatch sw2;

            JsonStore js =
                utility::open_session(actions["open_session_path"].get<std::string>());

            spdlog::info(
                "File {} loaded in {:.3} seconds.",
                actions["open_session_path"].get<std::string>(),
                sw2);

            if (actions["new_instance"]) {
                spdlog::stopwatch sw;
                auto new_session = self->spawn<session::SessionActor>(
                    js,
                    posix_path_to_uri(static_cast<std::string>(actions["open_session_path"])));

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
        }

        // add playlists/media..
        auto session =
            request_receive<caf::actor>(*self, global_actor, session::session_atom_v);

        if (actions.count("set_play_rate"))
            anon_mail(
                playhead::playhead_rate_atom_v,
                FrameRate(1.0 / static_cast<double>(actions["set_play_rate"])))
                .send(session);

        if (actions.count("set_media_rate"))
            anon_mail(
                session::media_rate_atom_v,
                FrameRate(1.0 / static_cast<double>(actions["set_media_rate"])))
                .send(session);

        auto pm =
            request_receive<caf::actor>(*self, global_actor, global::get_plugin_manager_atom_v);


        auto media_sent = false;
        for (const auto &p : actions["playlists"].items()) {
            if (p.value().empty())
                continue;

            // get the playlist to push media to. If p.key() is empty, session
            // will use user prefs to decide which playlist to return.
            caf::actor playlist = request_receive<caf::actor>(
                *self, session, session::get_push_playlist_atom_v, p.key());
            ;

            send_media(
                self,
                pm,
                session,
                playlist,
                p.value(),
                not actions["new_instance"],
                actions["compare"],
                actions["quick_view"],
                actions["in_frame"],
                actions["out_frame"]);

            media_sent = true;
        }

        // reset modified state..
        if (actions["new_instance"]) {
            anon_mail(last_changed_atom_v, utility::time_point())
                .delay(std::chrono::seconds(3))
                .send(session);
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
        std::vector<std::string> override_paths;
        for (const auto &p : args::get(cli_args.cli_override_pref_paths)) {
            override_paths.push_back(p);
        }

        auto prefs = JsonStore();
        if (!global_store::load_preferences(
                prefs, !cli_args.user_prefs_off.Matched(), pref_paths, override_paths)) {
            // xstudio will not work if application preferences could not be loaded
            throw std::runtime_error("Failed to load application preferences.");
        }
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
            throw std::runtime_error(fmt::format("Invalid request {}.", request).c_str());
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
        "playlists": {},
        "in_frame": null,
        "out_frame": null
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
                throw std::runtime_error(
                    fmt::format("Failed to find session {}", sname).c_str());
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
        const bool open_quick_view,
        const nlohmann::json in_frame,
        const nlohmann::json out_frame) {

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

                        // at this point, we must convert paths from local to
                        // absolute
                        std::string _p = p;
                        if (fs::exists(_p) && fs::path(_p).is_relative()) {
                            _p = fs::absolute(_p).string();
                        }
                        if (fs::is_directory(_p)) {
                            auto items = scan_posix_path(_p);
                            uri_fl.insert(uri_fl.end(), items.begin(), items.end());
                            continue;
                        } else if (fs::is_regular_file(_p)) {
                            files.push_back(_p);
                            continue;
                        }

                    } catch ([[maybe_unused]] const std::exception &err) {
                    }

                    // add to scan list..
                    FrameList fl;
                    if (p.find("http") == 0) {
                        // TODO: extend parse_cli_posix_path to handle http protocol.
                        auto uri = caf::make_uri(p);
                        if (uri) {
                            uri_fl.emplace_back(std::make_pair(*uri, fl));
                        }
                    } else {
                        caf::uri uri = parse_cli_posix_path(p, fl, true);
                        uri_fl.emplace_back(std::make_pair(uri, fl));
                    }

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
            anon_mail(
                module::change_attribute_value_atom_v,
                std::string("Compare"),
                utility::JsonStore(compare_mode),
                true)
                .send(playhead);
        }

        for (const auto &i : uri_fl) {
            try {
                // spdlog::warn("{}", to_string(i.first));

                // use the stem of the filepath as the name for the media item.
                // We do a further trim to the first . so that numbered paths
                // like 'some_exr.####.exr' becomes 'some_exr'
                const auto path    = fs::path(uri_to_posix_path(i.first));
                auto filename_stem = path.stem().string();
                const auto dotpos  = filename_stem.find(".");
                if (dotpos && dotpos != std::string::npos) {
                    filename_stem = std::string(filename_stem, 0, dotpos);
                }

                added_media.push_back(request_receive<UuidActor>(
                    *self,
                    playlist,
                    playlist::add_media_atom_v,
                    filename_stem,
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
        // for viewing. If we're doing quickview, we don't want to fiddle with
        // what's on screen though as the media is going to be shown in a
        // quickview window anyway.
        if (not open_quick_view) {

            auto playhead_selection_actor =
                request_receive<caf::actor>(*self, playlist, playlist::selection_actor_atom_v);

            if (playhead_selection_actor) {

                // Reset the current selection so that the added media is what is
                // selected.
                UuidVector selection;
                if (not compare_mode.empty()) {
                    // if we're doing a compare, select all the new media for
                    // comparison
                    selection.reserve(added_media.size());
                    for (auto &new_media : added_media) {
                        selection.push_back(new_media.uuid());
                    }
                } else if (added_media.size()) {
                    // otherwise, select the first media item to whack on screen
                    selection.push_back(added_media.front());
                }
                anon_mail(playlist::select_media_atom_v, selection)
                    .send(playhead_selection_actor);
            }
        }

        if (not open_quick_view) {
            // now set in/out loop points if specified
            const int in  = in_frame.is_number() ? in_frame.get<int>() : -1;
            const int out = out_frame.is_number() ? out_frame.get<int>() : -1;
            caf::actor playhead =
                request_receive<UuidActor>(*self, playlist, playlist::get_playhead_atom_v)
                    .actor();
            if (playhead && (in != -1 || out != -1)) {
                // we delay the send because the playhead will update its in/out
                // points when new media is shown, so we need to wait until the
                // new media is set-up in the playhead
                if (out != -1)
                    anon_mail(playhead::simple_loop_end_atom_v, out)
                        .delay(std::chrono::milliseconds(500))
                        .send(playhead);
                if (in != -1)
                    anon_mail(playhead::simple_loop_start_atom_v, in)
                        .delay(std::chrono::milliseconds(500))
                        .send(playhead);

                anon_mail(playhead::use_loop_range_atom_v, true)
                    .delay(std::chrono::milliseconds(500))
                    .send(playhead);
            }
        }

        // even if 'open_quick_view' is false, we send a message to the session
        // because auto-opening of quickview can be controlled via a preference
        if (open_quick_view) {
            anon_mail(
                ui::open_quickview_window_atom_v,
                added_media,
                compare_mode,
                JsonStore(in_frame),
                JsonStore(out_frame))
                .send(session);
        }
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
                        auto remote = request_receive_wait<caf::actor>(
                            *self,
                            connecting,
                            std::chrono::seconds(2),
                            caf::connect_atom_v,
                            host,
                            port);

                        auto auth = request_receive_wait<caf::actor>(
                            *self, remote, std::chrono::seconds(2), authenticate_atom_v);

                        spdlog::info("Connected to session '{}' at {}:{}", name, host, port);
                        return auth;
                    } catch (const std::exception &err) {
                        spdlog::debug("Failed to connect '{}'", err.what());
                    }
                }
            }
        } else {
            throw std::runtime_error("Failed to connect to middleman.");
        }

        if (not args::get(cli_args.remote_host).empty()) {

            throw std::runtime_error(fmt::format(
                                         "Failed to connect to session at {}:{}",
                                         args::get(cli_args.remote_host),
                                         args::get(cli_args.remote_port))
                                         .c_str());
        }

        if (not static_cast<std::string>(actions["session_name"]).empty()) {

            throw std::runtime_error(fmt::format(
                                         "Failed to connect to session  {}",
                                         static_cast<std::string>(actions["session_name"]))
                                         .c_str());
        }

        return caf::actor();
    }
};

int main(int argc, char **argv) {

    // this call sets up the CAF actor system
    CafActorSystem::instance();

    try {

        // Launcher class handles CLI parsing and communication with already running
        // xstudio instances
        Launcher l(argc, argv, CafActorSystem::system());

        // add to current session and exit
        if (not l.actions["new_instance"]) {
            CafActorSystem::exit();
            stop_logger();
            std::exit(EXIT_SUCCESS);
        }

#ifndef _WIN32
        struct sigaction sigIntHandler;
        sigIntHandler.sa_handler = my_handler;
        sigemptyset(&sigIntHandler.sa_mask);
        sigIntHandler.sa_flags = 0;
        sigaction(SIGINT, &sigIntHandler, nullptr);
#endif

        if (l.actions["headless"]) {

            // Headless mode.

            scoped_actor self{CafActorSystem::system()};

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
            // system.clock().cancel_all();
            if (shutdown_xstudio)
                self->send_exit(l.global_actor, caf::exit_reason::user_shutdown);
            std::this_thread::sleep_for(1s);

        } else {

            // Run the QApplication and launch UI
            execute_xstudio_ui(
                l.actions["disable_vsync"],
                l.prefs.get("/ui/qml/global_ui_scale_factor").value("value", 1.0f),
                l.actions["silence_qt_warnings"],
                argc,
                argv);

            // save state.. BUT NOT WHEN user_prefs_off
            if (not l.actions.value("user_prefs_off", false)) {

                scoped_actor self{CafActorSystem::system()};
                for (const auto &context : global_store::PreferenceContexts) {
                    try {
                        request_receive<bool>(
                            *self,
                            CafActorSystem::system().registry().get<caf::actor>(
                                global_store_registry),
                            global_store::save_atom_v,
                            context);
                    } catch (const std::exception &err) {
                        spdlog::warn("Failed to save prefs {}", err.what());
                    }
                }
            }

            l.global_actor = caf::actor();
        }

    } catch (const std::exception &err) {
        spdlog::critical("{} {}", __PRETTY_FUNCTION__, err.what());
        CafActorSystem::exit();
        stop_logger();
        std::exit(EXIT_FAILURE);
    }

    CafActorSystem::exit();
    stop_logger();
    std::exit(EXIT_SUCCESS);
}
