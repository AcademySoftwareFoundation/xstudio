// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0

RowLayout {
    id: collapsed_widget
    property alias collapsed: expand_button.collapsed
    property string text: ""

    XsExpandButton {
        Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
        width: 32
        height: width
        id: expand_button
    }

    XsLabel {
        id: label
        font.pixelSize: XsStyle.popupControlFontSize
        verticalAlignment: Text.AlignVCenter
        color: "white"
        text: collapsed_widget.text
    }
}
