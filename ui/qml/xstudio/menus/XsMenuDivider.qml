import QtQuick
import QtQuick.Layouts

import xStudio 1.0

Item {

    id: root
    property int minWidth: rlayout.implicitWidth
    implicitHeight: text ? 30 : XsStyleSheet.menuPadding*2 + XsStyleSheet.menuDividerHeight
    property var text: name ? name : ""

    function hideSubMenus() {}

    RowLayout {
        id: rlayout

        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: XsStyleSheet.menuDividerHeight
            color: XsStyleSheet.menuBorderColor
        }

        XsText {
            id: labelDiv
            Layout.leftMargin: root.text ? 4 : 0
            Layout.rightMargin: root.text ? 4 : 0
            text: root.text
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            color: XsStyleSheet.primaryTextColor
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
        }

        Rectangle {
            Layout.fillWidth: true
            height: XsStyleSheet.menuDividerHeight
            color: XsStyleSheet.menuBorderColor
        }
    }
}