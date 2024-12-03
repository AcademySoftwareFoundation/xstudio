// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QuickFuture 1.0
import QuickPromise 1.0
import xStudio 1.0
import xstudio.qml.models 1.0

Rectangle {

    width: layout.width+10
    height: layout.height+10
    color: "transparent"

    id: control
    visible: pixel_info_string != "" && pixel_info_viewport == view.name//pixel_info_attributes.enabled ? pixel_info_attributes.enabled : false

    XsModuleData {
        id: pixel_info_model_data
        modelDataName: "pixel_info_attributes"
    }

    XsAttributeValue {
        id: __pixel_info_string
        attributeTitle: "Pixel Info"
        model: pixel_info_model_data
    }
    property alias pixel_info_string: __pixel_info_string.value

    XsAttributeValue {
        id: __pixel_info_title
        attributeTitle: "Pixel Info Title"
        model: pixel_info_model_data
    }
    property alias pixel_info_title: __pixel_info_title.value

    XsAttributeValue {
        id: __pixel_info_viewport
        attributeTitle: "Current Viewport"
        model: pixel_info_model_data
    }
    property alias pixel_info_viewport: __pixel_info_viewport.value

    XsAttributeValue {
        id: __font_size
        attributeTitle: "Font Size"
        model: pixel_info_model_data
    }
    property alias font_size: __font_size.value

    XsAttributeValue {
        id: __font_opacity
        attributeTitle: "Font Opacity"
        model: pixel_info_model_data
    }
    property alias font_opacity: __font_opacity.value

    XsAttributeValue {
        id: __bg_opacity
        attributeTitle: "Bg Opacity"
        model: pixel_info_model_data
    }
    property alias bg_opacity: __bg_opacity.value

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: "black"
        opacity: bg_opacity
        border.color: "white"
        border.width: 2
    }

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
            font.family: XsStyleSheet.fixedWidthFontFamily
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
            font.family: XsStyleSheet.fixedWidthFontFamily
            opacity: font_opacity
        }
    }

}
