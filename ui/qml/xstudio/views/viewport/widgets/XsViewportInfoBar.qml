import QtQuick 2.12
import QtQuick.Layouts 1.15
// import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import "."
import "./toolbar"

Rectangle {
    id: toolBar
    x: barPadding
    width: parent.width-(x*2) //parent.width
    color: XsStyleSheet.panelTitleBarColor

    height: btnHeight*opacity
    visible: opacity != 0.0
    Behavior on opacity {NumberAnimation{ duration: 150 }}

    property string panelIdForMenu: panelId //TODO: to fix, not defined for quick-view

    property real barPadding: XsStyleSheet.panelPadding
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight

    Item {
        id: bgDivLeft;
        anchors.left: parent.left
        anchors.right: rowDiv.left
        anchors.rightMargin: rowDiv.spacing
        height: btnHeight
    }
    Item {
        id: bgDivRight;
        anchors.left: rowDiv.right
        anchors.leftMargin: rowDiv.spacing
        anchors.right: parent.right
        height: btnHeight
    }


    // onWidthChanged: { //#TODO: incomplete) for centered buttons
    //     if(parent.width < rowDiv.width) {

    //         rowDiv.preferredBtnWidth = (rowDiv.width/rowDiv.btnCount)
    //         rowDiv.width = toolBar.width
    //     }
    // }

    RowLayout{
        id: rowDiv
        spacing: 0


/*
        //     //for center buttons
        //     width = (preferredBtnWidth+spacing)*btnCount
        //     preferredBtnWidth = (maxBtnWidth*btnCount)>toolBar.width? (toolBar.width/btnCount) : maxBtnWidth

        //     //for fullWidth buttons
        //     width = parent.width - (spacing*(btnCount))
        //     preferredBtnWidth = (width/btnCount)
*/

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter

        width: (preferredBtnWidth+spacing)*btnCount //parent.width - (spacing*(btnCount))
        height: btnHeight

        property int btnCount: 6
        property real maxBtnWidth: 110
        property real preferredBtnWidth: (maxBtnWidth*btnCount)>toolBar.width? (toolBar.width/btnCount) : maxBtnWidth
        property real preferredMenuWidth: preferredBtnWidth<100? 100 : preferredBtnWidth

        // dummy 'valueText' property for offsetButton
        property var valueText

        Repeater {
            model: viewportPlayhead.autoAlignAttrData
            XsViewerMenuButton {
                Layout.preferredWidth: rowDiv.preferredBtnWidth
                Layout.preferredHeight: parent.height
                text: title
                shortText: abbr_title
                valueText: "" + value
                isBgGradientVisible: false
            }
        }

        XsViewerToolbarValueScrubber{
            id: offsetButton
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Offset"
            shortText: "Oft" //#TODO
            fromValue: -100
            toValue: 100
            stepSize: 1
            decimalDigits: 0
            isBgGradientVisible: false
            property int value: 0
            property real float_scrub_sensitivity: 0.05
            property int default_value: 0
            valueText: "" + value

            // another awkward two way binding!
            property int value__: viewportPlayhead.sourceOffsetFrames ? viewportPlayhead.sourceOffsetFrames : 0
            onValueChanged: {
                if (value != viewportPlayhead.sourceOffsetFrames) {
                    viewportPlayhead.sourceOffsetFrames = value
                }
            }

            onValue__Changed: {
                if (value != value__) {
                    value = value__
                }
            }

        }

        XsViewerMenuButton{  id: formatBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth*1.5
            Layout.preferredHeight: parent.height
            text: "Format"
            shortText: "Fmt" //#TODO
            valueText: currentOnScreenMediaSourceData.values.formatRole ? currentOnScreenMediaSourceData.values.formatRole : ""
            isBgGradientVisible: false
        }

        XsViewerMenuButton{  id: bitBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Bit Depth"
            shortText: "Bit" //#TODO
            valueText: currentOnScreenMediaSourceData.values.bitDepthRole ? currentOnScreenMediaSourceData.values.bitDepthRole : ""
            isBgGradientVisible: false
        }

        XsViewerMenuButton{  id: fpsBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "FPS"
            shortText: "FPS" //#TODO
            valueText: fps.toFixed(2)
            property real fps: currentOnScreenMediaSourceData.values.rateFPSRole ? currentOnScreenMediaSourceData.values.rateFPSRole : 24.0
            isBgGradientVisible: false
        }

        XsViewerMenuButton{  id: resBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Res"
            shortText: "Res" //#TODO
            valueText: currentOnScreenMediaSourceData.values.resolutionRole ? currentOnScreenMediaSourceData.values.resolutionRole : ""
            isBgGradientVisible: false
        }





    }
}