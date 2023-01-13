// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/qml/global_store_ui.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QtQml/QJSValue>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::global_store;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

// GlobalStoreHelper

GlobalStoreUI::GlobalStoreUI(QObject *parent) : QMLActor(parent) {}

void GlobalStoreUI::init(actor_system &system_) {
    QMLActor::init(system_);
    scoped_actor sys{system()};
    print_on_create(as_actor(), "GlobalStoreUI");

    gsh_ = std::make_shared<global_store::GlobalStoreHelper>(system_);
    caf::actor gsh_group;
    {
        JsonStore tmp;
        gsh_group = gsh_->get_group(tmp);
        preference_store_.set(tmp);
    }
    set_json_string(QStringFromStd(preference_store_.dump(4)));
    // spdlog::debug("{}", preference_store_.dump(4));
    request_receive<bool>(*sys, gsh_group, broadcast::join_broadcast_atom_v, as_actor());

    {

        try {
            auto grp =
                request_receive<caf::actor>(*sys, gsh_->get_actor(), get_event_group_atom_v);
            request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, as_actor());

        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

        try {
            autosave_ = request_receive<bool>(*sys, gsh_->get_actor(), autosave_atom_v);
            emit autosave_changed();
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    emit preferences_changed();

    // self()->set_down_handler([=](down_msg& /*msg*/) {
    // 	// if(msg.source == store)
    // 	// 	unsubscribe();
    // });


    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                // 		if(msg.source == store_events)
                // unsubscribe();
            },
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},

            [=](json_store::update_atom,
                const JsonStore & /*change*/,
                const std::string &path,
                const JsonStore &full) {
                preference_store_.set(full);
                set_json_string(QStringFromStd(preference_store_.dump(4)));

                // when a pref changes just return path ?
                emit preference_changed(QStringFromStd(path));
            },

            [=](json_store::update_atom, const JsonStore &json) {
                preference_store_.set(json);
                set_json_string(QStringFromStd(preference_store_.dump(4)));
                emit preferences_changed();
            },

            [=](utility::event_atom, global_store::autosave_atom, bool enabled) {
                autosave_ = enabled;
                emit autosave_changed();
            },

            [=](utility::event_atom,
                json_store::update_atom,
                const JsonStore &,
                const std::string &) {},

            [=](utility::event_atom, utility::change_atom) {}};
    });
}

bool GlobalStoreUI::save_preferences(const QString &context) {
    return gsh_->save(StdFromQString(context));
}

QString GlobalStoreUI::json_string() const { return json_string_; }

void GlobalStoreUI::set_autosave(const bool enabled) {
    scoped_actor sys{system()};
    autosave_ = enabled;
    sys->send(gsh_->get_actor(), autosave_atom_v, enabled);
    gsh_->set_value(enabled, "/ui/qml/autosave");
}

bool GlobalStoreUI::autosave() const { return autosave_; }

QVariant GlobalStoreUI::get_preference_as_qvariant(
    const std::string &path, const std::string &property) const {
    QVariant v;
    try {
        int ttype = get_qmetatype(preference_datatype(preference_store_, path));

        switch (ttype) {
        case QMetaType::Bool:
            v.setValue(preference_store_.get<bool>(path + "/" + property));
            break;

        case QMetaType::Int:
            v.setValue(preference_store_.get<int>(path + "/" + property));
            break;

        case QMetaType::Double:
            v.setValue(preference_store_.get<double>(path + "/" + property));
            break;

        case QMetaType::QString:
            v.setValue(
                QStringFromStd(preference_store_.get<std::string>(path + "/" + property)));
            break;

        case QMetaType::QJsonDocument: {
            try {
                QJsonDocument d = QJsonDocument::fromJson(
                    QStringFromStd(preference_store_.get<json>(path + "/" + property).dump())
                        .toUtf8());
                if (d.isArray()) {
                    v.setValue(d.array().toVariantList());
                } else if (d.isObject()) {
                    v.setValue(d.object().toVariantMap());
                } else {
                    v.setValue(d);
                }
            } catch (...) {
                QJsonDocument d = QJsonDocument::fromJson(
                    QStringFromStd(preference_store_.get<std::string>(path + "/" + property))
                        .toUtf8());

                if (d.isArray()) {
                    v.setValue(d.array().toVariantList());
                } else if (d.isObject()) {
                    v.setValue(d.object().toVariantMap());
                } else {
                    v.setValue(d);
                }
            }
        } break;

        case QMetaType::UnknownType:
        default:
            spdlog::warn("Unknown datatype {}", preference_datatype(preference_store_, path));
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {} ", __PRETTY_FUNCTION__, path, property, err.what());
    }
    return v;
}

QVariant GlobalStoreUI::get_preference_minimum(const QString &path) const {
    return get_preference_as_qvariant(StdFromQString(path), "minimum");
}

QVariant GlobalStoreUI::get_preference_maximum(const QString &path) const {
    return get_preference_as_qvariant(StdFromQString(path), "maximum");
}

QVariantMap GlobalStoreUI::get_preference(const QString &path) const {
    QVariantMap result;
    try {
        result = QVariantMapFromJson(preference_store_.get(StdFromQString(path)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QVariant GlobalStoreUI::get_preference_key(const QString &path, const QString &key) const {
    return get_preference_as_qvariant(StdFromQString(path), StdFromQString(key));
}

QVariant GlobalStoreUI::get_preference_value(const QString &path) const {
    return get_preference_as_qvariant(StdFromQString(path), "value");
}

QString GlobalStoreUI::get_preference_description(const QString &path) const {
    QString result;
    try {
        result = QStringFromStd(
            preference_store_.get<std::string>(StdFromQString(path) + "/description"));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QString GlobalStoreUI::get_preference_datatype(const QString &path) const {
    QString result;
    try {
        result = QStringFromStd(
            preference_store_.get<std::string>(StdFromQString(path) + "/datatype"));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QVariant GlobalStoreUI::get_preference_default(const QString &path) const {
    return get_preference_as_qvariant(StdFromQString(path), "default_value");
}

int GlobalStoreUI::get_qmetatype(const std::string &datatype) const {
    int ttype = QMetaType::UnknownType;

    if (datatype == "bool")
        ttype = QMetaType::Bool;
    else if (datatype == "int")
        ttype = QMetaType::Int;
    else if (datatype == "double")
        ttype = QMetaType::Double;
    else if (datatype == "string")
        ttype = QMetaType::QString;
    else if (datatype == "json")
        ttype = QMetaType::QJsonDocument;

    return ttype;
}

bool GlobalStoreUI::set_preference_value(
    const QString &path, const QVariant &value, const bool async) {

    bool result             = false;
    const std::string _path = StdFromQString(path);
    // spdlog::debug("{} {}", _path, StdFromQString(value.toString()));
    try {
        // check preference datatype and try casting to it..
        const std::string datatype = preference_datatype(preference_store_, _path);
        int ttype                  = get_qmetatype(datatype);

        switch (ttype) {
        case QMetaType::Bool:
            gsh_->set_value(value.toBool(), _path, async);
            result = true;
            break;

        case QMetaType::Int:
            gsh_->set_value(value.toInt(), _path, async);
            result = true;
            break;

        case QMetaType::Double:
            gsh_->set_value(value.toDouble(), _path, async);
            result = true;
            break;

        case QMetaType::QString:
            gsh_->set_value(StdFromQString(value.toString()), _path, async);
            result = true;
            break;

        case QMetaType::QJsonDocument: {
            QVariant v = value;
            if (value.userType() == qMetaTypeId<QJSValue>()) {
                v = qvariant_cast<QJSValue>(value).toVariant();
            }

            switch (static_cast<int>(v.type())) {
            case QMetaType::QVariantMap:
                gsh_->set_value(
                    nlohmann::json::parse(QJsonDocument(v.toJsonObject())
                                              .toJson(QJsonDocument::Compact)
                                              .constData()),
                    _path,
                    async);
                result = true;
                break;
            case QMetaType::QVariantList:
                gsh_->set_value(
                    nlohmann::json::parse(QJsonDocument(v.toJsonArray())
                                              .toJson(QJsonDocument::Compact)
                                              .constData()),
                    _path,
                    async);
                result = true;
                break;
            // case QMetaType::QVariantList:
            // 	gsh_->set_value(QJsonDocument(value.toJsonArray()).toJson().constData(), _path,
            // async); 	result = true; 	break;
            default:
                spdlog::warn("Unsupported datatype {}", v.typeName());
                break;
            }
        } break;

        case QMetaType::UnknownType:
        default:
            spdlog::warn("Unknown datatype {}", datatype);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

void GlobalStoreUI::set_json_string(const QString &str) {

    if (str == json_string_)
        return;

    json_string_ = str;

    emit json_string_changed();
}