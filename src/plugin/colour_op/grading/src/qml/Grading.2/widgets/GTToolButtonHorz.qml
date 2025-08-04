// SPDX-License-Identifier: Apache-2.0

import QtQuick

import QtQuick.Layouts

import xstudio.qml.bookmarks 1.0



import xStudio 1.0

XsPrimaryButton{ id: control

    property alias txt: txtDiv.text
    property alias src: imgDiv.source

    font.pixelSize: XsStyleSheet.fontSize

    RowLayout{
        width: parent.width
        height: 16
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 1
        spacing: 2
        opacity: parent.enabled || parent.isUnClickable? 1.0 : 0.33

        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        XsIcon { id: imgDiv
            Layout.maximumWidth: parent.height
            Layout.preferredWidth: parent.height
            Layout.fillHeight: true
            source: ""
        }
        XsText { id: txtDiv
            Layout.preferredWidth: textWidth
            Layout.fillHeight: true
            text: ""
            font: control.font
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

    }

    onClicked: {
    }
}
