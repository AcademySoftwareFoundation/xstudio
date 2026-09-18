// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts

import xStudio 1.0


XsComboBox{

    focus: false
    editable: true
    textField.font.weight: Font.Black

    signal clearButtonPressed(int clearedIndex)

    XsPrimaryButton{ id: clearBtn
        imgSrc: "qrc:/icons/close.svg"
        visible: currentIndex !== -1 && (hovered || popup.opened )

        width: height
        height: parent.height - XsStyleSheet.panelPadding

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 25 + XsStyleSheet.panelPadding

        forcedBgColorNormal: XsStyleSheet.panelTitleBarColor

        onClicked: {
            if(popupOptions.opened) popupOptions.close()
            var idx = currentIndex
            currentIndex = -1
            parent.focus = false
            clearButtonPressed(idx)
        }
    }
}