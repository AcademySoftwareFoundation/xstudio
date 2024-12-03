// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <caf/all.hpp>

#include "xstudio/ui/qml/log_ui.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "spdlog/common.h"

CAF_PUSH_WARNINGS
#include <QHash>
#include <QByteArray>
#include <QModelIndex>
#include <QVariant>
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QTextStream>
CAF_POP_WARNINGS

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;


QString LogFilterModel::logLevelString() const {
    return LogModel::logLevelToQString(log_Level_);
}

void LogFilterModel::setLogLevel(const int level) {
    if (level != log_Level_) {
        log_Level_ = level;
        invalidateFilter();
        emit logLevelChanged();
        emit logLevelStringChanged();
    }
}

QStringList LogFilterModel::logLevels() const {
    try {
        auto ll = qobject_cast<LogModel *>(sourceModel());
        return ll->logLevels();
    } catch (...) {
    }
    return QStringList();
}

bool LogFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    // check level
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);

    if (sourceModel()->data(index, LogModel::LevelRole).toInt() < log_Level_)
        return false;

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

QString LogFilterModel::logText(const int start, int count) const {
    QString txt;

    if (count == -1)
        count = rowCount(QModelIndex());

    for (int row = start; row < start + count; row++) {
        auto ind = index(row, 0);
        txt += "[" + data(ind, LogModel::TimeRole).toString() + "] [";
        txt += LogModel::logLevelToQString(data(ind, LogModel::LevelRole).toInt()) + "] ";
        txt += data(ind, LogModel::StringRole).toString() + "\n";
    }

    return txt;
}

void LogFilterModel::copyLogToClipboard(const int start, const int count) const {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(logText(start, count));
}

void LogFilterModel::saveLogToFile(const QUrl &path, const int start, const int count) const {
    saveLogToFile(path.path(), start, count);
}

void LogFilterModel::saveLogToFile(
    const QString &path, const int start, const int count) const {
    QFile fh(path);
    if (fh.open(QIODevice::WriteOnly)) {
        QTextStream out(&fh);
        out << logText(start, count);
        fh.close();
    }
}

LogModel::LogModel(QObject *parent) : QAbstractListModel(parent) {
    log_levels_.append("All");
    // QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    for (int i = spdlog::level::debug; i <= spdlog::level::off; i++) {
        log_levels_.append(logLevelToQString(i));
    }
}

QString LogModel::logLevelToQString(const int level) {
    auto str  = spdlog::level::to_string_view(static_cast<spdlog::level::level_enum>(level));
    auto qstr = QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    qstr[0]   = qstr[0].toUpper();
    return qstr;
}

QVariant LogModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();
    if (index.row() < data_.size()) {
        switch (role) {
        case TimeRole:
            result = data_[index.row()].timeStamp;
            break;

        case LevelRole:
            result = data_[index.row()].level;
            break;

        case LevelStringRole:
            result = logLevelToQString(data_[index.row()].level);
            break;

        case StringRole:
            result = data_[index.row()].text;
            break;
        }
    }

    return result;
}

QHash<int, QByteArray> LogModel::roleNames() const {
    QHash<int, QByteArray> roles;
    for (const auto &p : LogModel::role_names) {
        roles[p.first] = p.second.c_str();
    }
    return roles;
}

int LogModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return data_.size();
}


void LogModel::appendLog(const QDateTime &timestamp, const int level, const QString &text) {
    beginInsertRows(QModelIndex(), data_.size(), data_.size());
    data_.append({timestamp, level, text});
    endInsertRows();
    emit newLogRecord(timestamp, level, text);
}
