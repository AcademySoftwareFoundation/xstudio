// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/ui/qml/playlist_ui.hpp"
#include "xstudio/ui/qml/properties_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

CAF_PUSH_WARNINGS
#include <QDir>
#include <QFileInfo>
#include <QJSValue>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

PreferenceSet::PreferenceSet(QObject *parent) : QMLActor(parent) {

    update_timer_ = new QTimer(this);
    update_timer_->setSingleShot(true);
    update_timer_->setInterval(100);

    propertyMap_ = new QQmlPropertyMap(this);
    init(CafSystemObject::get_actor_system());

    QObject::connect(
        propertyMap_,
        SIGNAL(valueChanged(const QString &, const QVariant &)),
        update_timer_,
        SLOT(start()));

    QObject::connect(update_timer_, SIGNAL(timeout()), this, SLOT(push_prefs_to_store()));

    QObject::connect(
        this, SIGNAL(preferencePathChanged()), this, SLOT(build_prefs_from_store()));
}

void PreferenceSet::setPreferencePath(const QString &p) {
    if (preference_path_ != p) {
        preference_path_ = p;
        emit preferencePathChanged();
    }
}

void PreferenceSet::init(caf::actor_system &system) {
    super::init(system);
    // print_on_create(as_actor(), "PreferenceSet");

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                // 		if(msg.source == store_events)
                // unsubscribe();
            },

            [=](utility::event_atom, global_store::autosave_atom, bool) {},
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},

            [=](utility::event_atom,
                json_store::update_atom,
                const JsonStore &change,
                const std::string &path) {
                // incoming change from the backend
                if (path == preference_path_.toStdString() && change != prefs_store_) {

                    prefs_store_ = change;
                    update_qml_prefs();
                }
            },

            [=](utility::event_atom, utility::change_atom) {}};
    });
}

void PreferenceSet::push_prefs_to_store() {

    try {

        auto &vals = prefs_store_["value"];

        if (vals.is_object()) {
            for (auto key : propertyMap_->keys()) {
                if (not propertyMap_->value(key).isNull())
                    vals[key.toStdString()] = qvariant_to_json(propertyMap_->value(key));
            }
            vals["template_key"] = template_path_;
        } else if (
            propertyMap_->contains("value") && not propertyMap_->value("value").isNull()) {
            vals = qvariant_to_json(propertyMap_->value("value"));
        }

        prefs_store_["path"]    = preference_path_.toStdString();
        prefs_store_["context"] = std::vector<std::string>({"QML_UI"});

        if (global_store_) {
            anon_send(
                global_store_,
                json_store::set_json_atom_v,
                prefs_store_,
                preference_path_.toStdString());
        }

    } catch (std::exception &e) {

        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, e.what(), preference_path_.toStdString());
    }
}

void PreferenceSet::build_prefs_from_store() {

    try {

        scoped_actor sys{system()};

        if (!global_store_) {
            auto global = system().registry().template get<caf::actor>(global_registry);

            if (global) {
                global_store_ = request_receive_wait<caf::actor>(
                    *sys, global, std::chrono::seconds(10), global::get_global_store_atom_v);

                auto global_store_events = request_receive_wait<caf::actor>(
                    *sys,
                    global_store_,
                    std::chrono::seconds(10),
                    utility::get_event_group_atom_v);

                request_receive<bool>(
                    *sys, global_store_events, broadcast::join_broadcast_atom_v, as_actor());
            }
        }

        if (global_store_) {
            prefs_store_ = request_receive_wait<utility::JsonStore>(
                *sys,
                global_store_,
                std::chrono::seconds(10),
                json_store::get_json_atom_v,
                preference_path_.toStdString());

            if (template_path_.empty())
                template_path_ = preference_path_.toStdString();

            update_qml_prefs();

            // After we have built our prefs, we need to push back to the
            // global_store because this set of prefs might be dynamic and
            // not defined in the static prefs dictionaries.
            push_prefs_to_store();
        }

    } catch (std::exception &e) {

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

// void PreferenceSet::fetch_prefs_from_store() {

//     try {

//         if (global_store_) {

//             scoped_actor sys{system()};

//             prefs_store_ = request_receive_wait<utility::JsonStore>(
//                 *sys,
//                 global_store_,
//                 std::chrono::seconds(1),
//                 json_store::get_json_atom_v,
//                 preference_path_.toStdString());

//             update_qml_prefs();
//         }

//     } catch (std::exception &e) {

//         spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
//     }
// }

caf::actor_system &PreferenceSet::system() { return self()->home_system(); }

void PreferenceSet::update_qml_prefs() {

    try {

        auto &json = prefs_store_["value"];

        bool changed = false;
        if (json.is_object()) {
            for (auto p = json.begin(); p != json.end(); ++p) {
                auto key = QString::fromStdString(p.key());
                if (propertyMap_->contains(key)) {
                    const QVariant val = json_to_qvariant(p.value());
                    if ((*propertyMap_)[key] != val) {
                        (*propertyMap_)[key] = val;
                        changed              = true;
                    }
                } else {
                    propertyMap_->insert(key, json_to_qvariant(p.value()));
                    changed = true;
                }
            }
        } else {

            if (propertyMap_->contains("value")) {
                const QVariant val = json_to_qvariant(json);
                if ((*propertyMap_)["value"] != val) {
                    (*propertyMap_)["value"] = val;
                    changed                  = true;
                }
            } else {
                propertyMap_->insert("value", json_to_qvariant(json));
                changed = true;
            }
        }
        if (changed) {
            emit(propertiesChanged());
            emit(propertiesChangedFromBackend());
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}
