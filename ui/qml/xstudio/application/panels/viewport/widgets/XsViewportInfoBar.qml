import QtQuick
import QtQuick.Layouts
//

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import "."
import "./toolbar"

Rectangle {
    x: XsStyleSheet.panelPadding
    width: parent.width-(x*2) //parent.width
    height: XsStyleSheet.widgetStdHeight * opacity

    color: XsStyleSheet.panelTitleBarColor
    visible: opacity != 0.0

    Behavior on opacity {NumberAnimation{ duration: 150 }}

    property string panelIdForMenu: panelId //TODO: to fix, not defined for quick-view


    Item {
        anchors.left: parent.left
        anchors.right: rowDiv.left
        anchors.rightMargin: rowDiv.spacing
        height: XsStyleSheet.widgetStdHeight
    }

    Item {
        anchors.left: rowDiv.right
        anchors.leftMargin: rowDiv.spacing
        anchors.right: parent.right
        height: XsStyleSheet.widgetStdHeight
    }

    RowLayout{
        id: rowDiv
        spacing: 0

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter

        width: (preferredBtnWidth + spacing) * btnCount
        height: XsStyleSheet.widgetStdHeight

        readonly property int btnCount: 6
        readonly property int preferredBtnWidth: Math.min(110, parent.width / btnCount)
        readonly property int preferredMenuWidth: Math.max(100, preferredBtnWidth)

        Repeater {
            model: viewportPlayhead.autoAlignAttrData
            XsViewerMenuButton {
                Layout.preferredWidth: rowDiv.preferredBtnWidth
                Layout.preferredHeight: parent.height

                text: title
                shortText: abbr_title
                valueText: "" + value
                isBgGradientVisible: false
                enabled: attr_enabled
            }
        }

        XsViewerMediaOffset {

            //Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.minimumWidth: 100

        }

        XsViewerMenuButton {
            Layout.preferredWidth: rowDiv.preferredBtnWidth*1.5
            Layout.preferredHeight: parent.height

            text: "Format"
            shortText: "Fmt" //#TODO
            valueText: currentOnScreenMediaSourceData.values.formatRole ? currentOnScreenMediaSourceData.values.formatRole : ""
            isBgGradientVisible: false
        }

        XsViewerMenuButton {
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height

            text: "Bit Depth"
            shortText: "Bit" //#TODO
            valueText: currentOnScreenMediaSourceData.values.bitDepthRole ? currentOnScreenMediaSourceData.values.bitDepthRole : ""
            isBgGradientVisible: false
        }

        XsViewerMenuButton {
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height

            text: "FPS"
            shortText: "FPS" //#TODO
            valueText: fps.toFixed(2)
            property real fps: currentOnScreenMediaSourceData.values.rateFPSRole ? currentOnScreenMediaSourceData.values.rateFPSRole : 24.0
            isBgGradientVisible: false
        }

        XsViewerMenuButton {
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height

            text: "Res"
            shortText: "Res" //#TODO
            valueText: currentOnScreenMediaSourceData.values.resolutionRole ? currentOnScreenMediaSourceData.values.resolutionRole : ""
            isBgGradientVisible: false
        }
    }
}