import QtQuick

import xStudio 1.0

Item {

    id: parent_item
    visible: frame_error_message != ""
    anchors.fill: parent

    XsLabel {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 10
        text: frame_error_message
        color: "red"
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignBottom
        wrapMode: Text.Wrap
        font.weight: Font.Bold
        font.pixelSize: 20
    }

    /*Rectangle {
        anchors.centerIn: parent
        width: 500
        height: 500
    }*/
}