import QtQuick 2.15

import xStudioReskin 1.0

Item {
    width: parent.width
    height: XsStyleSheet.menuPadding*2 + XsStyleSheet.menuDividerHeight

    Rectangle {
        width: parent.width
        height: XsStyleSheet.menuDividerHeight
        anchors.verticalCenter: parent.verticalCenter
        color: XsStyleSheet.menuDividerColor
    }
}