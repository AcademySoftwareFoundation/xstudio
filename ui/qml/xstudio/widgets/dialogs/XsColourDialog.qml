// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls

import xstudio.qml.models 1.0
import xStudio 1.0

XsWindow {

	id: dialog
	width: 330
	height: 450

    signal rejected()
    signal accepted()

    property color currentColour: hsv_to_rgb(hsvColour)

    property real hue: 0.0
    property real sat: 0.0
    property real value__: slider.value/100.0
    property var hsvColour:  Qt.vector3d(hue, sat, value__)
    property var linkColour

    onHsvColourChanged: {
        if (wheelPressed) currentColour = hsv_to_rgb(hsvColour)
    }

    onLinkColourChanged: {
        if (!wheelPressed && (typeof linkColour == "object" || typeof linkColour == "color")) {
            currentColour = linkColour
        }
    }

    onCurrentColourChanged: {
        if (!wheelPressed) {
            var hsv = rgb_to_hsv(Qt.vector3d(currentColour.r, currentColour.g, currentColour.b))
            hue = hsv.x
            sat = hsv.y
            slider.value = hsv.z*100.0
        }
    }

    property bool wheelPressed: wheelMa.pressed || slider.pressed

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 10

        Item {

            Layout.fillWidth: true
            Layout.fillHeight: true
            anchors.margins: 10

            Item {

                width: Math.min(parent.height, parent.width)
                height: width - 24
                anchors.centerIn: parent

                ShaderEffect {

                    id: wheel
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: height
                    fragmentShader: "qrc:/shaders/colourwheel.frag.qsb"
                    property real vvv: slider.value/100.0
                    property real rim: 1.0

                    MouseArea {
                        id: wheelMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onPositionChanged: (mouse) => {
                            if (mouse.buttons == Qt.LeftButton) {
                                var xx = mouse.x*2.0/width - 1.0
                                var yy = mouse.y*2.0/width - 1.0
                                var angle = Math.atan2(xx, yy)
                                var d = Math.sqrt(xx*xx+yy*yy);
                                hue = (angle+Math.PI) / (2.0*Math.PI);
                                sat = clamp(d, 0.0, 1.0);
                            }
                        }
                    }   

                    Rectangle {
                        width: 14
                        height: 14
                        radius: width/2
                        property var dist: Math.abs(hsvColour.y)
                        property var angle: (1 - hsvColour.x) * (2 * Math.PI)
                        x: (Math.sin(angle) * dist/2.0 + 0.5)*parent.width - width/2
                        y: (0.5 - Math.cos(angle) * dist/2.0)*parent.width - width/2
                        border.width: 1
                    }

                }            

                XsSlider {

                    id: slider
                    value: 0.0
                    to: 100.0
                    from: 0.0

                    stepSize: 0.1
                    orientation: Qt.Vertical

                    height: wheel.height
                    anchors.left: wheel.right
                    anchors.leftMargin: 10
                    width: 14

                    background: Rectangle {

                        gradient: Gradient {
                            GradientStop { position: 1.0; color: "black" }
                            GradientStop { position: 0.0; color: hsv_to_rgb( Qt.vector3d(hsvColour.x, hsvColour.y, 1.0) ) }
                        }
        
                    }

                }
            }

        }

        Item {
            Layout.preferredHeight: 20
        }

        GridLayout {

            Layout.fillWidth: true
            columns: 10
            columnSpacing: 5
            rowSpacing: 5

            Rectangle {
                radius: 4
                Layout.rowSpan: 4
                Layout.preferredWidth: 60
                color: currentColour
                Layout.fillHeight: true
                border.width: 1
                border.color: "white"
            }
    
            Repeater {

                model: 36
                Rectangle {
                    radius: 4
                    Layout.preferredHeight: 20
                    Layout.fillWidth: true
                    Layout.horizontalStretchFactor: 1
                    property int col: index%9
                    property int row: index/9
                    property var foo: Qt.vector3d(
                        col/9,
                        row == 3 ? 0.0 : row == 0 ? 0.7 : 1.0,
                        row == 3 ? col/8 : row == 2 ? 0.7 : 1.0)
                    color: hsv_to_rgb(foo)
                    border.width: 1
                    border.color: cma.containsMouse ? "white" : "transparent"
                    MouseArea {
                        id: cma
                        anchors.fill: parent
                        hoverEnabled: true
                        onPressed: currentColour = parent.color
                    }
                    XsIcon {
                        source: "qrc:/icons/check.svg"
                        anchors.fill: parent
                        visible: parent.color == currentColour
                    }
                }
            }
        }

        Item {
            Layout.preferredHeight: 20
        }

        RowLayout {

            Layout.alignment: Qt.AlignRight
            Layout.fillHeight: false
            spacing: 5

            XsSimpleButton {
                id: btnCancel
                text: qsTr("Cancel")
                width: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: rejected()
            }
            XsSimpleButton {
                id: btnToClipboard
                text: qsTr("Ok")
                width: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: accepted()
            }
        }
    }

    function clamp(number, min, max) {
        return Math.max(min, Math.min(number, max))
    }

    function rgb_to_hsv(color) {

        var h, s, v = 0.0
        var r = color.x
        var g = color.y
        var b = color.z

        var max = Math.max(r, g, b)
        var min = Math.min(r, g, b)
        var delta = max - min

        v = max
        s = max === 0 ? 0 : delta / max

        if (max === min) {
            h = 0
        } else if (r === max) {
            h = (g - b) / delta
        } else if (g === max) {
            h = 2 + (b - r) / delta
        } else if (b === max) {
            h = 4 + (r - g) / delta
        }

        h = h < 0 ? h + 6 : h
        h /= 6

        // Handle extended range inputs (from OpenColorIO RGB_TO_HSV builtin)
        if (min < 0) {
            v += min
        }
        if (-min > max) {
            s = delta / -min
        }

        return Qt.vector3d(h, s, v)
    }

    function hsv_to_rgb(color) {

        var MAX_SAT = 1.999

        var r, g, b = 0.0
        var h = color.x
        var s = color.y
        var v = color.z

        h = ( h - Math.floor( h ) ) * 6.0
        s = clamp( s, 0.0, MAX_SAT )
        v = v

        r = clamp( Math.abs(h - 3.0) - 1.0, 0.0, 1.0 )
        g = clamp( 2.0 - Math.abs(h - 2.0), 0.0, 1.0 )
        b = clamp( 2.0 - Math.abs(h - 4.0), 0.0, 1.0 )

        var max = v
        var min = v * (1.0 - s)

        // Handle extended range inputs (from OpenColorIO HSV_TO_RGB builtin)
        if (s > 1.0)
        {
            min = v * (1.0 - s) / (2.0 - s)
            max = v - min
        }
        if (v < 0.0)
        {
            min = v / (2.0 - s)
            max = v - min
        }

        var delta = max - min
        r = r * delta + min
        g = g * delta + min
        b = b * delta + min

        return Qt.rgba(r, g, b, 1.0)
    }

}