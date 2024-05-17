// SPDX-License-Identifier: Apache-2.0
#pragma once
#pragma once

#pragma once

#ifndef TAG_QML_EXPORT_H
#define TAG_QML_EXPORT_H

#ifdef TAG_QML_STATIC_DEFINE
#define TAG_QML_EXPORT
#define TAG_QML_NO_EXPORT
#else
#ifndef TAG_QML_EXPORT
#ifdef tag_qml_EXPORTS
/* We are building this library */
#define TAG_QML_EXPORT __declspec(dllexport)
#else
/* We are using this library */
#define TAG_QML_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef TAG_QML_NO_EXPORT
#define TAG_QML_NO_EXPORT
#endif
#endif

#ifndef TAG_QML_DEPRECATED
#define TAG_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef TAG_QML_DEPRECATED_EXPORT
#define TAG_QML_DEPRECATED_EXPORT TAG_QML_EXPORT TAG_QML_DEPRECATED
#endif

#ifndef TAG_QML_DEPRECATED_NO_EXPORT
#define TAG_QML_DEPRECATED_NO_EXPORT TAG_QML_NO_EXPORT TAG_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef TAG_QML_NO_DEPRECATED
#define TAG_QML_NO_DEPRECATED
#endif
#endif

#endif /* TAG_QML_EXPORT_H */

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QAbstractListModel>
#include <QUrl>
#include <QAbstractItemModel>
#include <QQmlPropertyMap>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/tag/tag.hpp"

namespace xstudio {
namespace ui {
    namespace qml {


        class TagUI : public QObject {
            Q_OBJECT

            Q_PROPERTY(QUuid id READ id NOTIFY idChanged)
            Q_PROPERTY(QUuid link READ link NOTIFY linkChanged)
            Q_PROPERTY(QString type READ type NOTIFY typeChanged)
            Q_PROPERTY(QString data READ data NOTIFY dataChanged)
            Q_PROPERTY(bool persistent READ persistent NOTIFY persistentChanged)
            Q_PROPERTY(int unique READ unique NOTIFY uniqueChanged)

          public:
            TagUI(const tag::Tag tag, QObject *parent = nullptr);
            ~TagUI() override = default;

            [[nodiscard]] QUuid id() const { return QUuidFromUuid(tag_.id()); }
            [[nodiscard]] QUuid link() const { return QUuidFromUuid(tag_.link()); }
            [[nodiscard]] QString data() const { return QStringFromStd(tag_.data()); }
            [[nodiscard]] QString type() const { return QStringFromStd(tag_.type()); }
            [[nodiscard]] int unique() const { return static_cast<int>(tag_.unique()); }
            [[nodiscard]] bool persistent() const { return tag_.persistent(); }

          signals:
            void idChanged();
            void linkChanged();
            void typeChanged();
            void dataChanged();
            void uniqueChanged();
            void persistentChanged();

          private:
            tag::Tag tag_;
        };

        class TagListUI : public QObject {
            Q_OBJECT

            Q_PROPERTY(QList<QObject *> tags READ tags NOTIFY tagsChanged)

          public:
            TagListUI(QObject *parent = nullptr) : QObject(parent) {}
            ~TagListUI() override = default;

            [[nodiscard]] QList<QObject *> tags() const { return tags_; }

            void addTag(TagUI *tag);
            bool removeTag(const QUuid &id);

          signals:
            void tagsChanged();

          public:
            QList<QObject *> tags_;
        };

        class TagAttrs : public QQmlPropertyMap {

            Q_OBJECT

          public:
            TagAttrs(QObject *parent = nullptr);
            ~TagAttrs() override = default;

            void reset();
        };

        class TAG_QML_EXPORT TagManagerUI : public QMLActor {

            Q_OBJECT

          public:
            TagManagerUI(QObject *parent = nullptr);
            ~TagManagerUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            void add_tag(const tag::Tag &tag);
            void remove_tag(const utility::Uuid &id);

            TagAttrs *attrs_map_ = {nullptr};
            caf::actor backend_;
            caf::actor backend_events_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio