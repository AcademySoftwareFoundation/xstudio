// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls
import QtQuick.Shapes 1.9
import QtQuick.Effects

import xStudio 1.0
import xstudio.qml.models 1.0
import "."

Rectangle {

    id: root
    height: parent.height
    width: visible ? height : 0
    property var render_status: false
    // this button is arranged by a RowLayout - we make it match the height
    // and make it square thus:

    color: ma.pressed ? palette.highlight : "transparent"
    border.color: ma.containsMouse ? palette.highlight : "transparent"

    // access 'attribute' data exposed by our C++ plugin
    XsModuleData {
        id: video_render_attrs
        modelDataName: "video_render_attrs"
    }
    property alias video_render_attrs: video_render_attrs

    XsAttributeValue {
        id: __jobs_status_data
        attributeTitle: "jobs_status_data"
        model: video_render_attrs
    }
    property alias jobs_status_data: __jobs_status_data.value

    XsAttributeValue {
        id: __render_queue_visible
        attributeTitle: "render_queue_visible"
        model: video_render_attrs
    }
    property alias render_queue_visible: __render_queue_visible.value

    XsAttributeValue {
        id: __overall_status
        attributeTitle: "overall_status"
        model: video_render_attrs
    }
    property alias overall_status: __overall_status.value

    visible: overall_status != ""

    XsIcon {
        source: "qrc:/icons/movie.svg"
        imgOverlayColor: overall_status == "Error" ? "red" : XsStyleSheet.primaryTextColor
        width: parent.width
        height: width
    }

    Item {
        anchors.bottom: parent.bottom
        width: parent.height
        height: 2
        visible: overall_status != "Done"

        ShaderEffect { 
            id: imgDiv
            PropertyAnimation {
                target: imgDiv
                property: "fangle"
                from: 0
                to: 360
                duration: 2000
                loops: Animation.Infinite
                running: root.visible && overall_status != "Done"
            }
            anchors.fill: parent
            fragmentShader: "qrc:/shaders/activity_wheel.frag.qsb"
            property real shdr_radius: width/2.4
            property real sz: width
            property real line_width: 2.5
            property real fangle: 0.0
            property color line_colour: palette.highlight
        }
    }

    MouseArea {
        id: ma
        hoverEnabled: true
        anchors.fill: parent
        onClicked: {
            render_queue_visible = !render_queue_visible
        }
    }

}
