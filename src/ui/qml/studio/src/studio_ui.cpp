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

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {}};
    });
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
    std::string docs_index = utility::xstudio_root("/docs/user_docs/release_notes/index.html");
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
                std::ifstream i(StdFromQString(path.path()));
                i >> js;
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
            JsonStore js;

            std::ifstream i(StdFromQString(path.path()));
            i >> js;

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
            *sys, pm, utility::detail_atom_v, plugin_manager::PluginType::PT_DATA_SOURCE);

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
