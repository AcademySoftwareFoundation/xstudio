import QtQuick
import QtQuick.Layouts

import xStudio 1.0

Item {

    property int minWidth: rlayout.implicitWidth
    implicitHeight: name ? 30 : XsStyleSheet.menuPadding*2 + XsStyleSheet.menuDividerHeight

    function hideSubMenus() {}

    RowLayout {
        id: rlayout

        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: XsStyleSheet.menuDividerHeight
            color: XsStyleSheet.menuDividerColor
        }

        XsText {
            id: labelDiv
            Layout.leftMargin: text ? 4 : 0
            Layout.rightMargin: text ? 4 : 0
            text: name ? name : ""
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            color: palette.text
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
        }

        Rectangle {
            Layout.fillWidth: true
            height: XsStyleSheet.menuDividerHeight
            color: XsStyleSheet.menuDividerColor
        }
    }
}