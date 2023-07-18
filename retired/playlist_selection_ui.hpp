// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QPoint>
#include <QUrl>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/media_ui.hpp"
#include "xstudio/utility/frame_time.hpp"

namespace xstudio {
namespace utility {
    class MediaReference;
}
namespace ui {
    namespace qml {

        class PlaylistSelectionUI : public QMLActor {

            Q_OBJECT

            Q_PROPERTY(
                QList<QUuid> selectedMediaUuids READ selectedMedia NOTIFY selectionChanged)

          public:
            explicit PlaylistSelectionUI(QObject *parent = nullptr);
            ~PlaylistSelectionUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            caf::actor backend() { return backend_; }

            [[nodiscard]] const QList<QUuid> &selectedMedia() const { return selected_media_; }

          signals:

            void selectionChanged();

          public slots:

            void initSystem(QObject *system_qobject);
            void newSelection(const QVariantList &selection);
            [[nodiscard]] QVariantList currentSelection() const;
            void deleteSelected();
            void gatherSourcesForSelected();
            void evictSelected();
            void moveSelectionByIndex(const int index);

            QStringList selectedMediaFilePaths();
            QStringList selectedMediaFileNames();
            QList<QUrl> selectedMediaURLs();

          private:
            std::vector<utility::MediaReference> get_selected_media_refences();

          private:
            caf::actor backend_;
            caf::actor backend_events_;

            QList<QUuid> selected_media_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio