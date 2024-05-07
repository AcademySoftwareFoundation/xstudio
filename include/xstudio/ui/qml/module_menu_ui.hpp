// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifndef MODULE_QML_EXPORT_H
#define MODULE_QML_EXPORT_H

#ifdef MODULE_QML_STATIC_DEFINE
#define MODULE_QML_EXPORT
#define MODULE_QML_NO_EXPORT
#else
#ifndef MODULE_QML_EXPORT
#ifdef module_qml_EXPORTS
/* We are building this library */
#define MODULE_QML_EXPORT __declspec(dllexport)
#else
/* We are using this library */
#define MODULE_QML_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef MODULE_QML_NO_EXPORT
#define MODULE_QML_NO_EXPORT
#endif
#endif

#ifndef MODULE_QML_DEPRECATED
#define MODULE_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef MODULE_QML_DEPRECATED_EXPORT
#define MODULE_QML_DEPRECATED_EXPORT MODULE_QML_EXPORT MODULE_QML_DEPRECATED
#endif

#ifndef MODULE_QML_DEPRECATED_NO_EXPORT
#define MODULE_QML_DEPRECATED_NO_EXPORT MODULE_QML_NO_EXPORT MODULE_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef MODULE_QML_NO_DEPRECATED
#define MODULE_QML_NO_DEPRECATED
#endif
#endif

#endif /* MODULE_QML_EXPORT_H */

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QAbstractListModel>
#include <QUrl>
#include <QAbstractItemModel>
#include <QQmlPropertyMap>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/module/module.hpp"


namespace xstudio {
namespace ui {
    namespace qml {

        class ModuleAttrsToQMLShim;

        class MODULE_QML_EXPORT ModuleMenusModel : public QAbstractListModel {

            Q_OBJECT

          public:
            enum XsMenuRoles {
                MenuText = Qt::UserRole + 1024,
                IsCheckable,
                IsChecked,
                IsMultiChoice,
                Value,
                Enabled,
                IsDivider,
                Uuid,
                AttrType,
                HotkeyUuid
            };

            inline static const std::map<int, std::string> role_names = {
                {MenuText, "xs_module_menu_item_text"},
                {IsCheckable, "xs_module_menu_item_checkable"},
                {IsChecked, "xs_module_menu_item_checked"},
                {IsMultiChoice, "xs_module_menu_item_is_multichoice"},
                {Value, "xs_module_menu_item_value"},
                {Enabled, "xs_module_menu_item_enabled"},
                {IsDivider, "xs_module_menu_item_is_divider"},
                {Uuid, "xs_module_menu_item_uuid"},
                {AttrType, "xs_module_menu_item_attr_type"},
                {HotkeyUuid, "xs_module_menu_hotkey_uuid"}};

            Q_PROPERTY(int num_submenus READ num_submenus NOTIFY num_submenusChanged)
            Q_PROPERTY(QString root_menu_name READ rootMenuName WRITE setRootMenuName NOTIFY
                           rootMenuNameChanged)
            Q_PROPERTY(QString title READ title NOTIFY titleChanged)
            Q_PROPERTY(QStringList submenu_names READ submenu_names NOTIFY submenu_namesChanged)
            Q_PROPERTY(bool empty READ empty NOTIFY emptyChanged)

            ModuleMenusModel(QObject *parent = nullptr);

            ~ModuleMenusModel() override;

            [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

            [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

            bool setData(const QModelIndex &index, const QVariant &value, int role) override;

            void add_attributes_from_backend(const module::AttributeSet &attrs);

            [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &) const override {
                return Qt::ItemIsEnabled | Qt::ItemIsEditable;
            }

            void update_attribute_from_backend(
                const utility::Uuid &attr_uuid,
                const int role,
                const utility::JsonStore &role_value);

            void update_full_attribute_from_backend(const module::ConstAttributePtr &attr);

            void remove_attribute(const utility::Uuid &attr_uuid);

            [[nodiscard]] int num_submenus() const { return submenu_names_.size(); }

            [[nodiscard]] bool empty() const { return attributes_data_.empty(); }

          signals:

            void setAttributeFromFrontEnd(const QUuid, const int, const QVariant);
            void rootMenuNameChanged(QString);
            void num_submenusChanged();
            void titleChanged();
            void submenu_namesChanged();
            void emptyChanged();

          public slots:

            [[nodiscard]] QString rootMenuName() const { return menu_path_; }
            [[nodiscard]] QString title() const { return title_; }
            void setRootMenuName(QString p);
            // QObject * submenu(int index);
            [[nodiscard]] QStringList submenu_names() const { return submenu_names_; }

          private:
            bool already_have_attr_in_this_menu(const QUuid &uuid);
            bool is_attr_in_this_menu(const module::ConstAttributePtr &attr);
            void add_multi_choice_menu_item(const module::ConstAttributePtr &attr);
            void add_checkable_menu_item(const module::ConstAttributePtr &attr);
            void add_menu_action_item(const module::ConstAttributePtr &attr);
            void update_multi_choice_menu_item(
                const utility::Uuid &attr_uuid, const utility::JsonStore &string_choice_data);

            std::vector<QMap<int, QVariant>> attributes_data_;

            QStringList submenu_names_;
            QMap<QString, QList<QUuid>> attrs_per_submenus_;

            // QList<ModuleMenusModel *> submenus_;
            QString menu_path_;
            QString title_;
            int menu_nesting_depth_     = {0};
            ModuleAttrsToQMLShim *shim_ = {nullptr};
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio