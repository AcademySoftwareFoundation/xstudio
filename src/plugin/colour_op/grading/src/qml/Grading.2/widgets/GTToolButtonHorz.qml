// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

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
        XsImage { id: imgDiv
            Layout.maximumWidth: parent.height
            Layout.preferredWidth: parent.height
            Layout.fillHeight: true
            antialiasing: true
            smooth: true
            imgOverlayColor: palette.text
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
