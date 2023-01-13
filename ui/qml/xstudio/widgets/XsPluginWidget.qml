// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick 2.14

import xStudio 1.0

Item {
    id: container

    property string qmlWidgetString
    property string qmlMenuString
    property string qmlName
    property string backendId

    property var plugin_ui

    function loadUI() {
        if(qmlWidgetString) {
    		plugin_ui = Qt.createQmlObject(qmlWidgetString,
                                       container,
                                       qmlName);
        }
    }

    Component.onCompleted: loadUI()
}
