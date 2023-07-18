// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QuickFuture 1.0
import QuickPromise 1.0
import xStudio 1.0
import xstudio.qml.module 1.0

Rectangle {

    width: layout.width+10
    height: layout.height+10
    color: "transparent"

    id: control
    visible: pixel_info_string != ""//pixel_info_attributes.enabled ? pixel_info_attributes.enabled : false

    XsModuleAttributes {

        id: pixel_info_attributes
        attributesGroupNames: "pixel_info_attributes"
    }

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: "black"
        opacity: bg_opacity
        border.color: "white"
        border.width: 2        
    }

    // Properties on the XsModuleAttributes items are created at runtime after
    // this item is 'completed' and therefore we need to map to local variables
    // with checks as follows:
    property var pixel_info_string: pixel_info_attributes.pixel_info ? pixel_info_attributes.pixel_info : ""
    property var pixel_info_title: pixel_info_attributes.pixel_info_title ? pixel_info_attributes.pixel_info_title : ""
    property var font_size: pixel_info_attributes.font_size ? pixel_info_attributes.font_size : 8.0
    property var font_opacity: pixel_info_attributes.font_opacity ? pixel_info_attributes.font_opacity : 0.8
    property var bg_opacity: pixel_info_attributes.bg_opacity ? pixel_info_attributes.bg_opacity : 0.4

    Column {

        id: layout
        x: 5
        y: 5
        spacing: 4

        Text {
            id: title
            text: pixel_info_title
            color: "white"
            font.pixelSize: font_size
            font.family: XsStyle.fixWidthFontFamily
            opacity: font_opacity 
        }

        Rectangle {
            width: the_text.width
            height: 1
            opacity: bg_opacity
        }

        Text {
            id: the_text
            text: pixel_info_string
            color: "white"
            font.pixelSize: font_size
            font.family: XsStyle.fixWidthFontFamily
            opacity: font_opacity 
        }
    }

}
