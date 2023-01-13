// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QUrl>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        //  top level utility actor, for stuff that lives out side the session.
        class StudioUI : public QMLActor {

            Q_OBJECT

          public:
            explicit StudioUI(caf::actor_system &system, QObject *parent = nullptr);
            ~StudioUI() override = default;

            Q_INVOKABLE bool clearImageCache();

          signals:

          public slots:

          private:
            void init(caf::actor_system &system) override;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio