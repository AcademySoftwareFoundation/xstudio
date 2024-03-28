// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QAbstractListModel>
#include <QUrl>
#include <QAbstractItemModel>
#include <QQmlPropertyMap>
#include <QSortFilterProxyModel>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/module_menu_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/module/module.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class ModuleAttrsDirect : public QQmlPropertyMap {

            Q_OBJECT

          public:
            Q_PROPERTY(QStringList attributesGroupNames READ attributesGroupNames WRITE
                           setattributesGroupNames NOTIFY attributesGroupNamesChanged)
            Q_PROPERTY(QString roleName READ roleName WRITE setRoleName NOTIFY roleNameChanged)

            ModuleAttrsDirect(QObject *parent = nullptr);
            virtual ~ModuleAttrsDirect();

            void add_attributes_from_backend(
                const module::AttributeSet &attrs, const bool check_group = false);

            void update_attribute_from_backend(
                const utility::Uuid &attr_uuid,
                const int role,
                const utility::JsonStore &role_value);

            void remove_attribute(const utility::Uuid &attr_uuid);

          public slots:

            [[nodiscard]] QStringList attributesGroupNames() const { return attrs_group_name_; }
            [[nodiscard]] QString roleName() const { return role_name_; }

            void setattributesGroupNames(QStringList group_name);
            void setRoleName(QString group_name);

          signals:

            void setAttributeFromFrontEnd(const QUuid, const int, const QVariant);
            void attributesGroupNamesChanged(QStringList group_name);
            void attrAdded(QString attr_name);
            void roleNameChanged();

          protected:
            QVariant updateValue(const QString &key, const QVariant &input) override;

          private:
            QMap<QString, QUuid> attr_uuids_by_name_;
            QMap<QUuid, QString> attr_names_by_uuid_;
            QMap<QUuid, QVariant> attr_values_by_uuid_;
            QStringList attrs_group_name_;
            QString role_name_;
        };


        class OrderedModuleAttrsModel : public QSortFilterProxyModel {
            Q_OBJECT

            Q_PROPERTY(QStringList attributesGroupNames READ attributesGroupNames WRITE
                           setattributesGroupNames NOTIFY attributesGroupNamesChanged)

          public:
            OrderedModuleAttrsModel(QObject *parent = nullptr);
            // ~OrderedModuleAttrsModel() override = default;

          signals:
            void attributesGroupNamesChanged(QStringList group_name);

          public slots:
            [[nodiscard]] QStringList attributesGroupNames() const;
            void setattributesGroupNames(const QStringList &group_name);
        };


        class ModuleAttrsModel : public QAbstractListModel {

            Q_OBJECT

          public:
            Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)

            Q_PROPERTY(QStringList attributesGroupNames READ attributesGroupNames WRITE
                           setattributesGroupNames NOTIFY attributesGroupNamesChanged)

            ModuleAttrsModel(QObject *parent = nullptr);
            virtual ~ModuleAttrsModel();

            [[nodiscard]] int rowCount() { return rowCount(QModelIndex()); }

            [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

            [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

            bool setData(const QModelIndex &index, const QVariant &value, int role) override;

            void add_attributes_from_backend(
                const module::AttributeSet &attrs, const bool check_group = false);

            [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &) const override {
                return Qt::ItemIsEnabled | Qt::ItemIsEditable;
            }

            void update_attribute_from_backend(
                const utility::Uuid &attr_uuid,
                const int role,
                const utility::JsonStore &role_value);

            void update_full_attribute_from_backend(const module::ConstAttributePtr &attr);

            void remove_attribute(const utility::Uuid &attr_uuid);

          signals:

            void setAttributeFromFrontEnd(const QUuid, const int, const QVariant);
            void rowCountChanged();
            void doFullupdateFromBackend(QStringList group_name);
            void attributesGroupNamesChanged(QStringList group_name);

          public slots:

            [[nodiscard]] QStringList attributesGroupNames() const { return attrs_group_name_; }

            void setattributesGroupNames(QStringList group_name);

          private:
            bool have_attr(const QUuid &uuid);

            std::vector<QMap<int, QVariant>> attributes_data_;
            QStringList attrs_group_name_;
        };

        class ModuleAttrsToQMLShim : public QMLActor {

            Q_OBJECT

          public:
            ModuleAttrsToQMLShim(ModuleAttrsModel *model);
            ModuleAttrsToQMLShim(ModuleAttrsDirect *model);
            ModuleAttrsToQMLShim(ModuleMenusModel *model);
            ~ModuleAttrsToQMLShim() override;

            void init(caf::actor_system &system) override;

            void clear() {
                model_         = nullptr;
                qml_attrs_map_ = nullptr;
                menu_model_    = nullptr;
            }

          signals:

          public slots:

            void setAttributeFromFrontEnd(
                const QUuid property_uuid, const int role, const QVariant role_value);
            void setattributesGroupNames(QStringList group_name);
            void setRootMenuName(QString root_menu_name);
            void doFullupdateFromBackend(QStringList group_name);

          private:
            ModuleAttrsModel *model_          = {nullptr};
            ModuleAttrsDirect *qml_attrs_map_ = {nullptr};
            ModuleMenusModel *menu_model_     = {nullptr};
            utility::Uuid uuid_;
            caf::actor attrs_events_actor_;
            caf::actor attrs_events_actor_group_;
            QStringList attrs_group_name_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio