// SPDX-License-Identifier: Apache-2.0
#pragma once


#ifndef EVENT_QML_EXPORT_H
#define EVENT_QML_EXPORT_H

#ifdef EVENT_QML_STATIC_DEFINE
#  define EVENT_QML_EXPORT
#  define EVENT_QML_NO_EXPORT
#else
#  ifndef EVENT_QML_EXPORT
#    ifdef event_qml_EXPORTS
        /* We are building this library */
#      define EVENT_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define EVENT_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef EVENT_QML_NO_EXPORT
#    define EVENT_QML_NO_EXPORT 
#  endif
#endif

#ifndef EVENT_QML_DEPRECATED
#  define EVENT_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef EVENT_QML_DEPRECATED_EXPORT
#  define EVENT_QML_DEPRECATED_EXPORT EVENT_QML_EXPORT EVENT_QML_DEPRECATED
#endif

#ifndef EVENT_QML_DEPRECATED_NO_EXPORT
#  define EVENT_QML_DEPRECATED_NO_EXPORT EVENT_QML_NO_EXPORT EVENT_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef EVENT_QML_NO_DEPRECATED
#    define EVENT_QML_NO_DEPRECATED
#  endif
#endif

#endif /* EVENT_QML_EXPORT_H */

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QAbstractListModel>
#include <QUrl>
#include <QAbstractItemModel>
#include <QQmlPropertyMap>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/event/event.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class EVENT_QML_EXPORT EventUI : public QObject {
            Q_OBJECT

            Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
            Q_PROPERTY(int progressMinimum READ progressMinimum NOTIFY progressMinimumChanged)
            Q_PROPERTY(int progressMaximum READ progressMaximum NOTIFY progressMaximumChanged)
            Q_PROPERTY(float progressPercentage READ progressPercentage NOTIFY
                           progressPercentageChanged)
            Q_PROPERTY(bool complete READ complete NOTIFY completeChanged)
            Q_PROPERTY(QUuid event READ event NOTIFY eventChanged)

            Q_PROPERTY(QString text READ text NOTIFY textChanged)
            Q_PROPERTY(QString textRange READ textRange NOTIFY textRangeChanged)
            Q_PROPERTY(QString textPercentage READ textPercentage NOTIFY textPercentageChanged)

          public:
            EventUI(const event::Event &event, QObject *parent = nullptr);
            ~EventUI() override = default;

            void update(const event::Event &event);

            [[nodiscard]] int progress() const { return event_.progress(); }
            [[nodiscard]] int progressMinimum() const { return event_.progress_minimum(); }
            [[nodiscard]] int progressMaximum() const { return event_.progress_maximum(); }
            [[nodiscard]] float progressPercentage() const {
                return event_.progress_percentage();
            }
            [[nodiscard]] bool complete() const { return event_.complete(); }
            [[nodiscard]] QUuid event() const { return QUuidFromUuid(event_.event()); }

            [[nodiscard]] QString text() const {
                return QStringFromStd(event_.progress_text());
            }
            [[nodiscard]] QString textRange() const {
                return QStringFromStd(event_.progress_text_range());
            }
            [[nodiscard]] QString textPercentage() const {
                return QStringFromStd(event_.progress_text_percentage());
            }

          signals:
            void progressChanged();
            void progressMinimumChanged();
            void progressMaximumChanged();
            void progressPercentageChanged();
            void completeChanged();
            void eventChanged();
            void textChanged();
            void textRangeChanged();
            void textPercentageChanged();

          private:
            event::Event event_;
        };

        class EVENT_QML_EXPORT EventAttrs : public QQmlPropertyMap {

            Q_OBJECT

          public:
            EventAttrs(QObject *parent = nullptr);
            ~EventAttrs() override = default;
            void addEvent(const event::Event &);
        };

        class EVENT_QML_EXPORT EventManagerUI : public QMLActor {

            Q_OBJECT

          public:
            EventManagerUI(EventAttrs *attrs_map);
            ~EventManagerUI() override = default;

            void init(caf::actor_system &system) override;

          private:
            EventAttrs *attrs_map_ = {nullptr};
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio