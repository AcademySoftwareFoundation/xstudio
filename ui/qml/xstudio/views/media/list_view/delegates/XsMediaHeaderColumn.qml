// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudio 1.0

Item{

    id: headerItem

    width: size

    property real thumbWidth: 1
    property real titleBarTotalWidth: 0

    property real itemPadding: XsStyleSheet.panelPadding/2
    property var leftMargin: 12

    property color highlightColor: palette.highlight

    signal headerColumnResizing()
    property alias isDragged: dragArea.isDragActive
    onIsDraggedChanged:{
        headerColumnResizing()
    }

    onWidthChanged: {
        if (data_type == "thumbnail") {
            // thumbnail column sets the row height so that thumbnails scale
            // sets the size of the media items
            rowHeight = width*9/16
        }
    }

    Rectangle{

        id: headerDiv
        width: parent.width //- dragThumbDiv.width
        height: parent.height
        color: XsStyleSheet.panelTitleBarColor
        anchors.verticalCenter: parent.verticalCenter

        XsText{ id: titleDiv
            text: title ? title : ""
            anchors.verticalCenter: parent.verticalCenter
            x: position == "left" ? leftMargin : (parent.width-width)/2
        }

        MouseArea {

            id: headerMArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: dragArea.isDragActive? Qt.SizeHorCursor : Qt.ArrowCursor
            propagateComposedEvents: true
        }


        XsSecondaryButton{

            id: settings_btn
            visible: headerMArea.containsMouse || hovered || sort_btn.hovered
            width: XsStyleSheet.secondaryButtonStdWidth
            height: XsStyleSheet.secondaryButtonStdWidth
            bgColorNormal: XsStyleSheet.panelTitleBarColor
            imgSrc: "qrc:/icons/settings.svg"
            anchors.right: sort_btn.left
            anchors.rightMargin: itemPadding
            anchors.verticalCenter: parent.verticalCenter
            onClicked:{
                configure(columns_root_model.index(index, 0))
            }
        }

        XsSecondaryButton{

            id: sort_btn
            visible: headerMArea.containsMouse || hovered || settings_btn.hovered
            width: XsStyleSheet.secondaryButtonStdWidth
            height: XsStyleSheet.secondaryButtonStdWidth
            bgColorNormal: XsStyleSheet.panelTitleBarColor
            imgSrc: "qrc:/icons/triangle.svg"
            rotation: isSortedDesc? 0 : 180
            anchors.right: parent.right
            anchors.rightMargin: itemPadding
            anchors.verticalCenter: parent.verticalCenter
            property bool isSortedDesc: false
            onClicked:{
                sort_media(index, !isSortedDesc)
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
            visible: resizable ? resizable : false
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

                }

            }
        }

    }

}