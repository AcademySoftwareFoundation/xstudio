// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/web_bridge_ui.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::ui::qml;

WebBridgeUI::WebBridgeUI(QObject *parent) : QObject(parent) {}

QString WebBridgeUI::ping() const {
    spdlog::debug("WebBridgeUI::ping");
    return QStringLiteral("pong");
}
