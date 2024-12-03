// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.models 1.0
import xStudio 1.0

Item {

    Layout.fillWidth: true
    Layout.preferredWidth: 20
    Layout.fillHeight: true

    XsImage {

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 5
        width: height
        height: parent.height -2
        source: "qrc:/icons/help.svg"
        imgOverlayColor: ma.containsMouse ? palette.highlight : XsStyleSheet.hintColor
        antialiasing: true
        smooth: true

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                tooltip.visible = true
            }
        }

    }



    XsToolTip {
        id: tooltip
        text: descriptionRole
        width: metricsDiv.width>prefsLabelWidth*1.5 ? prefsLabelWidth*1.5: metricsDiv.width
        visible: ma.containsMouse
    }

}
