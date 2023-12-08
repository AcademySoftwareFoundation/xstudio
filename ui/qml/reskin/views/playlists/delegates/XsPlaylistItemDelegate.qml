// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

Button {
    id: contentDiv
    text: isMissing? "This playlist no longer exists" : _title
    width: parent.width; 
    height: parent.height

    property color bgColorPressed: "#33FFFFFF"
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color hintColor: XsStyleSheet.hintColor
    property color errorColor: XsStyleSheet.errorColor
    property var itemCount: 2 //_count
    property bool isSelected: false
    property bool isMissing: false
    
    font.pixelSize: textSize
    font.family: textFont
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.33

    contentItem:
    Item{
        anchors.fill: parent

        RowLayout{
            x: 4
            spacing: 4
            width: parent.width-(x*2)
            height: XsStyleSheet.widgetStdHeight
            anchors.verticalCenter: parent.verticalCenter
            
            XsSecondaryButton{ id: subsetBtn
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                imgSrc: "qrc:/assets/icons/new/arrow_drop_down.svg"
            }
            XsImage{
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: isMissing? "qrc:/assets/icons/new/error.svg" : "qrc:/assets/icons/new/draft.svg"
                imgOverlayColor: isMissing? errorColor : hintColor
            }
            Text {
                id: textDiv
                text: contentDiv.text+"-"+index //#TODO
                font: contentDiv.font
                color: isMissing? hintColor : textColorNormal 
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                topPadding: 2
                bottomPadding: 2
                leftPadding: 8
                
                // anchors.horizontalCenter: parent.horizontalCenter
                elide: Text.ElideRight
                
                Layout.fillWidth: true
                Layout.preferredHeight: 16
    
                XsToolTip{
                    text: contentDiv.text
                    visible: contentDiv.hovered && parent.truncated
                    width: metricsDiv.width == 0? 0 : contentDiv.width
                }
            }
            Item{
                Layout.preferredWidth: addBtn.visible? 16: countDiv.textWidth
                Layout.preferredHeight: 16

                XsText{ id: countDiv
                    text: itemCount
                    anchors.centerIn: parent
                    visible: !addBtn.visible
                    color: hintColor
                }
                XsSecondaryButton{ id: addBtn
                    anchors.fill: parent
                    imgSrc: "qrc:/assets/icons/new/add.svg"
                    visible: contentDiv.hovered
                    imgOverlayColor: hintColor
                }
            }
            
            Item{
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16

                XsImage{ id: errorIndicator
                    anchors.fill: parent
                    source: "qrc:/assets/icons/new/error.svg"
                    visible: !moreBtn.visible && index%2==0
                    imgOverlayColor: hintColor
                }
                XsSecondaryButton{ id: moreBtn
                    anchors.fill: parent
                    imgSrc: "qrc:/assets/icons/new/more_horiz.svg"
                    visible: contentDiv.hovered
                    imgOverlayColor: hintColor
                }
            }
        }
    }

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: contentDiv.down || contentDiv.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: contentDiv.down || isSelected? bgColorPressed : forcedBgColorNormal
        
        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: contentDiv.activeFocus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }
    }

    onPressed: {
        focus = true
        isSelected = !isSelected
    }
    onReleased: focus = false
    onPressAndHold: isMissing = !isMissing
    

}