// SPDX-License-Identifier: Apache-2.0
#pragma once

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/global_store_qml_export.h"

#include <caf/all.hpp>

#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"


CAF_PUSH_WARNINGS

CAF_POP_WARNINGS

namespace xstudio::global_store {
class GlobalStoreHelper;
}

namespace xstudio::ui::qml {
using namespace caf;

class GLOBAL_STORE_QML_EXPORT GlobalStoreModel
    : public caf::mixin::actor_object<JSONTreeModel> {
    Q_OBJECT

    Q_PROPERTY(bool autosave READ autosave WRITE setAutosave NOTIFY autosaveChanged)

    enum Roles {
        nameRole = JSONTreeModel::Roles::LASTROLE,
        pathRole,
        datatypeRole,
        contextRole,
        valueRole,
        descriptionRole,
        defaultValueRole,
        overriddenValueRole,
        overriddenPathRole
    };

  public:
    using super = caf::mixin::actor_object<JSONTreeModel>;
    explicit GlobalStoreModel(QObject *parent = nullptr);
    virtual void init(caf::actor_system &system);

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    [[nodiscard]] bool autosave() const { return autosave_; }

    void setAutosave(const bool enabled = true);

  signals:
    void autosaveChanged();

  public:
    caf::actor_system &system() { return self()->home_system(); }

  private:
    static nlohmann::json storeToTree(const nlohmann::json &src);

    bool updateProperty(const std::string &path, const utility::JsonStore &change);


    std::shared_ptr<global_store::GlobalStoreHelper> gsh_;
    bool autosave_{false};
};

/* A specialised version of the GlobalStoreModel where only 'public' data
entries from the GlobalStore are exposed in a flattened tree. The purpose of
this is to drive the preferences panel in the front end. */
class GLOBAL_STORE_QML_EXPORT PublicPreferencesModel
    : public caf::mixin::actor_object<JSONTreeModel> {
    Q_OBJECT

    Q_PROPERTY(bool autosave READ autosave WRITE setAutosave NOTIFY autosaveChanged)

    enum Roles {
        nameRole = JSONTreeModel::Roles::LASTROLE,
        pathRole,
        datatypeRole,
        contextRole,
        valueRole,
        descriptionRole,
        defaultValueRole,
        overriddenValueRole,
        overriddenPathRole,
        displayNameRole,
        categoryRole,
        optionsRole
    };

  public:
    using super = caf::mixin::actor_object<JSONTreeModel>;
    explicit PublicPreferencesModel(QObject *parent = nullptr);
    virtual void init(caf::actor_system &system);

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    [[nodiscard]] bool autosave() const { return autosave_; }

    void setAutosave(const bool enabled = true);

  signals:
    void autosaveChanged();

  public:
    caf::actor_system &system() { return self()->home_system(); }

  private:
    void buildModel(const utility::JsonStore &entireGlobalStore);

    void storeToTree(const nlohmann::json &src, nlohmann::json &tree);

    bool updateProperty(const std::string &path, const utility::JsonStore &change);


    std::shared_ptr<global_store::GlobalStoreHelper> gsh_;
    bool autosave_{false};
};


} // namespace xstudio::ui::qml
