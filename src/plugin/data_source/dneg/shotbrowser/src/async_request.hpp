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


class ShotBrowserResponse : public QObject {
    Q_OBJECT

  signals:
    //  Search value, search role,  search hint, set role, set value
    void received(QPersistentModelIndex, int, QString);

  public:
    ShotBrowserResponse(
        const QPersistentModelIndex index,
        int role,
        const nlohmann::json &request,
        const caf::actor backend,
        QThreadPool *pool = QThreadPool::globalInstance());

  private:
    void handleFinished();

    QFutureWatcher<QString> watcher_;

    const QPersistentModelIndex index_;
    const nlohmann::json request_;
    const int role_;
    const caf::actor backend_;
};

} // namespace xstudio::ui::qml