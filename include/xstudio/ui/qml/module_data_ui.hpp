// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/ui/qml/model_data_ui.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace caf;

class HELPER_QML_EXPORT ModulesModelData : public UIModelData {

    Q_OBJECT

  public:
    explicit ModulesModelData(QObject *parent = nullptr);
};

} // namespace xstudio::ui::qml