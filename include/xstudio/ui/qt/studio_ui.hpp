// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/mixin/actor_widget.hpp>

CAF_PUSH_WARNINGS
#include <QWidget>
CAF_POP_WARNINGS

namespace xstudio {
namespace ui {
    namespace qt {
        class StudioUI : public caf::mixin::actor_widget<QWidget> {

            Q_OBJECT
            using super = caf::mixin::actor_widget<QWidget>;

          public:
            StudioUI(QWidget *parent, Qt::WindowFlags f = 0);
            virtual ~StudioUI();

            void init(caf::actor_system &system, caf::actor session);
        };
    } // namespace qt
} // namespace ui
} // namespace xstudio
