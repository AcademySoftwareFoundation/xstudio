// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/module_menu_ui.hpp"
#include "xstudio/ui/qml/module_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

#include <QDebug>
#include <QTimer>
#include <QQmlEngine>

using namespace caf;
using namespace xstudio;
using namespace xstudio::module;
using namespace xstudio::ui::qml;

namespace {

QUuid uuid_from_attr_data(QMap<int, QVariant> &attr_data) {

    if (attr_data.contains(ModuleMenusModel::XsMenuRoles::Uuid)) {
        return attr_data[ModuleMenusModel::XsMenuRoles::Uuid].toUuid();
    }
    return QUuid();
}

} // namespace

ModuleMenusModel::ModuleMenusModel(QObject *parent) : QAbstractListModel(parent) {
    // QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    shim_ = new ModuleAttrsToQMLShim(this);
}

ModuleMenusModel::~ModuleMenusModel() {
    shim_->disconnect();
    shim_->clear();
}

QHash<int, QByteArray> ModuleMenusModel::roleNames() const {
    QHash<int, QByteArray> roles;
    for (const auto &p : ModuleMenusModel::role_names) {
        roles[p.first] = p.second.c_str();
    }
    return roles;
}

int ModuleMenusModel::rowCount(const QModelIndex &) const {
    return static_cast<int>(attributes_data_.size());
}

QVariant ModuleMenusModel::data(const QModelIndex &index, int role) const {

    QVariant rt;
    try {
        if ((int)attributes_data_.size() > index.row() &&
            attributes_data_[index.row()].contains(role)) {
            rt = attributes_data_[index.row()][role];
        } else {
        }
    } catch (std::exception &e) {
    }
    return rt;
}

bool ModuleMenusModel::already_have_attr_in_this_menu(const QUuid &uuid) {

    bool rt = false;
    for (const auto &p : attributes_data_) {
        if (p.contains(XsMenuRoles::Uuid)) {
            if (p[XsMenuRoles::Uuid].toUuid() == uuid) {
                rt = true;
            }
        }
    }
    return rt;
}

QString pathAtDepth(const QString &path, const int depth) {
    int d   = depth;
    int pos = 0;
    while (path.indexOf("|", pos) != -1) {
        pos = path.indexOf("|", pos + 1);
        if (!d) {
            return path.mid(0, path.indexOf("|", pos));
        }
        d--;
    }
    return path;
}

bool ModuleMenusModel::is_attr_in_this_menu(const ConstAttributePtr &attr) {

    if (menu_path_.isEmpty())
        return false;
    auto bf = submenu_names_;
    try {
        bool changed = false;

        if (attr->has_role_data(Attribute::MenuPaths)) {
            const QUuid attr_uuid(QUuidFromUuid(attr->uuid()));

            auto menu_paths =
                attr->get_role_data<std::vector<std::string>>(Attribute::MenuPaths);

            for (const auto &menu_path : menu_paths) {
                const int nesting_depth = menu_path_.count("|");

                QString path             = QStringFromStd(menu_path);
                QString path_to_my_depth = pathAtDepth(path, nesting_depth);

                if (menu_path_ == path) {
                    return true;
                } else if (path.indexOf(menu_path_) == 0) {

                    const QString child_menu_path = pathAtDepth(path, nesting_depth + 1);

                    if (not submenu_names_.count(child_menu_path)) {
                        submenu_names_.append(child_menu_path);
                        attrs_per_submenus_[child_menu_path].append(attr_uuid);
                        changed = true;
                    } else {
                        if (not attrs_per_submenus_[child_menu_path].count(attr_uuid)) {
                            attrs_per_submenus_[child_menu_path].append(attr_uuid);
                        }
                    }
                }
            }
        }

        if (changed) {
            emit submenu_namesChanged();
            emit num_submenusChanged();
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return false;
}

void ModuleMenusModel::add_attributes_from_backend(const module::AttributeSet &attrs) {

    const bool e = empty();
    try {
        if (not attrs.empty()) {

            module::AttributeSet new_menu_attrs;
            for (const auto &attr : attrs) {

                if (not is_attr_in_this_menu(attr))
                    continue;

                if (not already_have_attr_in_this_menu(QUuidFromUuid(attr->uuid()))) {
                    new_menu_attrs.push_back(attr);
                } else {
                    update_full_attribute_from_backend(attr);
                }
            }


            for (const auto &attr : new_menu_attrs) {

                if (attr->has_role_data(Attribute::Type)) {
                    const auto attr_type = attr->get_role_data<std::string>(Attribute::Type);
                    if (attr_type == Attribute::type_name(Attribute::StringChoiceAttribute)) {
                        add_multi_choice_menu_item(attr);
                    } else if (attr_type == Attribute::type_name(Attribute::BooleanAttribute)) {
                        add_checkable_menu_item(attr);
                    } else if (attr_type == Attribute::type_name(Attribute::ActionAttribute)) {
                        add_menu_action_item(attr);
                    }
                }
            }
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    if (empty() != e)
        emit emptyChanged();
}

void ModuleMenusModel::add_multi_choice_menu_item(const ConstAttributePtr &attr) {

    const bool e = empty();

    try {

        auto string_choices =
            attr->get_role_data<std::vector<std::string>>(Attribute::StringChoices);
        const QUuid attr_uuid = QUuidFromUuid(attr->uuid());

        std::vector<bool> enabled(string_choices.size(), true);
        if (attr->has_role_data(Attribute::StringChoicesEnabled)) {
            try {
                auto string_choices_enabled_data =
                    attr->get_role_data<std::vector<bool>>(Attribute::StringChoicesEnabled);
                if (string_choices_enabled_data.size() == enabled.size()) {
                    enabled = string_choices_enabled_data;
                }
            } catch (...) {
                // if the enabled vector is empty it won't cast to vect<bool>
            }
        }

        QString value = QStringFromStd(attr->get_role_data<std::string>(Attribute::Value));
        QStringList choices;
        for (auto choice : string_choices) {
            choices.append(QStringFromStd(choice));
        }

        bool dummy_entry = false;
        if (!choices.size()) {
            choices.append("-");
            enabled.push_back(false);
            dummy_entry = true;
        }

        beginInsertRows(
            QModelIndex(),
            attributes_data_.size(),
            static_cast<int>(attributes_data_.size() + choices.size()) - 1);

        int idx = 0;
        for (auto choice : choices) {
            QMap<int, QVariant> roles_data;
            roles_data[XsMenuRoles::MenuText]      = QVariant(choice);
            roles_data[XsMenuRoles::IsCheckable]   = QVariant(dummy_entry ? false : true);
            roles_data[XsMenuRoles::IsMultiChoice] = QVariant(dummy_entry ? false : true);
            roles_data[XsMenuRoles::Value]         = QVariant(value == choice);
            roles_data[XsMenuRoles::Uuid]          = attr_uuid;
            roles_data[XsMenuRoles::AttrType]      = "MultiChoice";
            roles_data[XsMenuRoles::Enabled]       = QVariant(enabled[idx]);
            attributes_data_.push_back(roles_data);
            idx++;
        }

        endInsertRows();

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    if (empty() != e)
        emit emptyChanged();
}

void ModuleMenusModel::update_multi_choice_menu_item(
    const utility::Uuid &attr_uuid, const utility::JsonStore &string_choice_data) {

    const QUuid quuid = QUuidFromUuid(attr_uuid);
    const bool e      = empty();

    if (not already_have_attr_in_this_menu(quuid))
        return;

    if (string_choice_data.is_array() && string_choice_data.size() &&
        string_choice_data.begin().value().is_string()) {

        const size_t num_new_string_choices = string_choice_data.size();

        int idx = 0;
        auto p  = attributes_data_.begin();

        // first, clear out existing choices
        while (p != attributes_data_.end()) {
            if ((*p)[XsMenuRoles::Uuid].toUuid() == quuid) {
                beginRemoveRows(QModelIndex(), idx, idx);
                p = attributes_data_.erase(p);
                endRemoveRows();
            } else {
                p++;
                idx++;
            }
        }

        QStringList choices;
        for (size_t i = 0; i < num_new_string_choices; ++i) {
            choices.append(QStringFromStd(string_choice_data[i].get<std::string>()));
        }

        beginInsertRows(
            QModelIndex(),
            attributes_data_.size(),
            static_cast<int>(attributes_data_.size() + choices.size()) - 1);

        int i = 0;
        for (auto choice : choices) {
            QMap<int, QVariant> roles_data;
            roles_data[XsMenuRoles::MenuText]      = QVariant(choice);
            roles_data[XsMenuRoles::IsCheckable]   = QVariant(true);
            roles_data[XsMenuRoles::IsMultiChoice] = QVariant(true);
            // Reset the current value to the first item, can we do something more clever?
            roles_data[XsMenuRoles::Value]    = i++ == 0 ? QVariant(true) : QVariant(false);
            roles_data[XsMenuRoles::Uuid]     = quuid;
            roles_data[XsMenuRoles::AttrType] = "MultiChoice";
            roles_data[XsMenuRoles::Enabled]  = QVariant(true);
            attributes_data_.push_back(roles_data);
        }

        endInsertRows();
    }

    if (empty() != e)
        emit emptyChanged();
}

void ModuleMenusModel::add_checkable_menu_item(const ConstAttributePtr &attr) {

    const bool e = empty();
    try {

        QString title = QStringFromStd(attr->get_role_data<std::string>(Attribute::Title));
        const QUuid attr_uuid = QUuidFromUuid(attr->uuid());
        const bool value      = attr->get_role_data<bool>(Attribute::Value);

        beginInsertRows(QModelIndex(), attributes_data_.size(), attributes_data_.size());

        QMap<int, QVariant> roles_data;
        roles_data[XsMenuRoles::MenuText]    = QVariant(title);
        roles_data[XsMenuRoles::IsCheckable] = QVariant(true);
        roles_data[XsMenuRoles::Value]       = QVariant(value);
        roles_data[XsMenuRoles::Uuid]        = attr_uuid;
        roles_data[XsMenuRoles::AttrType]    = "Toggle";
        attributes_data_.push_back(roles_data);

        endInsertRows();

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    if (empty() != e)
        emit emptyChanged();
}

void ModuleMenusModel::add_menu_action_item(const ConstAttributePtr &attr) {

    const bool e = empty();
    try {

        QString title = QStringFromStd(attr->get_role_data<std::string>(Attribute::Title));
        const QUuid attr_uuid = QUuidFromUuid(attr->uuid());

        beginInsertRows(QModelIndex(), attributes_data_.size(), attributes_data_.size());

        QMap<int, QVariant> roles_data;
        roles_data[XsMenuRoles::MenuText]    = QVariant(title);
        roles_data[XsMenuRoles::IsCheckable] = QVariant(false);
        roles_data[XsMenuRoles::Uuid]        = attr_uuid;
        roles_data[XsMenuRoles::Value]       = QVariant(false);
        roles_data[XsMenuRoles::AttrType]    = "Action";
        if (attr->has_role_data(Attribute::HotkeyUuid)) {
            roles_data[XsMenuRoles::HotkeyUuid] =
                QUuid(attr->get_role_data<std::string>(Attribute::HotkeyUuid).c_str());
        }
        attributes_data_.push_back(roles_data);

        endInsertRows();

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    if (empty() != e)
        emit emptyChanged();
}

void ModuleMenusModel::remove_attribute(const utility::Uuid &attr_uuid) {

    auto bf           = submenu_names_;
    const bool e      = empty();
    const QUuid quuid = QUuidFromUuid(attr_uuid);
    int idx           = 0;
    auto attr         = attributes_data_.begin();
    while (attr != attributes_data_.end()) {
        if ((*attr).contains(XsMenuRoles::Uuid) &&
            (*attr)[XsMenuRoles::Uuid].toUuid() == quuid) {
            beginRemoveRows(QModelIndex(), idx, idx);
            attr = attributes_data_.erase(attr);
            endRemoveRows();
        } else {
            attr++;
            idx++;
        }
    }

    auto p       = attrs_per_submenus_.begin();
    bool changed = false;
    while (p != attrs_per_submenus_.end()) {

        if (p.value().count(quuid)) {
            p.value().removeAll(quuid);
            if (p.value().empty()) {
                // a submenu has been completely cleared out - now we remove it by changing
                // submenu_names
                submenu_names_.removeAll(p.key());
                attrs_per_submenus_.remove(p.key());
                p       = attrs_per_submenus_.begin();
                changed = true;
            } else {
                p++;
            }

        } else {
            p++;
        }
    }
    if (changed) {
        emit submenu_namesChanged();
    }
    if (empty() != e)
        emit emptyChanged();
}

bool ModuleMenusModel::setData(const QModelIndex &index, const QVariant &value, int role) {

    if (role == (XsMenuRoles::Value)) {
        if (attributes_data_[index.row()][XsMenuRoles::AttrType].toString() == "MultiChoice" &&
            value.toBool()) {
            emit setAttributeFromFrontEnd(
                attributes_data_[index.row()][XsMenuRoles::Uuid].toUuid(),
                Attribute::Value,
                attributes_data_[index.row()][XsMenuRoles::MenuText]);
        } else if (
            attributes_data_[index.row()][XsMenuRoles::AttrType].toString() == "Toggle") {
            emit setAttributeFromFrontEnd(
                attributes_data_[index.row()][XsMenuRoles::Uuid].toUuid(),
                Attribute::Value,
                value);
        } else if (
            attributes_data_[index.row()][XsMenuRoles::AttrType].toString() == "Action") {
            emit setAttributeFromFrontEnd(
                attributes_data_[index.row()][XsMenuRoles::Uuid].toUuid(),
                Attribute::Value,
                QVariant());
        }
    }
    return true;
}

void ModuleMenusModel::update_full_attribute_from_backend(
    const module::ConstAttributePtr &attr) {

    const bool e = empty();

    try {
        const QUuid attr_uuid(QUuidFromUuid(attr->uuid()));
        int row = 0;
        for (auto &attribute : attributes_data_) {

            QUuid uuid = uuid_from_attr_data(attribute);
            if (uuid == attr_uuid) {
                for (const int role : attribute.keys()) {

                    if (attr->has_role_data(role)) {
                        QVariant new_role_data =
                            json_to_qvariant(attr->role_data_as_json(role));
                        if (new_role_data != attribute[role]) {
                            attribute[role]         = new_role_data;
                            const QModelIndex index = createIndex(row, 0);
                            emit dataChanged(index, index, {role});
                        }
                    }
                }
            }
            row++;
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    if (empty() != e)
        emit emptyChanged();
}

void ModuleMenusModel::update_attribute_from_backend(
    const utility::Uuid &attr_uuid, const int role, const utility::JsonStore &role_value) {

    try {
        if (role == Attribute::Value) {
            const QUuid quuid = QUuidFromUuid(attr_uuid);
            int row           = 0;
            for (auto &p : attributes_data_) {

                if (p.contains(XsMenuRoles::Uuid) && p[XsMenuRoles::Uuid].toUuid() == quuid) {
                    if (p[XsMenuRoles::AttrType].toString() == "MultiChoice") {
                        if (p[XsMenuRoles::MenuText] == json_to_qvariant(role_value) &&
                            !p[XsMenuRoles::Value].toBool()) {
                            p[XsMenuRoles::Value]   = QVariant(true);
                            const QModelIndex index = createIndex(row, 0);
                            emit dataChanged(index, index, {XsMenuRoles::Value});
                        } else if (
                            p[XsMenuRoles::MenuText] != json_to_qvariant(role_value) &&
                            p[XsMenuRoles::Value].toBool()) {
                            p[XsMenuRoles::Value]   = QVariant(false);
                            const QModelIndex index = createIndex(row, 0);
                            emit dataChanged(index, index, {XsMenuRoles::Value});
                        }
                    } else if (p[XsMenuRoles::AttrType].toString() == "Toggle") {
                        p[XsMenuRoles::Value]   = json_to_qvariant(role_value);
                        const QModelIndex index = createIndex(row, 0);
                        emit dataChanged(index, index, {XsMenuRoles::Value});
                    }
                }
                row++;
            }
        } else if (role == Attribute::StringChoices) {

            update_multi_choice_menu_item(attr_uuid, role_value);

        } else if (role == Attribute::StringChoicesEnabled) {

            QList<bool> enabled;
            if (role_value.is_array() && role_value.size() &&
                role_value.begin().value().is_boolean()) {

                for (size_t i = 0; i < role_value.size(); ++i) {
                    enabled.append(role_value[i].get<bool>());
                }
            }

            const QUuid quuid = QUuidFromUuid(attr_uuid);
            int row           = 0;
            int idx           = 0;
            for (auto &p : attributes_data_) {

                if (p.contains(XsMenuRoles::Uuid) && p[XsMenuRoles::Uuid].toUuid() == quuid) {
                    if (p[XsMenuRoles::AttrType].toString() == "MultiChoice") {

                        if (idx < enabled.size()) {
                            p[XsMenuRoles::Enabled] = QVariant(enabled[idx]);
                            const QModelIndex index = createIndex(row, 0);
                            emit dataChanged(index, index, {XsMenuRoles::Enabled});
                        }
                        idx++;
                    }
                }
                row++;
            }
        }

    } catch (std::exception &e) {
    }
}

void ModuleMenusModel::setRootMenuName(QString root_menu_name) {

    if (menu_path_ != root_menu_name) {
        menu_path_ = root_menu_name;
        emit rootMenuNameChanged(menu_path_);

        if (root_menu_name.contains("|")) {
            title_ = root_menu_name.mid(root_menu_name.lastIndexOf("|") + 1);
            emit titleChanged();
        }
    }
}