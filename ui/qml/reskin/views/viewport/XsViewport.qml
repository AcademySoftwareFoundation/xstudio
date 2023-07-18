import QtQuick 2.12

import xstudio.qml.viewport 1.0


Viewport {

    id: viewport
    anchors.fill: parent
    focus: true
    property var mainWindow: appWindow
    onMainWindowChanged: {
        appWindow.viewport = viewport
    }

    onFocusChanged: {
        console.log("focus", focus)
    }

    onActiveFocusChanged: {
        console.log("focus", activeFocus)
    }

}