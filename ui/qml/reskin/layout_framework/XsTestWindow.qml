// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 1.4

Window {
    id: thisWindow
    visible: true
    color: "transparent"
    title: "Popup Window"
    width: 300
    height: 300

    property var windowSource: undefined
    property var the_widget: undefined

    onWindowSourceChanged: {
        
        if (windowSource != undefined) {
            var component = Qt.createComponent(windowSource);
            if (component.status == Component.Ready) {
                the_widget = component.createObject(thisWindow, { width: thisWindow.width, height: thisWindow.height});

            }
        }
    }

}

