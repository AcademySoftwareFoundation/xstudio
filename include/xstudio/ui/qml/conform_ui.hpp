// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/conform_qml_export.h"

CAF_PUSH_WARNINGS
#include <QFuture>
#include <QList>
#include <QUuid>
#include <QtConcurrent>
#include <QVariantList>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class CONFORM_QML_EXPORT ConformEngineUI : public caf::mixin::actor_object<JSONTreeModel> {
            Q_OBJECT

          public:
            using super = caf::mixin::actor_object<JSONTreeModel>;
            enum Roles { nameRole = JSONTreeModel::Roles::LASTROLE };

            ConformEngineUI(QObject *parent = nullptr);
            ~ConformEngineUI() override = default;

            void init(caf::actor_system &system);
            caf::actor_system &system() const {
                return const_cast<caf::actor_companion *>(self())->home_system();
            }

            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;


            Q_INVOKABLE QFuture<QList<QUuid>> conformItemsFuture(
                const QString &task,
                const QModelIndex &container,
                const QModelIndex &item,
                const bool fanOut       = false,
                const bool removeSource = false) const;

            Q_INVOKABLE QFuture<QList<QUuid>> conformToSequenceFuture(
                const QModelIndex &playlistIndex,
                const QModelIndexList &mediaIndexes,
                const QModelIndex &sequenceIndex,
                const QModelIndex &conformTrackIndex,
                const bool replace,
                const QString &newTrackName = "") const;

            Q_INVOKABLE QFuture<QList<QUuid>> conformToNewSequenceFuture(
                const QModelIndexList &mediaIndexes,
                const QString &task              = "",
                const QModelIndex &playlistIndex = QModelIndex()) const;

            Q_INVOKABLE QFuture<bool> conformPrepareSequenceFuture(
                const QModelIndex &sequenceIndex, const bool onlyCreateConfrom = true) const;

            Q_INVOKABLE QFuture<QList<QUuid>> conformTracksToSequenceFuture(
                const QModelIndexList &trackIndexes, const QModelIndex &sequenceIndex) const;

            Q_INVOKABLE QFuture<QList<QUuid>> conformFindRelatedFuture(
                const QString &key,
                const QModelIndex &clipIndex,
                const QModelIndex &sequenceIndex) const;

          signals:

          private:
            QFuture<QList<QUuid>> conformItemsFuture(
                const std::string &task,
                const utility::UuidActorVector &items,
                const utility::UuidActor &playlist,
                const utility::UuidActor &container,
                const std::string &item_type,
                const utility::UuidVector &before,
                const bool removeSource,
                const QPersistentModelIndex &notifyIndex = QPersistentModelIndex(),
                const QUuid &notifyUuid                  = QUuid()) const;

            utility::Uuid conform_uuid_;
            caf::actor conform_events_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio