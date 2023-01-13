// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick 2.14
import xStudio 1.0

Item {
    id: container

    property string widgetString
    property var widget_ui
    // implicitHeight: widget_ui.height

    function loadUI() {
		widget_ui = Qt.createQmlObject(widgetString,
                                   container,
                                   "test");
        // widget_ui.implicitWidth = container.width
    }

    Component.onCompleted: loadUI()
}
