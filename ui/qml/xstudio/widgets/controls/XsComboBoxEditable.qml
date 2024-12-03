// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudio 1.0


XsComboBox{ id: widget

    focus: false
    editable: true
    textField.font.weight: Font.Black

    property real panelPadding: XsStyleSheet.panelPadding

    // model: null
    // editText: ""

    XsPrimaryButton{ id: clearBtn
        imgSrc: "qrc:/icons/close.svg"
        visible: widget.currentIndex !== -1 && (widget.hovered || widget.popup.opened )

        width: height
        height: widget.height - panelPadding

        anchors.verticalCenter: widget.verticalCenter
        anchors.right: widget.right
        anchors.rightMargin: 25 + panelPadding

        forcedBgColorNormal: XsStyleSheet.panelTitleBarColor

        onClicked: {
            if(widget.popupOptions.opened) widget.popupOptions.close()
            widget.currentIndex = -1
            widget.focus = false
        }
    }
}