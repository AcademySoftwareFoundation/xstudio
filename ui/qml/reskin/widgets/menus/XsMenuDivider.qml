import QtQuick 2.15

import xStudioReskin 1.0

Item {
    width: parentWidth
    height: XsStyleSheet.menuPadding*2 + XsStyleSheet.menuDividerHeight

    property real parentWidth: 0
    
    Rectangle {
        width: parent.width
        height: XsStyleSheet.menuDividerHeight
        anchors.verticalCenter: parent.verticalCenter
        color: XsStyleSheet.menuDividerColor
    }
}