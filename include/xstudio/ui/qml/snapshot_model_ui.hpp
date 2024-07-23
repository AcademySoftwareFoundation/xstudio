// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/utility/file_system_item.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QUrl>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace caf;

class HELPER_QML_EXPORT SnapshotModel : public JSONTreeModel {
    Q_OBJECT

    Q_PROPERTY(QVariant paths READ paths WRITE setPaths NOTIFY pathsChanged)

  public:
    enum Roles {
        childrenRole = JSONTreeModel::Roles::LASTROLE,
        mtimeRole,
        nameRole,
        pathRole,
        typeRole,
    };


    explicit SnapshotModel(QObject *parent = nullptr);

    QVariant paths() const { return paths_; }
    void setPaths(const QVariant &value);

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE bool createFolder(const QModelIndex &index, const QString &name);

    Q_INVOKABLE void rescan(const QModelIndex &index = QModelIndex(), const int depth = 0);
    Q_INVOKABLE QUrl buildSavePath(const QModelIndex &index, const QString &name) const;

  signals:
    void pathsChanged();

  protected:
    void sortByName(nlohmann::json &jsn);
    nlohmann::json sortByNameType(const nlohmann::json &jsn) const;


  private:
    utility::FileSystemItem items_;

    QVariant paths_;
};

} // namespace xstudio::ui::qml
