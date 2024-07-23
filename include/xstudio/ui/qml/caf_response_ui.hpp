// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <nlohmann/json.hpp>

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace caf;


class CafResponse : public QObject {
    Q_OBJECT

  signals:
    //  Search value, search role,  search hint, set role, set value
    void received(QVariant, int, QPersistentModelIndex, int, QString);
    //  Search value, search role,  set role
    void finished(QVariant, int, int);

  public:
    CafResponse(
        const QVariant search_value,
        const int search_role,
        const QPersistentModelIndex search_hint,
        const nlohmann::json &data,
        int role,
        const std::string &role_name,
        QThreadPool *pool);

    CafResponse(
        const QVariant search_value,
        const int search_role,
        const QPersistentModelIndex search_hint,
        const nlohmann::json &data,
        int role,
        const std::string &role_name,
        const std::map<int, std::string> &metadata_paths,
        QThreadPool *pool);


  private:
    void handleFinished();

    QFutureWatcher<QMap<int, QString>> watcher_;

    const QVariant search_value_;
    const int search_role_;
    const QPersistentModelIndex search_hint_;
    const int role_;
};

} // namespace xstudio::ui::qml