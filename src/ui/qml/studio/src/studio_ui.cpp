// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/ui/qml/studio_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

StudioUI::StudioUI(caf::actor_system &system, QObject *parent) : QMLActor(parent) {
    init(system);

    // create the offscreen viewport used for snapshot and note thumbnail
    // generation
    offscreen_snapshot_viewport();
}

StudioUI::~StudioUI() {
    caf::scoped_actor sys(system());
    for (auto output_plugin : video_output_plugins_) {
        sys->send_exit(output_plugin, caf::exit_reason::user_shutdown);
    }
    video_output_plugins_.clear();
    // Ofscreen viewports are unparented as they are running
    // in their own threads. Schedule deletion here.
    for (auto vp : offscreen_viewports_) {
        vp->stop();
    }
    system().registry().erase(studio_ui_registry);
    snapshot_offscreen_viewport_->stop();
}

void StudioUI::init(actor_system &system_) {
    QMLActor::init(system_);

    spdlog::debug("StudioUI init");


    // join application level events.
    {
        scoped_actor sys{system()};
        try {
            auto grp = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(studio_registry),
                utility::get_event_group_atom_v);
            request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, as_actor());

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    // get current session..

    try {
        scoped_actor sys{system()};
        // Get the session ...
        auto global = system().registry().template get<caf::actor>(global_registry);

        auto session = request_receive_wait<caf::actor>(
            *sys, global, std::chrono::seconds(1), session::session_atom_v);
        setSessionActorAddr(actorToQString(system(), session));

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    updateDataSources();

    // put ourselves in the registry
    system().registry().template put<caf::actor>(studio_ui_registry, as_actor());

    self()->set_down_handler([=](down_msg &msg) {
        for (auto p = video_output_plugins_.begin(); p != video_output_plugins_.end(); ++p) {
            if (msg.source == *p) {
                video_output_plugins_.erase(p);
                break;
            }
        }
    });

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](utility::event_atom, utility::change_atom) {},
            [=](utility::event_atom,
                session::session_request_atom,
                const std::string path,
                const JsonStore &js) {
                emit sessionRequest(
                    QUrlFromUri(posix_path_to_uri(path)), QByteArray(js.dump().c_str()));
            },

            [=](utility::event_atom, session::session_atom, caf::actor session) {
                setSessionActorAddr(actorToQString(system(), session));
            },

            [=](utility::event_atom,
                ui::open_quickview_window_atom,
                const utility::UuidActorVector &media_items,
                const std::string &compare_mode,
                const utility::JsonStore in_point,
                const utility::JsonStore out_point) {
                QStringList media_actors_as_strings;
                for (const auto &media : media_items) {
                    media_actors_as_strings.push_back(
                        QStringFromStd(actorToString(system(), media.actor())));
                }
                const int in  = in_point.is_number() ? in_point.get<int>() : -1;
                const int out = out_point.is_number() ? out_point.get<int>() : -1;
                emit openQuickViewers(
                    media_actors_as_strings, QStringFromStd(compare_mode), in, out);
            },

            [=](utility::event_atom,
                ui::show_message_box_atom,
                const std::string &message_title,
                const std::string &message_body,
                const bool close_button,
                const int timeout_seconds) {
                emit showMessageBox(
                    QStringFromStd(message_title),
                    QStringFromStd(message_body),
                    close_button,
                    timeout_seconds);
            },

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](ui::offscreen_viewport_atom, const std::string name) -> caf::actor {
                // create a new offscreen viewport and return the actor handle
                offscreen_viewports_.push_back(new xstudio::ui::qt::OffscreenViewport(name));
                return offscreen_viewports_.back()->as_actor();
            },

            [=](ui::offscreen_viewport_atom, const std::string name, caf::actor requester) {
                // create a new offscreen viewport and send it back to the 'requester' actor.
                // The reason we do it this way is because the requester might be a mixin
                // actor based off a QObject - if so it can't do request/receive message
                // handling with this actor which also lives in the Qt UI thread.
                offscreen_viewports_.push_back(new xstudio::ui::qt::OffscreenViewport(name));
                anon_send(
                    requester,
                    ui::offscreen_viewport_atom_v,
                    offscreen_viewports_.back()->as_actor());
            }

        };
    });

    // here we tell the studio that we're up and running so it can send us
    // any pending 'quickview' requests
    auto studio = system().registry().template get<caf::actor>(studio_registry);
    if (studio) {
        anon_send(studio, ui::open_quickview_window_atom_v, as_actor());
    }
}

void StudioUI::setSessionActorAddr(const QString &addr) {
    if (addr != session_actor_addr_) {
        session_actor_addr_ = addr;
        emit sessionActorAddrChanged();
    }
}


bool StudioUI::clearImageCache() {
    // get global cache.
    auto ic     = system().registry().template get<caf::actor>(image_cache_registry);
    auto result = false;
    try {
        scoped_actor sys{system()};
        result = request_receive<bool>(*sys, ic, clear_atom_v);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}


QUrl StudioUI::userDocsUrl() const {
    std::string docs_index = utility::xstudio_root("/docs/user_docs/index.html");
    return QUrl(QString(tr("file://")) + QString(docs_index.c_str()));
}

QUrl StudioUI::apiDocsUrl() const {
    std::string docs_index = utility::xstudio_root("/docs/index.html");
    return QUrl(QString(tr("file://")) + QString(docs_index.c_str()));
}

QUrl StudioUI::releaseDocsUrl() const {
    std::string docs_index = utility::xstudio_root("/user_docs/release_notes/index.html");
    return QUrl(QString(tr("file://")) + QString(docs_index.c_str()));
}

QUrl StudioUI::hotKeyDocsUrl() const {
    std::string docs_index =
        utility::xstudio_root("/docs/user_docs/getting_started/hotkeys.html");
    return QUrl(QString(tr("file://")) + QString(docs_index.c_str()));
}

void StudioUI::newSession(const QString &name) {
    scoped_actor sys{system()};

    auto session = sys->spawn<session::SessionActor>(StdFromQString(name));
    auto global  = system().registry().template get<caf::actor>(global_registry);
    sys->anon_send(global, session::session_atom_v, session);

    setSessionActorAddr(actorToQString(system(), session));
    emit newSessionCreated(session_actor_addr_);
}

QFuture<bool> StudioUI::loadSessionFuture(const QUrl &path, const QVariant &json) {
    return QtConcurrent::run([=]() {
        auto result = false;

        try {
            scoped_actor sys{system()};
            JsonStore js;

            if (json.isNull()) {
                js = utility::open_session(UriFromQUrl(path));
            } else {
                js = qvariant_to_json(json);
            }

            auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));
            auto global  = system().registry().template get<caf::actor>(global_registry);
            sys->anon_send(global, session::session_atom_v, session);
            setSessionActorAddr(actorToQString(system(), session));
            emit sessionLoaded(session_actor_addr_);
            result = true;

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return result;
    });
}

QFuture<bool> StudioUI::loadSessionRequestFuture(const QUrl &path) {
    return QtConcurrent::run([=]() {
        auto result = false;
        try {
            scoped_actor sys{system()};
            JsonStore js = utility::open_session(StdFromQString(path.path()));

            // if current session is empty load.
            // else notify UI
            auto global = system().registry().template get<caf::actor>(global_registry);
            // get current session.
            auto old_session =
                request_receive<caf::actor>(*sys, global, session::session_atom_v);

            // check empty..
            try {
                request_receive<caf::actor>(*sys, old_session, session::get_playlist_atom_v);
                // has content trigger request.
                request_receive<bool>(
                    *sys,
                    global,
                    session::session_request_atom_v,
                    uri_to_posix_path(UriFromQUrl(path)),
                    js);
            } catch (...) {
                // empty..
                auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));
                sys->anon_send(global, session::session_atom_v, session);
                setSessionActorAddr(actorToQString(system(), session));
                emit sessionLoaded(session_actor_addr_);
            }

            result = true;

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return result;
    });
}


void StudioUI::updateDataSources() {

    try {
        scoped_actor sys{system()};
        bool changed = false;

        // connect to plugin manager, acquire enabled datasource plugins
        // watch for changes..
        auto pm      = system().registry().template get<caf::actor>(plugin_manager_registry);
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_DATA_SOURCE));

        for (const auto &i : details) {
            try {
                bool found = false;
                for (const auto &ii : data_sources_) {
                    if (QUuidFromUuid(i.uuid_) == dynamic_cast<Plugin *>(ii)->uuid()) {
                        found = true;
                        break;
                    }
                }

                auto ui_str = request_receive<std::tuple<std::string, std::string>>(
                    *sys, pm, plugin_manager::spawn_plugin_ui_atom_v, i.uuid_);
                auto [widget, menu] = ui_str;

                if (i.enabled_ && not found && widget != "") {
                    changed      = true;
                    auto backend = request_receive<caf::actor>(
                        *sys, pm, plugin_manager::get_resident_atom_v, i.uuid_);
                    auto plugin = new Plugin(this);
                    plugin->setUuid(QUuidFromUuid(i.uuid_));
                    plugin->setQmlName(QStringFromStd(i.name_));
                    plugin->setQmlWidgetString(QStringFromStd(widget));
                    plugin->setQmlMenuString(QStringFromStd(menu));
                    plugin->setBackend(backend);
                    data_sources_.append(plugin);
                }

            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, to_string(i.uuid_), err.what());
            }
        }
        if (changed)
            emit dataSourcesChanged();
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void StudioUI::loadVideoOutputPlugins() {

    try {
        scoped_actor sys{system()};
        bool changed = false;

        // connect to plugin manager, acquire enabled datasource plugins
        // watch for changes..
        auto pm      = system().registry().template get<caf::actor>(plugin_manager_registry);
        auto details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys,
            pm,
            utility::detail_atom_v,
            plugin_manager::PluginType(plugin_manager::PluginFlags::PF_VIDEO_OUTPUT));

        for (const auto &i : details) {

            auto video_output_plugin = request_receive<caf::actor>(
                *sys, pm, plugin_manager::spawn_plugin_atom_v, i.uuid_);
            self()->monitor(video_output_plugin);
            video_output_plugins_.push_back(video_output_plugin);
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

xstudio::ui::qt::OffscreenViewport *StudioUI::offscreen_snapshot_viewport() {
    // create an offscreen viewport and send its companion actor to the actor that requested it
    if (!snapshot_offscreen_viewport_) {
        snapshot_offscreen_viewport_ = new xstudio::ui::qt::OffscreenViewport(
            "snapshot_viewport",
            false // this flag means we don't have QML overlays in the snapshot viewport
        );
        system().registry().put(
            offscreen_viewport_registry, snapshot_offscreen_viewport_->as_actor());
    }
    return snapshot_offscreen_viewport_;
}

QString StudioUI::renderScreenShotToDisk(
    const QUrl &path, const int compression, const int width, const int height) {

    try {

        scoped_actor sys{system()};

        request_receive<bool>(
            *sys,
            offscreen_snapshot_viewport()->as_actor(),
            viewport::render_viewport_to_image_atom_v,
            UriFromQUrl(path),
            width,
            height);

    } catch (std::exception &e) {
        return QString(e.what());
    }
    return QString();
}

QString StudioUI::renderScreenShotToClipboard(const int width, const int height) {

    try {

        scoped_actor sys{system()};

        request_receive<bool>(
            *sys,
            offscreen_snapshot_viewport()->as_actor(),
            viewport::render_viewport_to_image_atom_v,
            width,
            height);

    } catch (std::exception &e) {
        return QString(e.what());
    }
    return QString();
}

void StudioUI::setupSnapshotViewport(const QString &playhead_addr) {

    try {

        scoped_actor sys{system()};
        offscreen_snapshot_viewport()->setPlayhead(playhead_addr);

    } catch (std::exception &e) {
        spdlog::warn("{} {} ", __PRETTY_FUNCTION__, e.what());
    }
}
