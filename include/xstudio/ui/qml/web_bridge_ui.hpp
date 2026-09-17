// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/studio_qml_export.h"

namespace xstudio::ui::qml {

// Native object exposed to pages loaded in the web browser panel, over
// QWebChannel. Registered as the "xstudioWebBridge" context property when
// built with BUILD_WEBENGINE. Currently a stub that only proves the channel
// works end to end; the real API (load media, build timelines) comes later.
class STUDIO_QML_EXPORT WebBridgeUI : public QObject {

    Q_OBJECT

  public:
    explicit WebBridgeUI(QObject *parent = nullptr);
    ~WebBridgeUI() override = default;

    // Round-trip check: page JS calls ping() and gets "pong" back.
    Q_INVOKABLE [[nodiscard]] QString ping() const;
};

} // namespace xstudio::ui::qml
