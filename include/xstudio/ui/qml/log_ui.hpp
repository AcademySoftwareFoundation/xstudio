// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <iostream>

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/log_qml_export.h"

#include "spdlog/common.h"
#include "spdlog/details/log_msg.h"
#include "spdlog/details/synchronous_factory.h"
#include "spdlog/sinks/base_sink.h"

#include <QSortFilterProxyModel>
#include <QAbstractListModel>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QUrl>

namespace xstudio {
namespace ui {
    namespace qml {

        struct LogItem {
            QDateTime timeStamp;
            int level;
            QString text;
        };


        class LOG_QML_EXPORT LogModel : public QAbstractListModel {
            Q_OBJECT
            Q_PROPERTY(QStringList logLevels READ logLevels NOTIFY logLevelsChanged)

          public:
            enum LogRoles {
                TimeRole = Qt::UserRole + 1,
                LevelRole,
                LevelStringRole,
                StringRole
            };
            inline static const std::map<int, std::string> role_names = {
                {TimeRole, "timeRole"},
                {LevelRole, "levelRole"},
                {LevelStringRole, "levelStringRole"},
                {StringRole, "stringRole"}};

            LogModel(QObject *parent = nullptr);
            [[nodiscard]] int
            rowCount(const QModelIndex &parent = QModelIndex()) const override;
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            [[nodiscard]] QStringList logLevels() const { return log_levels_; }

            static QString logLevelToQString(const int level);

          signals:
            void logLevelsChanged();
            void newLogRecord(const QDateTime &timestamp, const int level, const QString &text);

          public slots:
            void appendLog(const QDateTime &timestamp, const int level, const QString &text);

          private:
            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

            QList<LogItem> data_;
            QStringList log_levels_;
        };
        // trace = SPDLOG_LEVEL_TRACE, debug = SPDLOG_LEVEL_DEBUG, info = SPDLOG_LEVEL_INFO,
        // warn = SPDLOG_LEVEL_WARN,
        //  err = SPDLOG_LEVEL_ERROR, critical = SPDLOG_LEVEL_CRITICAL, off =
        //  SPDLOG_LEVEL_OFF

        class LOG_QML_EXPORT LogFilterModel : public QSortFilterProxyModel {
            Q_OBJECT
            Q_PROPERTY(int logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
            Q_PROPERTY(QString logLevelString READ logLevelString NOTIFY logLevelStringChanged)
            Q_PROPERTY(QStringList logLevels READ logLevels NOTIFY logLevelsChanged)

          public:
            LogFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
                setFilterRole(LogModel::StringRole);
                setFilterCaseSensitivity(Qt::CaseInsensitive);
                setDynamicSortFilter(true);
                QObject::connect(this, &QSortFilterProxyModel::sourceModelChanged, [=]() {
                    if (auto m = dynamic_cast<LogModel *>(sourceModel())) {
                        QObject::connect(
                            m, &LogModel::newLogRecord, this, &LogFilterModel::newLogRecord);
                    }
                });
            }
            [[nodiscard]] int logLevel() const { return log_Level_; }
            [[nodiscard]] QString logLevelString() const;
            [[nodiscard]] QStringList logLevels() const;

          protected:
            [[nodiscard]] bool
            filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

          signals:
            void logLevelChanged();
            void logLevelStringChanged();
            void logLevelsChanged();
            void newLogRecord(const QDateTime &timestamp, const int level, const QString &text);

          public slots:
            void setLogLevel(const int level);
            void copyLogToClipboard(const int start = 0, int count = -1) const;
            void saveLogToFile(const QUrl &path, const int start = 0, int count = -1) const;

          private:
            void saveLogToFile(const QString &path, const int start = 0, int count = -1) const;
            [[nodiscard]] QString logText(const int start, int count) const;
            int log_Level_{SPDLOG_LEVEL_INFO};
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio


//
// qt_sink class
//
namespace spdlog {
namespace sinks {
    template <typename Mutex> class qtlog_sink : public base_sink<Mutex> {
      public:
        qtlog_sink(QObject *qt_object) { qt_object_ = qt_object; }

        ~qtlog_sink() override { flush_(); }
        // [2022-05-20 10:51:17.004] [xstudio] [debug] Autosaved APPLICATION

      protected:
        void sink_it_(const details::log_msg &msg) override {
            memory_buf_t formatted;
            base_sink<Mutex>::formatter_->format(msg, formatted);
            // string_view_t str = string_view_t(formatted.data(), formatted.size());
            auto str = std::string(formatted.data(), formatted.size());
            auto np  = str.find_first_of(']');
            np       = str.find_first_of(']', np + 1);
            np       = str.find_first_of(']', np + 1) + 2;

            if (np != std::string::npos)
                str = str.substr(np);

            auto qstr = QString::fromUtf8(str.data(), static_cast<int>(str.size())).trimmed();
            // trim preamble
            // [2022-05-19 15:50:41.323] [xstudio] [info]

            QMetaObject::invokeMethod(
                qt_object_,
                "appendLog",
                Qt::AutoConnection,
                Q_ARG(
                    const QDateTime &,
                    QDateTime::fromSecsSinceEpoch(
                        std::chrono::duration_cast<std::chrono::seconds>(
                            msg.time.time_since_epoch())
                            .count())),
                Q_ARG(const int, msg.level),
                Q_ARG(const QString &, qstr));
        }

        void flush_() override {}

      private:
        QObject *qt_object_ = nullptr;
    };

#include "spdlog/details/null_mutex.h"
#include <mutex>
    using qtlog_sink_mt = qtlog_sink<std::mutex>;
    using qtlog_sink_st = qtlog_sink<spdlog::details::null_mutex>;
} // namespace sinks

//
// Factory functions
//
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger>
qtlog_sink_logger_mt(const std::string &logger_name, QObject *qt_object) {
    return Factory::template create<sinks::qtlog_sink_mt>(logger_name, qt_object);
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger>
qtlog_sink_logger_st(const std::string &logger_name, QObject *qt_object) {
    return Factory::template create<sinks::qtlog_sink_st>(logger_name, qt_object);
}

} // namespace spdlog