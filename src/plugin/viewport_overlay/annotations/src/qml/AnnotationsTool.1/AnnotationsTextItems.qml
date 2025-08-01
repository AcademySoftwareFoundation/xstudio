// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QuickFuture 1.0
import QuickPromise 1.0
import xStudio 1.0
import xstudio.qml.module 1.0

Rectangle {

    id: control
    color: "transparent"
    width: viewport.width
    height: viewport.height

    property var imageBox: viewport.imageBoundaryInViewport

    XsModuleAttributesModel {
        
        id: text_times
        attributesGroupNames: "annotations_text_items_0"

    }

    Repeater {

        id: the_view
        anchors.fill: parent
        model: text_times
        focus: true

        delegate: Item {

            id: parent_item
            anchors.centerIn: parent
            width: 200
            height: 200
            property var dynamic_widget

            Text {
                anchors.fill: parent
                text: value
                font.pixelSize: 50
                font.family: XsStyle.menuFontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: attr_colour
                onColorChanged: {
                    console.log("Attr color", color)
                }
    
            }
        }
    }
}
