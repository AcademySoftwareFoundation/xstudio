// SPDX-License-Identifier: Apache-2.0
#pragma once


#ifndef GLOBAL_STORE_QML_EXPORT_H
#define GLOBAL_STORE_QML_EXPORT_H

#ifdef GLOBAL_STORE_QML_STATIC_DEFINE
#  define GLOBAL_STORE_QML_EXPORT
#  define GLOBAL_STORE_QML_NO_EXPORT
#else
#  ifndef GLOBAL_STORE_QML_EXPORT
#    ifdef global_store_qml_EXPORTS
        /* We are building this library */
#      define GLOBAL_STORE_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define GLOBAL_STORE_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef GLOBAL_STORE_QML_NO_EXPORT
#    define GLOBAL_STORE_QML_NO_EXPORT 
#  endif
#endif

#ifndef GLOBAL_STORE_QML_DEPRECATED
#  define GLOBAL_STORE_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef GLOBAL_STORE_QML_DEPRECATED_EXPORT
#  define GLOBAL_STORE_QML_DEPRECATED_EXPORT GLOBAL_STORE_QML_EXPORT GLOBAL_STORE_QML_DEPRECATED
#endif

#ifndef GLOBAL_STORE_QML_DEPRECATED_NO_EXPORT
#  define GLOBAL_STORE_QML_DEPRECATED_NO_EXPORT GLOBAL_STORE_QML_NO_EXPORT GLOBAL_STORE_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef GLOBAL_STORE_QML_NO_DEPRECATED
#    define GLOBAL_STORE_QML_NO_DEPRECATED
#  endif
#endif

#endif /* GLOBAL_STORE_QML_EXPORT_H */


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

class GLOBAL_STORE_QML_EXPORT GlobalStoreModel : public caf::mixin::actor_object<JSONTreeModel> {
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

} // namespace xstudio::ui::qml
