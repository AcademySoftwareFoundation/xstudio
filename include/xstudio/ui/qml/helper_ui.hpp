// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <functional>
#include <semver.hpp>
#include <filesystem>

#include "xstudio/ui/qml/actor_object.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/helper_qml_export.h"

CAF_PUSH_WARNINGS
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QImage>
#include <QPixmap>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPointF>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QString>
#include <QtConcurrent>
#include <QUrl>
#include <QUuid>

CAF_POP_WARNINGS


namespace xstudio {
namespace ui {
    namespace qml {
        using namespace caf;

        namespace fs = std::filesystem;

        QVariant mapFromValue(const nlohmann::json &value);
        nlohmann::json mapFromValue(const QVariant &value);

        inline QString QStringFromStd(const std::string &str) {
            return QString::fromUtf8(str.c_str());
        }
        inline std::string StdFromQString(const QString &str) {
            return str.toUtf8().constData();
        }

        /* Get the name of the window that this QObject belongs to (if any)*/
        inline QString item_window_name(QObject *obj) {

            if (qmlContext(obj)) {
                QQmlContext *c = qmlContext(obj);
                QObject *pobj  = obj;
                while (c) {
                    QObject *cobj = c->contextObject();
                    if (cobj && cobj->isWindowType()) {
                        return cobj->objectName();
                    }
                    pobj = cobj;
                    c    = c->parentContext();
                }
            }
            return QString();
        }

        class HELPER_QML_EXPORT TimeCode : public QObject {
            Q_OBJECT

            Q_PROPERTY(unsigned int hours READ hours WRITE setHours NOTIFY timeCodeChanged)
            Q_PROPERTY(
                unsigned int minutes READ minutes WRITE setMinutes NOTIFY timeCodeChanged)
            Q_PROPERTY(
                unsigned int seconds READ seconds WRITE setSeconds NOTIFY timeCodeChanged)
            Q_PROPERTY(unsigned int frames READ frames WRITE setFrames NOTIFY timeCodeChanged)
            Q_PROPERTY(
                double frameRate READ frameRate WRITE setFrameRate NOTIFY timeCodeChanged)
            Q_PROPERTY(bool dropFrame READ dropFrame WRITE setDropFrame NOTIFY timeCodeChanged)
            Q_PROPERTY(unsigned int totalFrames READ totalFrames WRITE setTotalFrames NOTIFY
                           timeCodeChanged)
            Q_PROPERTY(QString timeCode READ timeCode NOTIFY timeCodeChanged)

          public:
            explicit TimeCode(QObject *parent = nullptr) : QObject(parent) {}

            [[nodiscard]] unsigned int hours() const { return timecode_.hours(); }
            [[nodiscard]] unsigned int minutes() const { return timecode_.minutes(); }
            [[nodiscard]] unsigned int seconds() const { return timecode_.seconds(); }
            [[nodiscard]] unsigned int frames() const { return timecode_.frames(); }
            [[nodiscard]] double frameRate() const { return timecode_.framerate(); }
            [[nodiscard]] bool dropFrame() const { return timecode_.dropframe(); }
            [[nodiscard]] unsigned int totalFrames() const { return timecode_.total_frames(); }
            [[nodiscard]] QString timeCode() const {
                return QStringFromStd(timecode_.to_string());
            }

            void setHours(const unsigned int value) {
                timecode_.hours(value);
                emit timeCodeChanged();
            }

            void setMinutes(const unsigned int value) {
                timecode_.minutes(value);
                emit timeCodeChanged();
            }

            void setSeconds(const unsigned int value) {
                timecode_.seconds(value);
                emit timeCodeChanged();
            }

            void setFrames(const unsigned int value) {
                timecode_.frames(value);
                emit timeCodeChanged();
            }

            void setFrameRate(const double value) {
                timecode_.framerate(value);
                emit timeCodeChanged();
            }

            void setDropFrame(const bool value) {
                timecode_.dropframe(value);
                emit timeCodeChanged();
            }

            void setTotalFrames(const unsigned int value) {
                timecode_.total_frames(value);
                emit timeCodeChanged();
            }

            Q_INVOKABLE void setTimeCodeFromString(
                const QString &code,
                const double frameRate = 30.0,
                const bool dropFrame   = false) {
                timecode_ = utility::Timecode(StdFromQString(code), frameRate, dropFrame);
                emit timeCodeChanged();
            }

            Q_INVOKABLE void setTimeCodeFromFrames(
                const unsigned int frames,
                const double frameRate = 30.0,
                const bool dropFrame   = false) {
                timecode_ = utility::Timecode(frames, frameRate, dropFrame);
                emit timeCodeChanged();
            }

            Q_INVOKABLE void setTimeCode(
                const unsigned int hour,
                const unsigned int minute,
                const unsigned int second,
                const unsigned int frame,
                const double frameRate = 30.0,
                const bool dropFrame   = false) {
                timecode_ =
                    utility::Timecode(hour, minute, second, frame, frameRate, dropFrame);
                emit timeCodeChanged();
            }


          signals:
            void timeCodeChanged();

          private:
            utility::Timecode timecode_;
        };

        class HELPER_QML_EXPORT ModelRowCount : public QObject {
            Q_OBJECT

            Q_PROPERTY(QModelIndex index READ index WRITE setIndex NOTIFY indexChanged)
            Q_PROPERTY(int count READ count NOTIFY countChanged)

          public:
            explicit ModelRowCount(QObject *parent = nullptr) : QObject(parent) {}

            [[nodiscard]] QModelIndex index() const { return index_; }
            [[nodiscard]] int count() const { return count_; }

            Q_INVOKABLE void setIndex(const QModelIndex &index);

          signals:
            void indexChanged();
            void countChanged();

          private slots:
            void inserted(const QModelIndex &parent, int first, int last);
            void moved(
                const QModelIndex &sourceParent,
                int sourceStart,
                int sourceEnd,
                const QModelIndex &destinationParent,
                int destinationRow);
            void removed(const QModelIndex &parent, int first, int last);

          private:
            void setCount(const int count);

            QPersistentModelIndex index_;
            int count_{0};
        };

        class HELPER_QML_EXPORT ModelProperty : public QObject {
            Q_OBJECT

            Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
            Q_PROPERTY(int roleId READ roleId NOTIFY roleIdChanged)
            Q_PROPERTY(QString role READ role WRITE setRole NOTIFY roleChanged)
            Q_PROPERTY(QModelIndex index READ index WRITE setIndex NOTIFY indexChanged)

          public:
            explicit ModelProperty(QObject *parent = nullptr) : QObject(parent) {}

            [[nodiscard]] QModelIndex index() const { return index_; }
            [[nodiscard]] QVariant value() const { return value_; }
            [[nodiscard]] int roleId() const { return role_id_; }
            [[nodiscard]] QString role() const { return role_; }

            Q_INVOKABLE void setIndex(const QModelIndex &index);
            Q_INVOKABLE void setRole(const QString &role);
            Q_INVOKABLE void setValue(const QVariant &value);

          signals:
            void valueChanged();
            void roleIdChanged();
            void roleChanged();
            void indexChanged();

          private slots:
            void dataChanged(
                const QModelIndex &topLeft,
                const QModelIndex &bottomRight,
                const QVector<int> &roles = QVector<int>());
            void removed(const QModelIndex &parent, int first, int last);


          private:
            bool updateValue();
            [[nodiscard]] int getRoleId(const QString &role) const;

            QPersistentModelIndex index_;
            QString role_;
            int role_id_{-1};
            QVariant value_;
        };

        class HELPER_QML_EXPORT ModelPropertyTree : public JSONTreeModel {
            Q_OBJECT

            Q_PROPERTY(QModelIndex index READ index WRITE setIndex NOTIFY indexChanged)
            Q_PROPERTY(int roleId READ roleId NOTIFY roleIdChanged)
            Q_PROPERTY(QString role READ role WRITE setRole NOTIFY roleChanged)

          public:
            explicit ModelPropertyTree(QObject *parent = nullptr) : JSONTreeModel(parent) {}

            [[nodiscard]] QModelIndex index() const { return index_; }
            [[nodiscard]] int roleId() const { return role_id_; }
            [[nodiscard]] QString role() const { return role_; }

            Q_INVOKABLE void setIndex(const QModelIndex &index);
            Q_INVOKABLE void setRole(const QString &role);

          signals:
            void roleIdChanged();
            void roleChanged();
            void indexChanged();

          private slots:
            void myDataChanged(
                const QModelIndex &topLeft,
                const QModelIndex &bottomRight,
                const QVector<int> &roles = QVector<int>());

          private:
            bool updateValue();
            [[nodiscard]] int getRoleId(const QString &role) const;

            QPersistentModelIndex index_;
            QString role_;
            int role_id_{-1};
            QVariant value_;
        };


        class HELPER_QML_EXPORT ModelPropertyMap : public QObject {
            Q_OBJECT

            Q_PROPERTY(QQmlPropertyMap *values READ values NOTIFY valuesChanged)
            Q_PROPERTY(QModelIndex index READ index WRITE setIndex NOTIFY indexChanged)

          public:
            explicit ModelPropertyMap(QObject *parent = nullptr);

            [[nodiscard]] QModelIndex index() const { return index_; }
            [[nodiscard]] QQmlPropertyMap *values() const { return values_; }

            Q_INVOKABLE void setIndex(const QModelIndex &index);
            Q_INVOKABLE void dump();

          signals:
            void indexChanged();
            void valuesChanged();
            void contentChanged();

          protected slots:
            void dataChanged(
                const QModelIndex &topLeft,
                const QModelIndex &bottomRight,
                const QVector<int> &roles = QVector<int>());
            void valueChangedSlot(const QString &key, const QVariant &value);

          protected:
            virtual void valueChanged(const QString &key, const QVariant &value);
            virtual bool updateValues(const QVector<int> &roles = {});
            [[nodiscard]] int getRoleId(const QString &role) const;

            QPersistentModelIndex index_;
            QQmlPropertyMap *values_{nullptr};
        };

        class HELPER_QML_EXPORT PreferencePropertyMap : public ModelPropertyMap {
            Q_OBJECT

            Q_PROPERTY(QVariant value READ value WRITE setMyValue NOTIFY myValueChanged)

            Q_PROPERTY(QVariant dataType READ dataType NOTIFY dataTypeChanged)
            Q_PROPERTY(QVariant context READ context NOTIFY contextChanged)
            Q_PROPERTY(QVariant name READ name NOTIFY nameChanged)
            Q_PROPERTY(QVariant defaultValue READ defaultValue NOTIFY defaultValueChanged)
            Q_PROPERTY(QVariant jsonString READ jsonString NOTIFY jsonStringChanged)

          public:
            explicit PreferencePropertyMap(QObject *parent = nullptr)
                : ModelPropertyMap(parent) {}
            [[nodiscard]] QVariant value() const { return values_->value("valueRole"); }
            [[nodiscard]] QVariant dataType() const { return values_->value("datatypeRole"); }
            [[nodiscard]] QVariant context() const { return values_->value("contextRole"); }
            [[nodiscard]] QVariant name() const { return values_->value("nameRole"); }
            [[nodiscard]] QVariant defaultValue() const {
                return values_->value("defaultValueRole");
            }
            [[nodiscard]] QVariant jsonString() const { return values_->value("jsonTextRole"); }

            void setMyValue(const QVariant &value);

          signals:
            void myValueChanged();
            void dataTypeChanged();
            void contextChanged();
            void nameChanged();
            void defaultValueChanged();
            void jsonStringChanged();

          protected:
            void valueChanged(const QString &key, const QVariant &value) override;

          private:
            void emitChange(const QString &key);
        };

        class HELPER_QML_EXPORT ModelNestedPropertyMap : public ModelPropertyMap {
            Q_OBJECT

          public:
            explicit ModelNestedPropertyMap(QObject *parent = nullptr)
                : ModelPropertyMap(parent) {}


          protected:
            bool updateValues(const QVector<int> &roles = {}) override;
            void valueChanged(const QString &key, const QVariant &value) override;

            QString data_role_    = {"valueRole"};
            QString default_role_ = {"defaultValueRole"};
        };

        class HELPER_QML_EXPORT CafSystemObject : public QObject {

            Q_OBJECT

          public:
            CafSystemObject(QObject *parent, caf::actor_system &sys);

            ~CafSystemObject() override = default;

            caf::actor_system &actor_system() { return system_ref_.get(); }

            static caf::actor_system &get_actor_system();

          private:
            std::reference_wrapper<caf::actor_system> system_ref_;
        };

        inline QUrl QUrlFromUri(const caf::uri &uri) {
            if (uri.empty())
                return QUrl();
            return QUrl(QStringFromStd(to_string(uri)));
        }

        inline caf::uri UriFromQUrl(const QUrl &url) {
            // try {
            // 	return utility::url_to_uri(StdFromQString(url.toEncoded()));
            // std::cerr << StdFromQString(url.toEncoded()) <<std::endl;
            auto uri = caf::make_uri(StdFromQString(url.toEncoded()));
            if (uri)
                return *uri;
            // } catch(...) {
            // }
            return caf::uri();
        }

        inline QUuid QUuidFromUuid(const utility::Uuid &uuid) {
            return QUuid(QStringFromStd(to_string(uuid)));
        }

        inline utility::Uuid UuidFromQUuid(const QUuid &quuid) {
            return utility::Uuid(StdFromQString(quuid.toString()));
        }

        inline QVariantMap QVariantMapFromJson(const utility::JsonStore &js) {
            return QJsonDocument::fromJson(QStringFromStd(js.dump()).toUtf8())
                .object()
                .toVariantMap();
        }

        inline QVariantList QVariantListFromJson(const utility::JsonStore &js) {
            return QJsonDocument::fromJson(QStringFromStd(js.dump()).toUtf8())
                .array()
                .toVariantList();
        }

        inline QVariantList QVariantListFromJson(const std::string &js) {
            return QJsonDocument::fromJson(QStringFromStd(js).toUtf8()).array().toVariantList();
        }

        inline QVariant QVariantFromJson(const utility::JsonStore &js) {
            return QJsonDocument::fromJson(QStringFromStd(js.dump()).toUtf8())
                .object()
                .toVariantMap();
        }

        nlohmann::json qvariant_to_json(const QVariant &var);

        QVariant json_to_qvariant(const nlohmann::json &var);

        QString actorToQString(actor_system &sys, const caf::actor &actor);
        caf::actor actorFromQString(actor_system &sys, const QString &addr);

        std::string actorToString(actor_system &sys, const caf::actor &actor);
        caf::actor actorFromString(actor_system &sys, const std::string &addr);

        QString getThumbnailURL(
            actor_system &sys,
            const caf::actor &actor,
            const int frame          = 0,
            const bool cache_to_disk = false);

        inline utility::JsonStore dropToJsonStore(const QVariantMap &drop) {
            auto jsn = qvariant_to_json(drop);
            std::regex rgx(R"(\r\n|\n)"); //, std::regex::extended);
            for (auto i : jsn.items()) {
                if (i.value().is_string()) {
                    jsn[i.key()] = utility::resplit(i.value(), rgx);
                } else {
                    jsn[i.key()] = i.value();
                }
            }
            return jsn;
        }

        class HELPER_QML_EXPORT Helpers : public QObject {
            Q_OBJECT

          public:
            Helpers(QQmlEngine *engine, QObject *parent = nullptr)
                : QObject(parent), engine_(engine) {}
            ~Helpers() override = default;

            Q_INVOKABLE [[nodiscard]] bool openURL(const QUrl &url) const {
                return openURLFuture(url).result();
            }

            Q_INVOKABLE [[nodiscard]] QFuture<bool> openURLFuture(const QUrl &url) const {
                return QtConcurrent::run([=]() { return QDesktopServices::openUrl(url); });
            }

            Q_INVOKABLE [[nodiscard]] QModelIndex qModelIndex() const { return QModelIndex(); }

            Q_INVOKABLE [[nodiscard]] bool showURIS(const QList<QUrl> &urls) const {
                auto uris = QStringList();
                for (const auto &i : urls)
                    uris.push_back(i.toString());

                auto arguments = QStringList(
                    {"--session",
                     "--print-reply",
                     "--dest=org.freedesktop.FileManager1",
                     "--type=method_call",
                     "/org/freedesktop/FileManager1",
                     "org.freedesktop.FileManager1.ShowItems",
                     QString("array:string:") + uris.join(','),
                     "string:"});

                return startDetachedProcess("dbus-send", arguments);
            }

            Q_INVOKABLE void setOverrideCursor(const QString &name = "") {
                const static auto cursors = std::map<QString, Qt::CursorShape>(
                    {{"Qt.ArrowCursor", Qt::ArrowCursor},
                     {"Qt.UpArrowCursor", Qt::UpArrowCursor},
                     {"Qt.CrossCursor", Qt::CrossCursor},
                     {"Qt.WaitCursor", Qt::WaitCursor},
                     {"Qt.IBeamCursor", Qt::IBeamCursor},
                     {"Qt.SizeVerCursor", Qt::SizeVerCursor},
                     {"Qt.SizeHorCursor", Qt::SizeHorCursor},
                     {"Qt.SizeBDiagCursor", Qt::SizeBDiagCursor},
                     {"Qt.SizeFDiagCursor", Qt::SizeFDiagCursor},
                     {"Qt.SizeAllCursor", Qt::SizeAllCursor},
                     {"Qt.BlankCursor", Qt::BlankCursor},
                     {"Qt.SplitVCursor", Qt::SplitVCursor},
                     {"Qt.SplitHCursor", Qt::SplitHCursor},
                     {"Qt.PointingHandCursor", Qt::PointingHandCursor},
                     {"Qt.ForbiddenCursor", Qt::ForbiddenCursor},
                     {"Qt.OpenHandCursor", Qt::OpenHandCursor},
                     {"Qt.ClosedHandCursor", Qt::ClosedHandCursor},
                     {"Qt.WhatsThisCursor", Qt::WhatsThisCursor},
                     {"Qt.BusyCursor", Qt::BusyCursor},
                     {"Qt.DragMoveCursor", Qt::DragMoveCursor},
                     {"Qt.DragCopyCursor", Qt::DragCopyCursor},
                     {"Qt.DragLinkCursor", Qt::DragLinkCursor}});

                if (name == "")
                    restoreOverrideCursor();
                else {
                    if (cursors.count(name))
                        QGuiApplication::setOverrideCursor(QCursor(cursors.at(name)));
                    else
                        QGuiApplication::setOverrideCursor(
                            QCursor(QPixmap(name).scaledToWidth(32, Qt::SmoothTransformation)));
                }
            }

            Q_INVOKABLE void restoreOverrideCursor() {
                QGuiApplication::restoreOverrideCursor();
            }

            Q_INVOKABLE [[nodiscard]] QString getUserName() const {
                return QStringFromStd(utility::get_user_name());
            }

            Q_INVOKABLE [[nodiscard]] QString pathFromURL(const QUrl &url) const {
                return url.path().replace("//", "/");
            }

            Q_INVOKABLE [[nodiscard]] QPersistentModelIndex
            makePersistent(const QModelIndex &index) const {
                return QPersistentModelIndex(index);
            }

            Q_INVOKABLE [[nodiscard]] QUrl QUrlFromQString(const QString &str) const {
                return QUrl(str);
            }

            Q_INVOKABLE [[nodiscard]] bool urlExists(const QUrl &url) const {
                auto path   = fs::path(utility::uri_to_posix_path(UriFromQUrl(url)));
                auto result = false;
                try {
                    result = fs::exists(path);
                } catch (...) {
                }
                return result;
            }

            Q_INVOKABLE [[nodiscard]] QString fileFromURL(const QUrl &url) const {
                static const std::regex as_hash_pad(R"(\{:0(\d+)d\})");
                auto tmp = utility::uri_to_posix_path(UriFromQUrl(url));
                tmp      = tmp.substr(tmp.find_last_of("/") + 1);
                // convert padding spec to #'s
                // {:04d}
                try {
                    tmp = fmt::format(
                        fmt::runtime(std::regex_replace(tmp, as_hash_pad, R"({:#<$1})")), "");
                } catch (...) {
                    tmp = std::regex_replace(tmp, as_hash_pad, "#");
                }

                return QStringFromStd(tmp);
            }

            Q_INVOKABLE [[nodiscard]] QString validMediaExtensions() const {
                std::string result;
                for (const auto &i : utility::supported_extensions) {
                    if (not result.empty())
                        result += " ";
                    result +=
                        std::string("*") + i + " " + std::string("*") + utility::to_lower(i);
                }
                return QStringFromStd(result);
            }

            Q_INVOKABLE [[nodiscard]] QString validTimelineExtensions() const {
                std::string result;
                for (const auto &i : utility::supported_timeline_extensions) {
                    if (not result.empty())
                        result += " ";
                    result +=
                        std::string("*") + i + " " + std::string("*") + utility::to_lower(i);
                }
                return QStringFromStd(result);
            }

            /*Q_INVOKABLE QQuickItem* findItemByName(QList<QObject*> nodes, const QString
            &name); Q_INVOKABLE QQuickItem* findItemByName(const QString& name) { return
            findItemByName(engine_->rootObjects(), name);
            }*/
            Q_INVOKABLE [[nodiscard]] QString
            getEnv(const QString &key, const QString &fallback = "") const {
                QString result = fallback;
                auto value     = utility::get_env(StdFromQString(key));
                if (value)
                    result = QStringFromStd(*value);
                return result;
            }

            Q_INVOKABLE [[nodiscard]] QString expandEnvVars(const QString &value) const {
                return QStringFromStd(xstudio::utility::expand_envvars(StdFromQString(value)));
            }

            Q_INVOKABLE [[nodiscard]] bool startDetachedProcess(
                const QString &program,
                const QStringList &arguments,
                const QString &workingDirectory = QString(),
                qint64 *pid                     = nullptr) const;

            Q_INVOKABLE [[nodiscard]] QString readFile(const QUrl &url) const;
            Q_INVOKABLE [[nodiscard]] QDateTime getFileMTime(const QUrl &url) const;
            Q_INVOKABLE [[nodiscard]] QVariant
            QVariantFromUuidString(const QString &uuid) const {
                return QVariant::fromValue(QUuidFromUuid(utility::Uuid(StdFromQString(uuid))));
            }

            Q_INVOKABLE [[nodiscard]] QUuid makeQUuid() const {
                return QUuidFromUuid(utility::Uuid::generate());
            }

            Q_INVOKABLE [[nodiscard]] QUuid QUuidFromUuidString(const QString &uuid) const {
                return QUuidFromUuid(utility::Uuid(StdFromQString(uuid)));
            }

            Q_INVOKABLE [[nodiscard]] QString QUuidToQString(const QUuid &uuid) const {
                return uuid.toString(QUuid::WithoutBraces);
            }

            Q_INVOKABLE [[nodiscard]] QItemSelection createItemSelection() const {
                return QItemSelection();
            }
            Q_INVOKABLE [[nodiscard]] QItemSelection
            createItemSelection(const QModelIndex &tl, const QModelIndex &br) const {
                return QItemSelection(tl, br);
            }

            Q_INVOKABLE [[nodiscard]] QModelIndexList
            createListFromRange(const QItemSelectionRange &r) const {
                return r.indexes();
            }

            Q_INVOKABLE [[nodiscard]] QItemSelection
            createItemSelection(const QModelIndexList &l) const {
                auto s = QItemSelection();
                for (const auto &i : l)
                    s.select(i, i);
                return s;
            }

            Q_INVOKABLE [[nodiscard]] QItemSelection
            intersectItemSelection(const QItemSelection &a, const QItemSelection &b) const {
                auto s = QItemSelection();
                for (const auto &i : a.indexes()) {
                    if (b.contains(i))
                        s.select(i, i);
                }
                return s;
            }

            Q_INVOKABLE [[nodiscard]] QItemSelection
            createItemSelectionFromList(const QVariantList &l) const {
                auto s = QItemSelection();
                for (const auto &i : l)
                    s.select(i.toModelIndex(), i.toModelIndex());
                return s;
            }

            Q_INVOKABLE [[nodiscard]] QModelIndexList
            getParentIndexes(const QModelIndexList &l) const;

            Q_INVOKABLE [[nodiscard]] QModelIndexList
            getParentIndexesFromRange(const QItemSelection &l) const;

            Q_INVOKABLE [[nodiscard]] bool itemSelectionContains(
                const QItemSelection &selection, const QModelIndex &item) const {
                return selection.contains(item);
            }

            Q_INVOKABLE [[nodiscard]] QColor
            saturate(const QColor &color, const double factor = 1.5) const {
                double h, s, l, a;
                color.getHslF(&h, &s, &l, &a);
                s = std::max(0.0, std::min(1.0, s * factor));
                return QColor::fromHslF(h, s, l, a);
            }

            Q_INVOKABLE [[nodiscard]] QColor
            luminate(const QColor &color, const double factor = 1.5) const {
                double h, s, l, a;
                color.getHslF(&h, &s, &l, &a);
                l = std::max(0.0, std::min(1.0, l * factor));
                return QColor::fromHslF(h, s, l, a);
            }

            Q_INVOKABLE [[nodiscard]] QColor
            alphate(const QColor &color, const double alpha) const {
                auto result = color;
                result.setAlphaF(std::max(0.0, std::min(1.0, alpha)));
                return result;
            }

            Q_INVOKABLE [[nodiscard]] QColor saturateLuminate(
                const QColor &color,
                const double sfactor = 1.0,
                const double lfactor = 1.0) const {
                double h, s, l, a;
                color.getHslF(&h, &s, &l, &a);
                s = std::max(0.0, std::min(1.0, s * sfactor));
                l = std::max(0.0, std::min(1.0, l * lfactor));
                return QColor::fromHslF(h, s, l, a);
            }

            Q_INVOKABLE [[nodiscard]] QObject *contextPanel(QObject *obj) const;

            Q_INVOKABLE [[nodiscard]] QString contextPanelAddress(QObject *obj) const {
                return objPtrTostring(contextPanel(obj));
            }

            Q_INVOKABLE void setMenuPathPosition(
                const QString &menu_path, const QString &menu_name, const float position) const;

            static inline QString objPtrTostring(QObject *obj) {
                if (!obj)
                    return QString();
                return QString("0x%1").arg(
                    reinterpret_cast<qlonglong>(obj), QT_POINTER_SIZE * 2, 16, QChar('0'));
            }

            Q_INVOKABLE void inhibitScreenSaver(const bool inhibit = true) const;

          private:
            QQmlEngine *engine_;
        };

        class HELPER_QML_EXPORT KeyEventsItem : public QQuickItem {
            Q_OBJECT

            Q_PROPERTY(QString context READ context WRITE setContext NOTIFY contextChanged)

          public:
            explicit KeyEventsItem(QQuickItem *parent = nullptr);

            [[nodiscard]] QString context() const { return QStringFromStd(context_); }

            void setContext(const QString &context) {
                if (context_ != StdFromQString(context)) {
                    context_ = StdFromQString(context);
                    emit contextChanged();
                }
            }
          signals:
            void contextChanged();

          protected:
            bool event(QEvent *event) override;
            void keyPressEvent(QKeyEvent *event) override;
            void keyReleaseEvent(QKeyEvent *event) override;

          private:
            std::string context_;
            caf::actor keypress_monitor_;
            std::string window_name_;
        };


        class HELPER_QML_EXPORT QMLUuid : public QObject {
            Q_OBJECT
            Q_PROPERTY(QString asString READ asString WRITE setFromString NOTIFY changed)
            Q_PROPERTY(QUuid asQuuid READ asQuuid WRITE setFromQuuid NOTIFY changed)
            Q_PROPERTY(bool isNull READ isNull NOTIFY changed)

          public:
            explicit QMLUuid(QObject *parent = nullptr) : QObject(parent), uuid_() {}

            QMLUuid(const QString &uuid, QObject *parent = nullptr)
                : QObject(parent), uuid_(StdFromQString(uuid)) {}

            Q_INVOKABLE QMLUuid(const QUuid &uuid, QObject *parent = nullptr)
                : QObject(parent), uuid_(UuidFromQUuid(uuid)) {}

            QMLUuid(const utility::Uuid &uuid, QObject *parent = nullptr)
                : QObject(parent), uuid_(uuid) {}


            ~QMLUuid() override = default;

            [[nodiscard]] QString asString() const { return QStringFromStd(to_string(uuid_)); }
            [[nodiscard]] QUuid asQuuid() const { return QUuidFromUuid(uuid_); }
            [[nodiscard]] bool isNull() const { return uuid_.is_null(); }

          signals:
            void changed();

          public slots:
            void setFromString(const QString &other) {
                uuid_.from_string(StdFromQString(other));
                emit changed();
            }
            void setFromQuuid(const QUuid &other) {
                uuid_ = UuidFromQUuid(other);
                emit changed();
            }

          private:
            utility::Uuid uuid_;
        };

        class HELPER_QML_EXPORT SemVer : public QObject {
            Q_OBJECT
            Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
            Q_PROPERTY(uint major READ major WRITE setMajor NOTIFY versionChanged)
            Q_PROPERTY(uint minor READ minor WRITE setMinor NOTIFY versionChanged)
            Q_PROPERTY(uint patch READ patch WRITE setPatch NOTIFY versionChanged)

          public:
            SemVer(QObject *parent = nullptr) : QObject(parent) {}
            SemVer(const QString &version, QObject *parent = nullptr);

            [[nodiscard]] QString version() const {
                return QStringFromStd(version_.to_string());
            }
            void setVersion(const QString &version);

            uint major() const { return version_.major; }
            void setMajor(const uint &major) {
                version_.major = major;
                emit versionChanged();
            }

            uint minor() const { return version_.minor; }
            void setMinor(const uint &minor) {
                version_.minor = minor;
                emit versionChanged();
            }

            [[nodiscard]] uint patch() const { return version_.patch; }
            void setPatch(const uint &patch) {
                version_.patch = patch;
                emit versionChanged();
            }


          signals:
            void versionChanged();

          public slots:
            [[nodiscard]] int compare(const QString &other) const;
            [[nodiscard]] int compare(const SemVer &other) const;

          private:
            semver::version version_;
        };

        class HELPER_QML_EXPORT ClipboardProxy : public QObject {
            Q_OBJECT
            Q_PROPERTY(QString text READ dataText WRITE setDataText NOTIFY dataChanged)
            Q_PROPERTY(QString selectionText READ selectionText WRITE setSelectionText NOTIFY
                           selectionChanged)
            Q_PROPERTY(QVariant data READ data WRITE setData NOTIFY dataChanged)
          public:
            explicit ClipboardProxy(QObject *parent = nullptr);

            void setDataText(const QString &text);
            [[nodiscard]] QString dataText() const;

            void setData(const QVariant &data);
            [[nodiscard]] QVariant data() const;

            void setSelectionText(const QString &text);
            [[nodiscard]] QString selectionText() const;

          signals:
            void dataChanged();
            void selectionChanged();
        };

        class HELPER_QML_EXPORT Plugin : public QObject {
            Q_OBJECT
            Q_PROPERTY(QString qmlName READ qmlName NOTIFY qmlNameChanged)
            Q_PROPERTY(
                QString qmlWidgetString READ qmlWidgetString NOTIFY qmlWidgetStringChanged)
            Q_PROPERTY(QString qmlMenuString READ qmlMenuString NOTIFY qmlMenuStringChanged)
            Q_PROPERTY(QUuid uuid READ uuid NOTIFY uuidChanged)
            Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
            Q_PROPERTY(QString backendId READ backendId NOTIFY backendIdChanged)

          public:
            explicit Plugin(QObject *parent = nullptr) : QObject(parent) {}

            [[nodiscard]] QString qmlName() const { return qml_name_; }
            void setQmlName(const QString &qml_name) {
                if (qml_name != qml_name_) {
                    qml_name_ = qml_name;
                    emit qmlNameChanged();
                }
            }

            [[nodiscard]] QString qmlWidgetString() const { return qml_widget_string_; }
            void setQmlWidgetString(const QString &qml_widget_string) {
                if (qml_widget_string != qml_widget_string_) {
                    qml_widget_string_ = qml_widget_string;
                    emit qmlWidgetStringChanged();
                }
            }

            [[nodiscard]] QString qmlMenuString() const { return qml_menu_string_; }
            void setQmlMenuString(const QString &qml_menu_string) {
                if (qml_menu_string_ != qml_menu_string) {
                    qml_menu_string_ = qml_menu_string;
                    emit qmlMenuStringChanged();
                }
            }

            [[nodiscard]] QUuid uuid() const { return uuid_; }
            void setUuid(const QUuid &uuid) {
                if (uuid != uuid_) {
                    uuid_ = uuid;
                    emit uuidChanged();
                }
            }

            [[nodiscard]] QString backend() const {
                return QStringFromStd(to_string(backend_));
            }
            [[nodiscard]] QString backendId() const {
                return QStringFromStd(std::to_string(backend_id_));
            }
            void setBackend(caf::actor backend) {
                if (backend != backend_) {
                    backend_    = std::move(backend);
                    backend_id_ = backend_.id();
                    emit backendIdChanged();
                    emit backendChanged();
                }
            }

          signals:
            void qmlNameChanged();
            void qmlWidgetStringChanged();
            void qmlMenuStringChanged();
            void uuidChanged();
            void backendChanged();
            void backendIdChanged();

          private:
            QString qml_widget_string_;
            QString qml_menu_string_;
            QString qml_name_;
            QUuid uuid_;
            caf::actor backend_;
            caf::actor_id backend_id_;
        };

        class HELPER_QML_EXPORT ImagePainter : public QQuickPaintedItem {

            Q_OBJECT

            Q_PROPERTY(QVariant image READ image WRITE setImage NOTIFY imageChanged)
            Q_PROPERTY(bool fill READ fill WRITE setFill NOTIFY fillChanged)

          public:
            ImagePainter(QQuickItem *parent = nullptr);
            ~ImagePainter() override = default;

            [[nodiscard]] const QVariant &image() const { return image_property_; }
            [[nodiscard]] bool fill() const { return fill_; }

            void setImage(QVariant &image) {
                image_property_ = image;
                if (image_property_.canConvert<QImage>()) {
                    image_ = image_property_.value<QImage>();
                } else {
                    image_ = QImage();
                }
                emit imageChanged();
                update();
            }

            void setFill(const bool fill) {
                if (fill != fill_) {
                    fill_ = fill;
                    emit fillChanged();
                    update();
                }
            }

            void paint(QPainter *painter) override;

          signals:

            void imageChanged();
            void fillChanged();

          private:
            QVariant image_property_;
            QImage image_;
            bool fill_ = {false};
        };


        class HELPER_QML_EXPORT MarkerModel : public JSONTreeModel {
            Q_OBJECT

            Q_PROPERTY(QVariant markerData READ markerData WRITE setMarkerData NOTIFY
                           markerDataChanged)

          public:
            enum Roles {
                commentRole = JSONTreeModel::Roles::LASTROLE,
                durationRole,
                flagRole,
                layerRole,
                nameRole,
                rateRole,
                startRole
            };
            explicit MarkerModel(QObject *parent = nullptr);

            Q_INVOKABLE bool setMarkerData(const QVariant &data);

            Q_INVOKABLE QModelIndex addMarker(
                const int frame,
                const double rate,
                const QString &name      = "Marker",
                const QString &flag      = "#FFFF0000",
                const QVariant &metadata = mapFromValue(R"({})"_json));

            [[nodiscard]] QVariant markerData() const;

            QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

            bool setData(
                const QModelIndex &index,
                const QVariant &value,
                int role = Qt::EditRole) override;

          signals:
            void markerDataChanged();


          private:
        };

        /* Surprisingly, QML does not provide any facility to bind to a named
        property, where you name the property value as a string.
        This class will allow us to do this.*/
        class HELPER_QML_EXPORT PropertyFollower : public QObject {
            Q_OBJECT

            Q_PROPERTY(QVariant propertyValue READ propertyValue NOTIFY propertyValueChanged)
            Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged)
            Q_PROPERTY(QString propertyName READ propertyName WRITE setPropertyName NOTIFY
                           propertyNameChanged)

          public:
            PropertyFollower(QObject *parent = nullptr) : QObject(parent) {}

            Q_INVOKABLE void setTarget(QObject *target);

            Q_INVOKABLE void setPropertyName(const QString propertyName);

            QVariant propertyValue() { return the_property_.read(); }

            QString propertyName() { return property_name_; }

            QObject *target() { return target_; }

          signals:

            void propertyValueChanged();
            void targetChanged();
            void propertyNameChanged();

          private:
            QQmlProperty the_property_;
            QString property_name_;
            QObject *target_ = {nullptr};
        };


    } // namespace qml
} // namespace ui
} // namespace xstudio
