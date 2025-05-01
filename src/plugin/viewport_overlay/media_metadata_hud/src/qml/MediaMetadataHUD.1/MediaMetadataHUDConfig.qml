// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.qmlmodels

import xStudio 1.0
import xstudio.qml.models 1.0
import "."
// This is an adapted version of XsAttributesPanel, allowing us to add a dynamic
// list of masks that the user can enable/disable

XsWindow {

	id: dialog
	width: 500
	height: 500
    title: "Metadata Picker"
    property var windowVisible: visible
 
    ColumnLayout {
        anchors.fill: parent

        MediaMetadataViewer {
            Layout.fillWidth: true
            Layout.fillHeight: true
            select_mode: true
        }

        XsSimpleButton {

            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            Layout.rightMargin: 10
            Layout.bottomMargin: 10
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.close()
            }
        }
    }
    
}