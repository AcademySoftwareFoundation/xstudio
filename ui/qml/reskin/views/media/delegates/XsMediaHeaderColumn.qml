// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

Item{ 
    
    id: headerItem
    
    width: size

    property real thumbWidth: 1
    property real titleBarTotalWidth: 0

    property bool sorted: false
    property bool isSortedDesc: false
    property real itemPadding: XsStyleSheet.panelPadding/2

    property color highlightColor: palette.highlight

    signal headerColumnClicked()
    signal headerColumnResizing()
    property alias isDragged: dragArea.isDragActive
    onIsDraggedChanged:{
        headerColumnResizing()

        if(title=="File") toggleFillWidth = true
        else toggleFillWidth = false
    }
    property bool toggleFillWidth: false

    
    Rectangle{ id: headerDiv
        width: parent.width //- dragThumbDiv.width
        height: parent.height
        color: XsStyleSheet.panelTitleBarColor
        anchors.verticalCenter: parent.verticalCenter

        Rectangle{ //#TODO: test
            anchors.fill: parent
            color: highlightColor
            visible: isDragged
        }

        Rectangle{ id: highlightDiv
            visible: headerMArea.containsMouse && sortable //|| dragArea.isDragActive
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: "#33FFFFFF" }
                // dragArea.isDragActive? palette.highlight:"#33FFFFFF" } //!sortable? "#55000000": //headerMArea.pressed? palette.highlight
            }
        }
        XsText{ id: titleDiv
            text: title
            anchors.verticalCenter: parent.verticalCenter
            width: sortBtn.visible? (parent.width - sortBtn.width - itemPadding*3) : (parent.width - itemPadding*2)
            anchors.right: sortBtn.visible? sortBtn.left : parent.right
            anchors.rightMargin: itemPadding

            horizontalAlignment: text=="File"? Text.AlignLeft : Text.AlignHCenter
        }
        MouseArea { id: headerMArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: dragArea.isDragActive? Qt.SizeHorCursor : Qt.ArrowCursor

            onClicked: headerColumnClicked()
        }
        
        XsSecondaryButton{ id: sortBtn
            visible: sortable && sorted
            width: XsStyleSheet.secondaryButtonStdWidth
            height: XsStyleSheet.secondaryButtonStdWidth
            imgSrc: "qrc:/icons/triangle.svg"
            rotation: isSortedDesc? 0 : 180
            anchors.right: parent.right
            anchors.rightMargin: itemPadding
            anchors.verticalCenter: parent.verticalCenter
            enabled: false
            onlyVisualyEnabled: true

            onClicked:{
                isSortedDesc = !isSortedDesc
            }
        }

    }

    Item{ id: dragThumbDiv
        width: thumbWidth*4
        height: parent.height


        Rectangle{ id: visualThumb
            width: dragArea.containsMouse || dragArea.isDragActive? thumbWidth*2 : thumbWidth
            height: parent.height
            anchors.right: parent.right
            color: dragArea.containsMouse || dragArea.isDragActive? highlightColor : XsStyleSheet.widgetBgNormalColor
        }
        
        anchors.verticalCenter: parent.verticalCenter
        x: headerItem.width-width 

        Drag.active: dragArea.isDragActive

        MouseArea { id: dragArea
            visible: resizable
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor

            property bool isDragActive: drag.active

            drag.target: parent 
            drag.minimumX: 16+(4*2)+(12) 
            // drag.maximumX: 100
            drag.smoothed: false

            property real mouseXOnPress: 0
            property real mouseXOnRelease: 0
            property bool isExpanded: false

            onPressed: {
                dragArea.mouseXOnPress = mouseX
            }

            onMouseXChanged: {
                if(dragArea.mouseXOnPress > mouseX) isExpanded = false
                else isExpanded = true

                if(drag.active) {

                    size = (dragThumbDiv.x + dragThumbDiv.width) // / titleBarTotalWidth
                    

                    // if(isExpanded) {
                    //     headerItemsModel.get(index+1).size += headerItemsModel.get(index).size
                    // }
                    // else{

                    // }
                    
                }
            
            }
        }

    }

}