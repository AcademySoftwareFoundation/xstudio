// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

import xStudioReskin 1.0

Item{

    id: panel
    anchors.fill: parent

    property color bgColorPressed: palette.highlight 
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property real textSize: XsStyleSheet.fontSize
    property var textFont: XsStyleSheet.fontFamily
    property color textColorNormal: palette.text
    property color hintColor: XsStyleSheet.hintColor

    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    property bool isEditToolsExpanded: false

    //#TODO: test
    property bool showIcons: false

    // background
    Rectangle{
        z: -1000
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#5C5C5C" }
            GradientStop { position: 1.0; color: "#474747" }
        }
    }

    Item{
        
        id: actionDiv
        width: parent.width; 
        height: btnHeight+(panelPadding*2)

        RowLayout{
            x: panelPadding
            spacing: 1
            width: parent.width-(x*2)
            height: btnHeight
            anchors.verticalCenter: parent.verticalCenter
            
            XsText{ 
                
                XsModelProperty {
                    id: playheadTimecode
                    role: "value"
                    index: currentPlayheadData.search_recursive("Current Source Timecode", "title")
                }
                    
                Connections {
                    target: currentPlayheadData // this bubbles up from XsSessionWindow
                    function onJsonChanged() {
                        playheadTimecode.index = currentPlayheadData.search_recursive("Current Source Timecode", "title")
                    }
                }
    
                id: timestampDiv
                Layout.preferredWidth: btnWidth*3
                Layout.preferredHeight: parent.height
                text: playheadTimecode.value ? playheadTimecode.value : "00:00:00:00"
                font.pixelSize: XsStyleSheet.fontSize +6
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter

            }

            XsPrimaryButton{ id: addPlaylistBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/add.svg"
                text: "Add"
                onClicked: showIcons = !showIcons
            }
            XsPrimaryButton{ id: deleteBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/delete.svg"
                text: "Delete"
            }
            XsPrimaryButton{ 
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/undo.svg"
                text: "Undo"
            }
            XsPrimaryButton{ 
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/redo.svg"
                text: "Redo"
            }
            XsSearchButton{ id: searchBtn
                Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
                Layout.preferredHeight: parent.height
                isExpanded: false
                hint: "Search..."
                // isExpandedToLeft: true
            }
            XsText{ id: titleDiv
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: parent.height
                text: viewedMediaSetProperties.values.nameRole
                font.bold: true
                
                opacity: searchBtn.isExpanded? 0:1
                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuart }  }
            }
            XsPrimaryButton{
                Layout.preferredWidth: btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Loop IO"
                onClicked:{
                    isActive = !isActive
                }
            }
            XsPrimaryButton{ 
                Layout.preferredWidth: btnWidth*2.6
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Loop Selection"
                onClicked:{
                    isActive = !isActive
                }
            }
            Item{
                Layout.preferredWidth: panelPadding/2
                Layout.preferredHeight: parent.height
            }
            XsPrimaryButton{
                Layout.preferredWidth: showIcons? btnWidth : btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: showIcons? "qrc:/icons/center_focus_strong.svg":""
                text: "Focus"
                onClicked:{
                    isActive = !isActive
                }
            }
            XsPrimaryButton{
                Layout.preferredWidth: btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Ripple"
                onClicked:{
                    isActive = !isActive
                }
            }
            XsPrimaryButton{
                Layout.preferredWidth: btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Gang"
                onClicked:{
                    isActive = !isActive
                }
            }
            XsPrimaryButton{
                Layout.preferredWidth: btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Snap"
                onClicked:{
                    isActive = !isActive
                }
            }
            Item{
                Layout.preferredWidth: panelPadding/2
                Layout.preferredHeight: parent.height
            }
            XsPrimaryButton{
                Layout.preferredWidth: btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Overwrite"
            }
            XsPrimaryButton{
                Layout.preferredWidth: btnWidth*1.8
                Layout.preferredHeight: parent.height
                imgSrc: ""
                text: "Insert"
            }
            Item{
                Layout.preferredWidth: panelPadding/2
                Layout.preferredHeight: parent.height
            }
            XsPrimaryButton{ id: settingsBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/settings.svg"
            }
            XsPrimaryButton{ id: filterBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/filter.svg"
            }
            XsPrimaryButton{ id: morePlaylistBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/more_vert.svg"
            }

        }
        
    }
    
    Rectangle{ 
        
        id: timelineDiv
        x: panelPadding
        y: actionDiv.height
        width: panel.width-(x*2)
        height: panel.height-y-panelPadding
        color: XsStyleSheet.panelBgColor

        XsTimelineEditTools{

            x: spacing
            y: spacing

            width: isEditToolsExpanded? cellWidth*2 : cellWidth
            height: parent.height<cellHeight*(model.count/2)+cellHeight? cellHeight*(model.count/2)+cellHeight : parent.height

            onHeightChanged:{
                if(height<cellWidth*toolsModel.count) isEditToolsExpanded = true
                else isEditToolsExpanded = false
            }

            toolsModel: editToolsModel
        }
        
    }

    XsTimelinePanel
    { 
        id: theTimeline
        // TODO: do this with proper anchors. Can't seem to do it
        // without going over the edit buttons ....
        y: actionDiv.height
        width: parent.width
        height: parent.height
        x: 40        
    }

    ListModel{ id: editToolsModel

        ListElement{ _type:"basic"; _name:"Select"; _icon:"qrc:/icons/arrow_selector_tool.svg"}
        ListElement{ _type:"basic"; _name:"Move"; _icon:"qrc:/icons/open_with.svg"}
        // ListElement{ _type:"basic"; _name:"Move LR"; _icon:"qrc:/icons/arrows_outward.svg"}
        // ListElement{ _type:"basic"; _name:"Move UD"; _icon:"qrc:/icons/arrows_outward.svg"}
        ListElement{ _type:"basic"; _name:"Cut"; _icon:"qrc:/icons/content_cut.svg"}
        ListElement{ _type:"basic"; _name:"Roll"; _icon:"qrc:/icons/expand.svg"}
        ListElement{ _type:"basic"; _name:"Reorder"; _icon:"qrc:/icons/repartition.svg"}

    }

}