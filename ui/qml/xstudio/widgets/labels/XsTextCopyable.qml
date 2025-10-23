// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

Item {

    id: root
    property alias horizontalAlignment: widget.horizontalAlignment
    property alias verticalAlignment: widget.verticalAlignment
    property alias text: widget.text
    property alias font: widget.font
    opacity: enabled ? 1.0 : 0.5
    clip: true
    property bool truncated: widget.contentWidth > width
    implicitWidth: widget.contentWidth
    implicitHeight: widget.contentHeight

    TextEdit {

        id: widget
        x: Math.min(root.width-contentWidth, 0)
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        text: ""
        color: palette.text
        clip: true

        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily

        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter

        wrapMode: Text.NoWrap
        readOnly: true

        // using a shader effect to fade off left edge of text to indicate
        // if the text has elided (i.e. can't fit!)
        layer.enabled: contentWidth > (root.width+4)
        layer.samplerName: "maskSource"
        layer.effect: ShaderEffect {
            property real p1: (10-widget.x)/widget.width
            property real p0: -widget.x/widget.width
            fragmentShader: "qrc:/shaders/mask.frag.qsb"
        }

    }

}