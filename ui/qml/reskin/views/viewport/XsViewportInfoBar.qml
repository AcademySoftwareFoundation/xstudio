import QtQuick 2.12
import QtQuick.Layouts 1.15
// import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0
import "./widgets"

Rectangle {
    id: toolBar
    x: barPadding
    width: parent.width-(x*2) //parent.width
    height: btnHeight //+(barPadding*2)
    color: XsStyleSheet.panelTitleBarColor

    property string panelIdForMenu: panelId

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

        property int btnCount: 5
        property real maxBtnWidth: 110
        property real preferredBtnWidth: (maxBtnWidth*btnCount)>toolBar.width? (toolBar.width/btnCount) : maxBtnWidth
        property real preferredMenuWidth: preferredBtnWidth<100? 100 : preferredBtnWidth
       
        // dummy 'value' property for offsetButton
        property var value         

        XsViewerSeekEditButton{ id: offsetButton
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Offset"
            shortText: "Oft" //#TODO
            fromValue: -10
            defaultValue: 0
            toValue: 10
            valueText: 0
            stepSize: 1
            decimalDigits: 0
            showValueWhenShortened: true
            isBgGradientVisible: false
        }
        
        XsViewerMenuButton{  id: formatBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Format"
            shortText: "Fmt" //#TODO
            valueText: "dnxhd"
            clickDisabled: true
            showValueWhenShortened: true
            isBgGradientVisible: false
        }
        XsViewerMenuButton{  id: bitBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Bit Depth"
            shortText: "Bit" //#TODO
            valueText: "8 bits"
            clickDisabled: true
            showValueWhenShortened: true
            isBgGradientVisible: false
        }
        XsViewerMenuButton{  id: fpsBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "FPS"
            shortText: "FPS" //#TODO
            valueText: "24.0"
            clickDisabled: true
            showValueWhenShortened: true
            isBgGradientVisible: false
        }
        XsViewerMenuButton{  id: resBtn
            Layout.preferredWidth: rowDiv.preferredBtnWidth
            Layout.preferredHeight: parent.height
            text: "Res"
            shortText: "Res" //#TODO
            valueText: "1920x1080"
            clickDisabled: true
            showValueWhenShortened: true
            isBgGradientVisible: false
            shortThresholdWidth: 99+10 //60+30
        }
        

        


    }
}