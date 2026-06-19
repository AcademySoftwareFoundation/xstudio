// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

import xStudio 1.0

import "."

Rectangle{ 
    
    id: header
    width: parent.width
    height: XsStyleSheet.widgetStdHeight
    color: XsStyleSheet.widgetBgNormalColor

    property alias mouseX: mouseArea.mouseX
    property alias model: repeater.model
    property alias barWidth: titleBar.width
    property alias dragIdx: mouseArea.dragIdx
    property alias dragging: mouseArea.pressed
    property var columns_root_model

    property alias columns_model: columns_model

    DelegateModel {

        id: columns_model
        model: columns_root_model

        delegate: FSListHeaderColumn {

            Layout.fillWidth: resizable ? false : true
            Layout.preferredWidth: resizable ? size : -1
            Layout.minimumWidth: resizable ? 40 : size
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            onXChanged: {
                var cp = columnPositions
                cp[index] = x
                columnPositions = cp
            }
            onWidthChanged: {
                var cw = columnWidths
                cw[index] = width
                columnWidths = cw
            }

        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: dragIdx !== -1 ? Qt.SizeHorCursor : Qt.ArrowCursor
        property real mouseXOnPress: 0
        property real sizeOnPress: 0
        property bool isExpanded: false
        property int dragIdx: -1

        onExited: {
            if (!pressed && dragIdx !== -1) {
                dragIdx = -1
            }
        }

        onPressed: {
            //sizeOnPress = size
            if (dragIdx == -1) return
            sizeOnPress = columns_root_model.get(dragIdx).size
            mouseXOnPress = mouse.x
        }

        onReleased: {
            if (!containsMouse && dragIdx !== -1) {
                dragIdx = -1
            }
        }

        onMouseXChanged: (mouse)=> {

            if(pressed && dragIdx !== -1) {

                var delta = mouse.x - mouseXOnPress
                var newSize = sizeOnPress - delta
                var v = columns_root_model.get(dragIdx)
                v.size = newSize
                columns_root_model.set(dragIdx, v)

            } else {

                for (var i = 0; i < columnPositions.length; i++) {
                    if (Math.abs(mouse.x - columnPositions[i]) < 4) {
                        dragIdx = i
                        return
                    }
                }
                if (dragIdx !== -1) {
                    dragIdx = -1
                }

            }

        }

    }

    RowLayout{ 
        
        id: titleBar
        width: parent.width
        height: parent.height
        spacing: 0

        Repeater{
            id: repeater
            model: columns_model
        }

    }

}