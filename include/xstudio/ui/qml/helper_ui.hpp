// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <functional>
#include <semver.hpp>

#include "xstudio/ui/qml/actor_object.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

CAF_PUSH_WARNINGS
#include <QCursor>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQmlPropertyMap>
#include <QString>
#include <QItemSelection>
#include <QUrl>
#include <QUuid>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QQmlPropertyMap>

CAF_POP_WARNINGS

namespace xstudio {
namespace ui {
    namespace qml {
        using namespace caf;


        QVariant mapFromValue(const nlohmann::json &value);
        nlohmann::json mapFromValue(const QVariant &value);

        class ModelRowCount : public QObject {
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

        class ModelProperty : public QObject {
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

          private:
            bool updateValue();
            [[nodiscard]] int getRoleId(const QString &role) const;

            QPersistentModelIndex index_;
            QString role_;
            int role_id_{-1};
            QVariant value_;
        };

        class ModelPropertyTree : public JSONTreeModel {
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


        class ModelPropertyMap : public QObject {
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

          protected slots:
            void dataChanged(
                const QModelIndex &topLeft,
                const QModelIndex &bottomRight,
                const QVector<int> &roles = QVector<int>());
            void valueChangedSlot(const QString &key, const QVariant &value);

          protected:
            virtual void valueChanged(const QString &key, const QVariant &value);
            virtual void updateValues(const QVector<int> &roles = {});
            [[nodiscard]] int getRoleId(const QString &role) const;

            QPersistentModelIndex index_;
            QQmlPropertyMap *values_{nullptr};
        };

        class ModelNestedPropertyMap : public ModelPropertyMap {
            Q_OBJECT

          public:
            explicit ModelNestedPropertyMap(QObject *parent = nullptr)
                : ModelPropertyMap(parent) {}


          protected:
            void updateValues(const QVector<int> &roles = {}) override;
            void valueChanged(const QString &key, const QVariant &value) override;

            QString data_role_    = {"valueRole"};
            QString default_role_ = {"defaultValueRole"};
        };

        class CafSystemObject : public QObject {

            Q_OBJECT

          public:
            CafSystemObject(QObject *parent, caf::actor_system &sys);

            ~CafSystemObject() override = default;

            caf::actor_system &actor_system() { return system_ref_.get(); }

            static caf::actor_system &get_actor_system();

          private:
            std::reference_wrapper<caf::actor_system> system_ref_;
        };

        class QMLActor : public caf::mixin::actor_object<QObject> {
            Q_OBJECT

          public:
            using super = caf::mixin::actor_object<QObject>;
            explicit QMLActor(QObject *parent = nullptr);

            virtual ~QMLActor();
            virtual void init(caf::actor_system &system) { super::init(system); }

          public:
            caf::actor_system &system() { return self()->home_system(); }
        };

        inline QString QStringFromStd(const std::string &str) {
            return QString::fromUtf8(str.c_str());
        }
        inline std::string StdFromQString(const QString &str) {
            return str.toUtf8().constData();
        }

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
                jsn[i.key()] = utility::resplit(i.value(), rgx);
            }
            return jsn;
        }

        class Helpers : public QObject {
            Q_OBJECT

          public:
            Helpers(QQmlEngine *engine, QObject *parent = nullptr)
                : QObject(parent), engine_(engine) {}
            ~Helpers() override = default;

            Q_INVOKABLE [[nodiscard]] bool openURL(const QUrl &url) const {
                return QDesktopServices::openUrl(url);
            }
            Q_INVOKABLE [[nodiscard]] QString ShowURIS(const QList<QUrl> &urls) const {
                std::vector<caf::uri> uris;
                for (const auto &i : urls)
                    uris.emplace_back(UriFromQUrl(i));
                return QStringFromStd(utility::filemanager_show_uris(uris));
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


            Q_INVOKABLE [[nodiscard]] QString fileFromURL(const QUrl &url) const {
                static const std::regex as_hash_pad(R"(\{:0(\d+)d\})");
                auto tmp = utility::uri_to_posix_path(UriFromQUrl(url));
                tmp      = tmp.substr(tmp.find_last_of("/") + 1);
                // convert padding spec to #'s
                // {:04d}
                try {
                    tmp = fmt::format(std::regex_replace(tmp, as_hash_pad, R"({:#<$1})"), "");
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
            Q_INVOKABLE [[nodiscard]] [[nodiscard]] QString
            getEnv(const QString &key, const QString &fallback = "") const {
                QString result = fallback;
                auto value     = utility::get_env(StdFromQString(key));
                if (value)
                    result = QStringFromStd(*value);
                return result;
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
            Q_INVOKABLE [[nodiscard]] QItemSelection
            createItemSelection(const QModelIndexList &l) const {
                auto s = QItemSelection();
                for (const auto &i : l)
                    s.select(i, i);
                return s;
            }
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

          private:
            QQmlEngine *engine_;
        };

        class CursorPosProvider : public QObject {
            Q_OBJECT

          public:
            CursorPosProvider(QObject *parent = nullptr) : QObject(parent) {}
            ~CursorPosProvider() override = default;

            Q_INVOKABLE QPointF cursorPos() { return QCursor::pos(); }
        };

        class QMLUuid : public QObject {
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

        class SemVer : public QObject {
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

        class ClipboardProxy : public QObject {
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

        class Plugin : public QObject {
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

    } // namespace qml
} // namespace ui
} // namespace xstudio
