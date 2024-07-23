// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

Item{

    id: panel
    anchors.fill: parent
    property color bgColorPressed: palette.highlight 
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    property bool isSubDivider: false

    property real textSize: XsStyleSheet.fontSize
    property var textFont: XsStyleSheet.fontFamily
    property color textColorNormal: palette.text
    property color hintColor: XsStyleSheet.hintColor

    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    property real rowHeight: XsStyleSheet.widgetStdHeight

    //#TODO: just for testing
    property bool highlightTextOnActive: false

    // background
    Rectangle{
        z: -1000
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#5C5C5C" }
            GradientStop { position: 1.0; color: "#474747" }
        }
    }

    Item{id: actionDiv
        width: parent.width; 
        height: btnHeight+(panelPadding*2)

        RowLayout{
            x: panelPadding
            spacing: 1
            width: parent.width-(x*2)
            height: btnHeight
            anchors.verticalCenter: parent.verticalCenter
            
            XsPrimaryButton{ id: addBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                Layout.alignment: Qt.AlignLeft
                imgSrc: "qrc:/icons/add.svg"
            }
            XsPrimaryButton{ id: deleteBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/delete.svg"
            }
            XsSearchButton{ id: searchBtn
                Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
                Layout.preferredHeight: parent.height
                isExpanded: false
                hint: "Search media..."
            }
            XsText{
                Layout.fillWidth: true
                Layout.minimumWidth: 0//btnWidth
                Layout.preferredHeight: parent.height
                text: searchBtn.isExpanded? "" : selectedMediaSetProperties.values.nameRole ? selectedMediaSetProperties.values.nameRole : ""
                font.bold: true

                opacity: searchBtn.isExpanded? 0:1
                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuart }  }
            }
            XsPrimaryButton{ id: listViewBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/list.svg"
                isActive: true
                onClicked:{
                    listViewBtn.isActive = true
                    gridViewBtn.isActive = false
                }
            }
            XsPrimaryButton{ id: gridViewBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/view_grid.svg"
                onClicked:{
                    listViewBtn.isActive = false
                    gridViewBtn.isActive = true
                }
            }
            XsPrimaryButton{ id: moreBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                Layout.alignment: Qt.AlignRight
                imgSrc: "qrc:/icons/more_vert.svg"
            }

        }
        
    }
    
    Rectangle{ id: mediaListDiv
        x: panelPadding
        y: actionDiv.height
        width: panel.width-(x*2)
        height: panel.height-y-panelPadding
        color: XsStyleSheet.panelBgColor
        
        XsMediaHeader{
            
            id: titleBar
            width: mediaListDiv.width
            height: XsStyleSheet.widgetStdHeight
        }        

        XsMediaItems{ id: mediaList

            y: titleBar.height  

            columns_model: titleBar.columns_model
            itemRowHeight: rowHeight
            itemRowWidth: mediaListDiv.width
                   
        }

    }
    
}