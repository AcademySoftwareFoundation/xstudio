// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/module_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

#include <QDebug>
#include <QTimer>

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::module;
using namespace xstudio::ui::qml;

namespace {

bool attr_is_in_group(
    const module::ConstAttributePtr &attr, const QStringList &group_names_in) {

    bool match = false;
    try {
        auto group_names =
            attr->get_role_data<std::vector<std::string>>(Attribute::Role::Groups);
        for (auto g : group_names) {
            if (group_names_in.contains(QStringFromStd(g))) {
                match = true;
                break;
            }
        }
    } catch (std::exception &e) {
    }

    return match;
}

QUuid uuid_from_attr_data(QMap<int, QVariant> &attr_data) {

    if (attr_data.contains(Attribute::UuidRole)) {
        return attr_data[Attribute::UuidRole].toUuid();
    }
    return QUuid();
}

} // namespace


ModuleAttrsDirect::ModuleAttrsDirect(QObject *parent)
    : QQmlPropertyMap(this, parent), role_name_("value") {
    new ModuleAttrsToQMLShim(this);
    emit roleNameChanged();
}

ModuleAttrsDirect::~ModuleAttrsDirect() {}

void ModuleAttrsDirect::add_attributes_from_backend(
    const module::AttributeSet &attrs, bool check_group) {
    try {
        auto role_id = Attribute::role_index(role_name_.toStdString());
        for (const auto &attr : attrs) {
            try {
                if (check_group &&
                    (!attr_is_in_group(attr, attrs_group_name_) ||
                     (role_id != Attribute::Role::Value and !attr->has_role_data(role_id))))
                    continue;

                const auto uuid = QUuidFromUuid(attr->uuid());
                QString qstr_name(
                    QStringFromStd(attr->get_role_data<std::string>(Attribute::Role::Title))
                        .toLower());
                qstr_name = qstr_name.replace(" ", "_");

                // force the attr name to be lower case alphanumeric and underscore only
                qstr_name = qstr_name.replace(QRegExp("[^_a-z0-9]"), "");
                QVariant valueqv;
                try {
                    valueqv = json_to_qvariant(attr->role_data_as_json(role_id));
                } catch (...) {
                }

                attr_uuids_by_name_[qstr_name] = uuid;
                attr_names_by_uuid_[uuid]      = qstr_name;
                attr_values_by_uuid_[uuid]     = valueqv;

                insert(qstr_name, valueqv);
                emit attrAdded(qstr_name);

            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void ModuleAttrsDirect::setRoleName(QString role_name) {

    if (role_name != role_name_) {
        role_name_ = role_name;

        // re-use this signal to get the backend to re-broadcast attributes
        // back to this class so we can update our properties to reflect the
        // data role
        emit attributesGroupNamesChanged(attrs_group_name_);
    }
}


void ModuleAttrsDirect::update_attribute_from_backend(
    const utility::Uuid &attr_uuid, const int role, const utility::JsonStore &role_value) {
    int role_id = Attribute::Role::Value;
    try {
        role_id = Attribute::role_index(role_name_.toStdString());
        // spdlog::warn("{} {} {} {} {}", __PRETTY_FUNCTION__, role_value.dump(2), role_id,
        // role, to_string(attr_uuid));

        if (role == role_id) {
            const QUuid uuidv = QUuidFromUuid(attr_uuid);
            if (attr_names_by_uuid_.contains(uuidv)) {
                try {
                    const QVariant valueqv = json_to_qvariant(role_value);
                    insert(attr_names_by_uuid_[uuidv], valueqv);
                    attr_values_by_uuid_[uuidv] = valueqv;
                    emit valueChanged(attr_names_by_uuid_[uuidv], valueqv);
                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                };
            } else {
                /*spdlog::warn("{} Backend requested update of unregistered attribute. {}",
                    __PRETTY_FUNCTION__, to_string(attr_uuid));*/
            }
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void ModuleAttrsDirect::remove_attribute(const utility::Uuid &) {
    // QQmlPropertyMap does not provide a way to remove properties after they
    // have been created
}

QVariant ModuleAttrsDirect::updateValue(const QString &key, const QVariant &input) {

    if (!attr_uuids_by_name_.contains(key)) {
        spdlog::warn(
            "{} Attempt to update value of unregistered attribute \"{}\"",
            __PRETTY_FUNCTION__,
            StdFromQString(key));
        return QVariant();
    }

    const QUuid &uuid = attr_uuids_by_name_[key];

    // the attribute is being set from QML ... we send a signal to pass this to the backend,
    // and wait for the change to come back via update_attribute_from_backend
    emit setAttributeFromFrontEnd(uuid, Attribute::Role::Value, input);

    // now we OVERRIDE the change from QML, because the change can't happen until it has
    // bubbled back up from the backend. The intention is to resolve sync issues between front
    // and backend that could otherwise set-up infinite loop where the backend change creates
    // a change signal in QML which is passed back to the backend - if a floating point round
    // error, say, means data in the backend is slightly different to the front end we would
    // get stuck in an infinite loop!
    return attr_values_by_uuid_[uuid];
}

void ModuleAttrsDirect::setattributesGroupNames(QStringList group_name) {

    if (attrs_group_name_ != group_name) {
        attrs_group_name_ = group_name;
        emit attributesGroupNamesChanged(attrs_group_name_);
    }
}

OrderedModuleAttrsModel::OrderedModuleAttrsModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setSourceModel(new ModuleAttrsModel(this));
    setDynamicSortFilter(true);
    setSortRole(module::Attribute::ToolbarPosition + Qt::UserRole + 1);
    sort(0, Qt::AscendingOrder);
}

QStringList OrderedModuleAttrsModel::attributesGroupNames() const {
    auto mam = dynamic_cast<ModuleAttrsModel *>(sourceModel());
    if (mam)
        return mam->attributesGroupNames();
    return QStringList();
}

void OrderedModuleAttrsModel::setattributesGroupNames(const QStringList &group_name) {
    auto mam = dynamic_cast<ModuleAttrsModel *>(sourceModel());
    if (mam) {
        mam->setattributesGroupNames(group_name);
        emit attributesGroupNamesChanged(group_name);
    }
}

ModuleAttrsModel::ModuleAttrsModel(QObject *parent) : QAbstractListModel(parent) {
    new ModuleAttrsToQMLShim(this);
}

ModuleAttrsModel::~ModuleAttrsModel() {}

QHash<int, QByteArray> ModuleAttrsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    for (const auto &p : Attribute::role_names) {
        roles[p.first + Qt::UserRole + 1] = p.second.c_str();
    }
    return roles;
}

int ModuleAttrsModel::rowCount(const QModelIndex &) const {
    return static_cast<int>(attributes_data_.size());
}

QVariant ModuleAttrsModel::data(const QModelIndex &index, int role) const {

    QVariant rt;

    role -= Qt::UserRole + 1;

    try {
        if ((int)attributes_data_.size() > index.row() &&
            attributes_data_[index.row()].contains(role)) {
            rt = attributes_data_[index.row()][role];
        }
    } catch (const std::exception &) {
    }

    return rt;
}

bool ModuleAttrsModel::have_attr(const QUuid &uuid) {

    bool rt = false;
    for (const auto &p : attributes_data_) {
        if (p.contains(Attribute::UuidRole)) {
            if (p[Attribute::UuidRole].toUuid() == uuid) {
                rt = true;
            }
        }
    }
    return rt;
}

void ModuleAttrsModel::add_attributes_from_backend(
    const module::AttributeSet &attrs, const bool check_group) {
    if (not attrs.empty()) {

        module::AttributeSet new_attrs;
        for (const auto &attr : attrs) {

            if (not have_attr(QUuidFromUuid(attr->uuid()))) {
                if (check_group && !attr_is_in_group(attr, attrs_group_name_))
                    continue;
                new_attrs.push_back(attr);
            } else {
                update_full_attribute_from_backend(attr);
            }
        }

        beginInsertRows(
            QModelIndex(),
            attributes_data_.size(),
            static_cast<int>(attributes_data_.size() + new_attrs.size()) - 1);

        for (const auto &attr : new_attrs) {

            QMap<int, QVariant> attr_qt_data;
            const nlohmann::json json = attr->as_json();

            for (auto p = json.begin(); p != json.end(); ++p) {
                const int role = Attribute::role_index(p.key());
                if (role == Attribute::UuidRole) {
                    attr_qt_data[role] = QUuidFromUuid(p.value().get<utility::Uuid>());
                } else {
                    attr_qt_data[role] = json_to_qvariant(p.value());
                }
            }
            attributes_data_.push_back(attr_qt_data);
        }

        endInsertRows();
        emit rowCountChanged();
    }
}

void ModuleAttrsModel::remove_attribute(const utility::Uuid &attr_uuid) {
    const QUuid quuid = QUuidFromUuid(attr_uuid);
    int idx           = 0;
    for (auto attr = attributes_data_.begin(); attr != attributes_data_.end(); attr++) {
        if ((*attr).contains(Attribute::UuidRole) &&
            (*attr)[Attribute::UuidRole].toUuid() == quuid) {
            beginRemoveRows(QModelIndex(), idx, idx);
            attributes_data_.erase(attr);
            endRemoveRows();
            emit rowCountChanged();
            break;
        }
        idx++;
    }
}

bool ModuleAttrsModel::setData(const QModelIndex &index, const QVariant &value, int role) {

    role -= Qt::UserRole + 1;
    emit setAttributeFromFrontEnd(
        attributes_data_[index.row()][Attribute::UuidRole].toUuid(), role, value);

    return true;
}

void ModuleAttrsModel::update_full_attribute_from_backend(
    const module::ConstAttributePtr &full_attr) {

    try {
        const QUuid attr_uuid(QUuidFromUuid(full_attr->uuid()));
        int row = 0;
        for (auto &attribute : attributes_data_) {

            QUuid uuid = uuid_from_attr_data(attribute);
            if (uuid == attr_uuid) {
                for (const int role : attribute.keys()) {

                    if (full_attr->has_role_data(role)) {
                        QVariant new_role_data =
                            json_to_qvariant(full_attr->role_data_as_json(role));
                        if (new_role_data != attribute[role]) {
                            attribute[role]         = new_role_data;
                            const QModelIndex index = createIndex(row, 0);
                            emit dataChanged(index, index, {role + Qt::UserRole + 1});
                        }
                    }
                }
            }
            row++;
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void ModuleAttrsModel::update_attribute_from_backend(
    const utility::Uuid &attr_uuid, const int role, const utility::JsonStore &role_value) {

    try {

        const QUuid quuid = QUuidFromUuid(attr_uuid);

        // special case ... if an attribute has been added to a 'group' (so is dynamically
        // added to the Model), or if an attribute is removed from a 'group' (so is dynamiucally
        // removed from the Model) then we need to trigger a full update
        if (role == Attribute::Groups) {

            bool attr_is_in_this_model = false;
            for (auto &p : attributes_data_) {

                if (p.contains(Attribute::UuidRole) &&
                    p[Attribute::UuidRole].toUuid() == quuid) {
                    attr_is_in_this_model = true;
                    break;
                }
            }

            bool attr_should_be_in_this_model = false;
            if (role_value.is_array()) {
                for (const auto &o : role_value) {
                    QString attr_group_name = QStringFromStd(o.get<std::string>());
                    if (attrs_group_name_.contains(attr_group_name)) {
                        attr_should_be_in_this_model = true;
                        break;
                    }
                }
            }

            if (attr_is_in_this_model && !attr_should_be_in_this_model) {
                remove_attribute(attr_uuid);
            } else if (!attr_is_in_this_model && attr_should_be_in_this_model) {
                emit doFullupdateFromBackend(attrs_group_name_);
            }
        }

        int row = 0;
        for (auto &p : attributes_data_) {

            if (p.contains(Attribute::UuidRole) && p[Attribute::UuidRole].toUuid() == quuid) {
                if (role_value.is_null())
                    p[role] = QVariant();
                else
                    p[role] = json_to_qvariant(role_value);
                const QModelIndex index = createIndex(row, 0);
                emit dataChanged(index, index, {role + Qt::UserRole + 1});
            }
            row++;
        }
    } catch (std::exception &e) {
    }
}

void ModuleAttrsModel::setattributesGroupNames(QStringList group_name) {

    if (attrs_group_name_ != group_name) {
        attrs_group_name_ = group_name;
        emit attributesGroupNamesChanged(attrs_group_name_);
    }
}

ModuleAttrsToQMLShim::~ModuleAttrsToQMLShim() {

    // wipe the message handler, because it seems to be possible that messages
    // come in before our actor companion has exited but AFTER this object
    // (ModuleAttrsToQMLShim) has been destroyed - if we don't wipe the message
    // handler it gets a bit 'crashy' as it tries to execute a function in
    // the handler declared in 'ModuleAttrsToQMLShim::init' when the object is deleted.
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return caf::message_handler();
    });

    if (attrs_events_actor_group_) {
        caf::scoped_actor sys(CafSystemObject::get_actor_system());
        try {
            request_receive<bool>(
                *sys, attrs_events_actor_group_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
    disconnect();
}


ModuleAttrsToQMLShim::ModuleAttrsToQMLShim(ModuleAttrsModel *model)
    : QMLActor(static_cast<QObject *>(model)), model_(model), uuid_(utility::Uuid::generate()) {

    init(CafSystemObject::get_actor_system());

    QObject::connect(
        model_,
        &ModuleAttrsModel::setAttributeFromFrontEnd,
        this,
        &ModuleAttrsToQMLShim::setAttributeFromFrontEnd);

    QObject::connect(
        model_,
        &ModuleAttrsModel::attributesGroupNamesChanged,
        this,
        &ModuleAttrsToQMLShim::setattributesGroupNames);

    QObject::connect(
        model_,
        &ModuleAttrsModel::doFullupdateFromBackend,
        this,
        &ModuleAttrsToQMLShim::doFullupdateFromBackend);
}

ModuleAttrsToQMLShim::ModuleAttrsToQMLShim(ModuleAttrsDirect *qml_attrs_map)
    : QMLActor(static_cast<QObject *>(qml_attrs_map)),
      qml_attrs_map_(qml_attrs_map),
      uuid_(utility::Uuid::generate()) {

    init(CafSystemObject::get_actor_system());

    QObject::connect(
        qml_attrs_map,
        &ModuleAttrsDirect::setAttributeFromFrontEnd,
        this,
        &ModuleAttrsToQMLShim::setAttributeFromFrontEnd);

    QObject::connect(
        qml_attrs_map,
        &ModuleAttrsDirect::attributesGroupNamesChanged,
        this,
        &ModuleAttrsToQMLShim::setattributesGroupNames);
}

ModuleAttrsToQMLShim::ModuleAttrsToQMLShim(ModuleMenusModel *model)
    : QMLActor(static_cast<QObject *>(model)),
      menu_model_(model),
      uuid_(utility::Uuid::generate()) {

    init(CafSystemObject::get_actor_system());

    QObject::connect(
        model,
        &ModuleMenusModel::setAttributeFromFrontEnd,
        this,
        &ModuleAttrsToQMLShim::setAttributeFromFrontEnd);

    QObject::connect(
        model,
        &ModuleMenusModel::rootMenuNameChanged,
        this,
        &ModuleAttrsToQMLShim::setRootMenuName);
}

void ModuleAttrsToQMLShim::init(caf::actor_system &system) {

    QMLActor::init(system);

    // spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(as_actor()));


    // this doesn't help with Qt actors..
    self()->set_default_handler(caf::drop);

    attrs_events_actor_ = system.registry().get<caf::actor>(module_events_registry);

    try {
        caf::scoped_actor sys(system);
        attrs_events_actor_group_ = utility::request_receive<caf::actor>(
            *sys, attrs_events_actor_, utility::get_group_atom_v);

        anon_send(attrs_events_actor_group_, broadcast::join_broadcast_atom_v, as_actor());
        // request_receive<bool>(*sys, group, broadcast::join_broadcast_atom_v, as_actor());
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](utility::serialise_atom) -> utility::JsonStore { return utility::JsonStore(); },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &addr) {
                if (addr == caf::actor_cast<caf::actor_addr>(attrs_events_actor_group_)) {
                    attrs_events_actor_group_ = caf::actor();
                }
            },
            [=](const group_down_msg &g) {},
            [=](full_attributes_description_atom,
                const AttributeSet &attrs,
                const utility::Uuid &requester_uuid) {
                if (requester_uuid == uuid_) {
                    if (model_)
                        model_->add_attributes_from_backend(attrs);
                    if (qml_attrs_map_)
                        qml_attrs_map_->add_attributes_from_backend(attrs);
                    if (menu_model_)
                        menu_model_->add_attributes_from_backend(attrs);
                }
            },
            [=](full_attributes_description_atom, const AttributeSet &attrs) {
                if (model_)
                    model_->add_attributes_from_backend(attrs, true);
                if (qml_attrs_map_)
                    qml_attrs_map_->add_attributes_from_backend(attrs, true);
                if (menu_model_)
                    menu_model_->add_attributes_from_backend(attrs);
            },

            [=](change_attribute_event_atom,
                const utility::Uuid & /*module_uuid*/,
                const utility::Uuid &attr_uuid,
                const int role,
                const utility::JsonStore &role_value) {
                if (model_)
                    model_->update_attribute_from_backend(attr_uuid, role, role_value);
                if (qml_attrs_map_)
                    qml_attrs_map_->update_attribute_from_backend(attr_uuid, role, role_value);
                if (menu_model_)
                    menu_model_->update_attribute_from_backend(attr_uuid, role, role_value);
            },
            [=](remove_attrs_from_ui_atom, const std::vector<utility::Uuid> &attr_uuids) {
                if (model_) {
                    for (const auto &attr_uuid : attr_uuids) {
                        model_->remove_attribute(attr_uuid);
                    }
                }
                if (menu_model_) {
                    for (const auto &attr_uuid : attr_uuids) {
                        menu_model_->remove_attribute(attr_uuid);
                    }
                }
            },
            [=](attribute_deleted_event_atom, const utility::Uuid & /*attr_uuid*/) {
                // model_->remove_attribute(attr_uuid);
            }};
    });
}

void ModuleAttrsToQMLShim::setAttributeFromFrontEnd(
    const QUuid property_uuid, const int role, const QVariant role_value) {
    try {
        auto js = utility::JsonStore();
        if (not role_value.isNull())
            js = utility::JsonStore(qvariant_to_json(role_value));

        anon_send(
            attrs_events_actor_,
            change_attribute_request_atom_v,
            UuidFromQUuid(property_uuid),
            role,
            js);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void ModuleAttrsToQMLShim::setattributesGroupNames(QStringList group_name) {
    if (attrs_group_name_ != group_name) {

        // The attributes group name (e.g. 'main_toolbar' or 'playhead') has changed
        // (due to something happening in QML code). We therefore request a full list
        // of attributes belonging to that group from the backend.
        attrs_group_name_ = group_name;
        std::vector<std::string> strs;
        for (const auto &n : group_name) {
            strs.emplace_back(StdFromQString(n));
        }
        anon_send(attrs_events_actor_, request_full_attributes_description_atom_v, strs, uuid_);
    }
}

void ModuleAttrsToQMLShim::doFullupdateFromBackend(QStringList group_name) {

    attrs_group_name_ = group_name;
    std::vector<std::string> strs;
    for (const auto &n : group_name) {
        strs.emplace_back(StdFromQString(n));
    }
    anon_send(attrs_events_actor_, request_full_attributes_description_atom_v, strs, uuid_);
}

void ModuleAttrsToQMLShim::setRootMenuName(QString root_menu_name) {

    if (attrs_group_name_.empty() ||
        (attrs_group_name_.size() && attrs_group_name_[0] != root_menu_name)) {

        // The attributes group name (e.g. 'main_toolbar' or 'playhead') has changed
        // (due to something happening in QML code). We therefore request a full list
        // of attributes belonging to that group from the backend.
        if (attrs_group_name_.empty()) {
            attrs_group_name_.append(root_menu_name);
        } else {
            attrs_group_name_[0] = root_menu_name;
        }

        anon_send(
            attrs_events_actor_,
            request_menu_attributes_description_atom_v,
            StdFromQString(root_menu_name),
            uuid_);
    }
}
