// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/embedded_python_ui.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/global_store/global_store.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;


EmbeddedPythonUI::EmbeddedPythonUI(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    setRoleNames(std::vector<std::string>({
        "nameRole",
        "menuPathRole",
        "scriptPathRole",
        "snippetTypeRole",
        "typeRole",
    }));
}


bool SnippetFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    auto result       = true;
    auto source_index = sourceModel()->index(source_row, 0, source_parent);

    if (not snippet_type_.isEmpty() and
        source_index.data(EmbeddedPythonUI::Roles::snippetTypeRole).toString() != snippet_type_)
        result = false;

    return result;
}

void EmbeddedPythonUI::set_backend(caf::actor backend) {
    spdlog::debug("EmbeddedPythonUI set_backend");
    scoped_actor sys{system()};

    try {
        backend_ = backend;
        // join events.
        if (backend_events_) {
            try {
                request_receive<bool>(
                    *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
            } catch ([[maybe_unused]] const std::exception &) {
            }
            backend_events_ = caf::actor();
        }
        try {
            backend_events_ =
                request_receive<caf::actor>(*sys, backend_, get_event_group_atom_v);
            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        auto uuids    = request_receive<UuidVector>(*sys, backend_, json_store::sync_atom_v);
        snippet_uuid_ = uuids[0];

        // get system presets
        auto data =
            request_receive<JsonStore>(*sys, backend_, json_store::sync_atom_v, snippet_uuid_);
        setModelData(data);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // // capture snippets..
    // try {
    //     // we're doing this a bit oddly...
    //     // as prefs isn't the place for this..
    //     // should have a dir for it ?

    //     auto prefs = global_store::GlobalStoreHelper(system());
    //     JsonStore j;
    //     prefs.get_group(j);

    //     auto paths = global_store::preference_value<JsonStore>(
    //         j, "/ui/qml/reskin_windows_and_panels_model");
    //     // process app/user..

    //     JsonStore snippets;

    //     if (check_create_path(xstudio_root("/snippets"))) {
    //         snippets.merge(merge_json_from_path(xstudio_root("/snippets")));
    //     }

    //     for (const auto &i : paths) {
    //         try {
    //             snippets.merge(merge_json_from_path(i));
    //         } catch (...) {
    //         }
    //     }

    //     if (check_create_path(snippets_path())) {
    //         snippets.merge(merge_json_from_path(snippets_path()));
    //     }

    //     // for each path scan for snippet files, build into snippet dict.
    //     for (const auto &i : snippets.items()) {
    //         addSnippet(new SnippetUI(i.value(), this));
    //     }
    // } catch (const std::exception &e) {
    //     spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    // }

    emit backendChanged();
}

QUuid EmbeddedPythonUI::createSession() {
    if (backend_) {
        scoped_actor sys{system()};
        // spdlog::warn("pyEval {0}", str.toStdString());
        try {
            auto uuid = request_receive<Uuid>(
                *sys, backend_, embedded_python::python_create_session_atom_v, true);
            event_uuid_ = uuid;
            emit sessionIdChanged();
            return QUuidFromUuid(uuid);
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    } else
        spdlog::warn("No python backend");

    return QUuid();
}

bool EmbeddedPythonUI::sendInput(const QString &str) {
    if (backend_) {
        scoped_actor sys{system()};
        // spdlog::warn("pyEval {0}", str.toStdString());
        try {
            waiting_ = true;
            emit waitingChanged();
            std::string input_string = StdFromQString(str);
            sys->anon_send(
                backend_,
                embedded_python::python_session_input_atom_v,
                event_uuid_,
                input_string);
            return true;
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    } else
        spdlog::warn("No python backend");
    return false;
}

bool EmbeddedPythonUI::sendInterrupt() {
    if (backend_) {
        scoped_actor sys{system()};
        // spdlog::warn("pyEval {0}", str.toStdString());
        try {
            return request_receive<bool>(
                *sys, backend_, embedded_python::python_session_interrupt_atom_v, event_uuid_);
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    } else
        spdlog::warn("No python backend");
    return false;
}

void EmbeddedPythonUI::pyExec(const QString &str) const {
    if (backend_) {
        scoped_actor sys{system()};
        // spdlog::warn("pyExec {0}", str.toStdString());
        sys->anon_send(
            backend_, embedded_python::python_exec_atom_v, str.toStdString(), event_uuid_);
    } else {
        spdlog::warn("No python backend");
    }
}

void EmbeddedPythonUI::reloadSnippets() const {
    if (backend_) {
        scoped_actor sys{system()};
        // spdlog::warn("pyExec {0}", str.toStdString());
        sys->anon_send(backend_, media::rescan_atom_v);
    } else {
        spdlog::warn("No python backend");
    }
}

bool EmbeddedPythonUI::saveSnippet(const QUrl &path, const QString &content) const {
    auto result = false;
    if (backend_) {
        scoped_actor sys{system()};
        // spdlog::warn("pyExec {0}", str.toStdString());
        try {
            result = request_receive<bool>(
                *sys,
                backend_,
                global_store::save_atom_v,
                UriFromQUrl(path),
                StdFromQString(content));
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    } else {
        spdlog::warn("No python backend");
    }

    return result;
}

void EmbeddedPythonUI::pyEvalFile(const QUrl &path) {
    if (backend_) {
        scoped_actor sys{system()};
        auto uri = UriFromQUrl(path);
        // spdlog::warn("pyExec {0}", to_string(uri));
        sys->anon_send(backend_, embedded_python::python_eval_file_atom_v, uri, event_uuid_);
    } else {
        spdlog::warn("No python backend");
    }
}

QVariant EmbeddedPythonUI::pyEval(const QString &str) {
    if (backend_) {
        scoped_actor sys{system()};
        spdlog::warn("pyEval {0}", str.toStdString());
        try {
            return json_to_qvariant(request_receive<JsonStore>(
                *sys, backend_, embedded_python::python_eval_atom_v, str.toStdString()));
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    } else
        spdlog::warn("No python backend");

    return QVariantMap();
}


void EmbeddedPythonUI::init(actor_system &system_) {
    super::init(system_);

    spdlog::debug("EmbeddedPythonUI init");

    try {
        caf::scoped_actor sys(system());
        auto global = system().registry().template get<caf::actor>(global_registry);
        auto actor  = request_receive<caf::actor>(*sys, global, global::get_python_atom_v);
        set_backend(actor);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // self()->set_down_handler([=](down_msg& msg) {
    // 	if(msg.source == store)
    // 		unsubscribe();
    // });

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                // 		if(msg.source == store_events)
                // unsubscribe();
            },
            [=](utility::event_atom,
                json_store::sync_atom,
                const Uuid &uuid,
                const JsonStore &event) {
                try {
                    if (uuid == snippet_uuid_)
                        receiveEvent(event);

                    // spdlog::warn("{}", modelData().dump(2));
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](utility::event_atom,
                embedded_python::python_output_atom,
                const utility::Uuid &uuid,
                const std::tuple<std::string, std::string> &output) {
                if (uuid == event_uuid_) {
                    auto out = std::get<0>(output);
                    auto err = std::get<1>(output);
                    if (not out.empty()) {
                        emit stdoutEvent(QStringFromStd(out));
                    }
                    if (not err.empty()) {
                        emit stderrEvent(QStringFromStd(err));
                    }
                    waiting_ = false;
                    emit waitingChanged();
                }
            },

            [=](utility::event_atom, utility::change_atom) {},

            [=](utility::event_atom, utility::name_atom, const std::string &) {}};
    });
}

QVariant EmbeddedPythonUI::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);

        switch (role) {
        case Roles::nameRole:
        case Qt::DisplayRole:
            if (j.count("name"))
                result = QString::fromStdString(j.at("name"));
            break;

        case Roles::typeRole:
            if (j.count("type"))
                result = QString::fromStdString(j.at("type"));
            break;

        case Roles::snippetTypeRole:
            if (j.count("snippet_type"))
                result = QString::fromStdString(j.at("snippet_type"));
            break;

        case Roles::menuPathRole:
            if (j.count("menu_path"))
                result = QString::fromStdString(j.at("menu_path"));
            break;

        case Roles::scriptPathRole:
            try {
                if (j.count("script_path")) {
                    auto uri = caf::make_uri(j.at("script_path"));
                    if (uri)
                        result = QVariant::fromValue(QUrlFromUri(*uri));
                }
            } catch (...) {
            }
            break;

        default:
            result = JSONTreeModel::data(index, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }

    return result;
}
