// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/utility/file_system_item.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"


CAF_PUSH_WARNINGS
#include <QUrl>
CAF_POP_WARNINGS

namespace xstudio::session_snapshots {

using namespace caf;

class SessionSnapshotsPlugin : public plugin::StandardPlugin {
  public:
    inline static const utility::Uuid PLUGIN_UUID =
        utility::Uuid("5a095fde-296c-4d78-90e1-7da55ee44806");

    SessionSnapshotsPlugin(caf::actor_config &cfg, const utility::JsonStore &init_settings);
    virtual ~SessionSnapshotsPlugin() = default;
};

class SnapshotMenuModel : public ui::qml::JSONTreeModel {
    Q_OBJECT

    Q_PROPERTY(QVariant paths READ paths WRITE setPaths NOTIFY pathsChanged)

  public:
    explicit SnapshotMenuModel(QObject *parent = nullptr);

    QVariant paths() const { return paths_; }
    void setPaths(const QVariant &value);

    Q_INVOKABLE bool createFolder(const QModelIndex &index, const QString &name);

    Q_INVOKABLE void rescan(const QModelIndex &index = QModelIndex(), const int depth = 0);
    Q_INVOKABLE QUrl buildSavePath(const QModelIndex &index, const QString &name) const;

    QML_NAMED_ELEMENT("SnapshotMenuModel")

  signals:
    void pathsChanged();

  protected:
    utility::JsonStore toMenuModelItemData(utility::FileSystemItem *item);


  private:
    utility::FileSystemItem items_;

    QVariant paths_;
};

} // namespace xstudio::session_snapshots
