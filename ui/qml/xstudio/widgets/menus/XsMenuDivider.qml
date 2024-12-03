import QtQuick 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

Item {

    height: name ? 30 : XsStyleSheet.menuPadding*2 + XsStyleSheet.menuDividerHeight
    property var minWidth: label_metrics.width + 20

    TextMetrics {
        id:     label_metrics
        font:   labelDiv.font
        text:   labelDiv.text
    }

    function hideSubMenus() {}

    RowLayout {

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