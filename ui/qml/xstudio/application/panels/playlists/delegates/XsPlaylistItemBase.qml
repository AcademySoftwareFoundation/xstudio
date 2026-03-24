// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts
import QtQml.Models 2.15
import QuickFuture 1.0

import xstudio.qml.helpers 1.0
import xStudio 1.0

import "."

Item {
    implicitHeight: itemRowStdHeight + indicator_height

    property real indicator_height: drag_target_indicator.height + end_drag_target_indicator.height

    property var iconSource: "qrc:/icons/list_alt.svg"
    property bool indent: false
    property bool isDragTarget: drag_drop_handler.isDragTarget && canReceiveDrag && incomingDragSource != "PlayList"
    property bool canReceiveDrag: true
    property bool isDragReorderTarget: drag_drop_handler.isDragTarget && canReceiveDrag && incomingDragSource == "PlayList"
    property bool dragToEnd: false
    property bool isDivider: false

    property var incomingDragSource

    property var metadataChanged: metadataChangedRole
    property var placeHolder: placeHolderRole
    property var decoratorModel: []

    function updateDecorations() {
        if(typeRole == "Playlist" && !placeHolder) {
            let meta = theSessionData.getJSONObject(modelIndex, "/ui/decorators")
            if(meta != undefined) {
                let tmp = []
                for (const [key, value] of Object.entries(meta)) {
                  tmp.push([key, value])
                }
                decoratorModel = tmp
            }
        }
    }

    onMetadataChangedChanged: updateDecorations()
    onPlaceHolderChanged: updateDecorations()

    Rectangle {
        id: drag_target_indicator
        width: parent.width
        height: visible ? 4 : 0
        visible: isDragReorderTarget && !dragToEnd
        color: XsStyleSheet.accentColor
        Behavior on height { NumberAnimation{duration: 250} }
    }

    Rectangle {
        id: end_drag_target_indicator
        anchors.bottom: parent.bottom
        width: parent.width
        height: visible ? 4 : 0
        visible: isDragReorderTarget && dragToEnd
        color: XsStyleSheet.accentColor
        Behavior on height { NumberAnimation{duration: 250} }
    }
    // background
    Rectangle {

        id: bgDiv
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: indicator_height
        height: itemRowStdHeight

        border.color: (down || hovered || isDragTarget) ? XsStyleSheet.accentColor : "transparent"
        border.width: 1
        color: down || isCurrent ? Qt.darker(XsStyleSheet.accentColor, 2) : isSelected ? Qt.darker(XsStyleSheet.accentColor, 3) : "transparent"

        Rectangle{
            z: -1
            width: parent.width
            height: 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            color: XsStyleSheet.baseColor
            visible: lineVisible
        }

    }

    /* modelIndex should be set to point into the session data model and get
    to the playlist that we are representing */
    property var modelIndex

    onModelIndexChanged: {
        if (modelIndex.valid && modelIndex.row) {
            let prev_item_idx = theSessionData.index(modelIndex.row-1,0,modelIndex.parent)
            lineVisible = theSessionData.get(prev_item_idx, "typeRole") != "ContainerDivider"
        } else {
            lineVisible = true
        }
        updateDecorations()
    }

    /* first index in playlist is media ... */

    property bool isCurrent: modelIndex == currentMediaContainerIndex
    property bool isSelected: sessionSelectionModel.isSelected(modelIndex)
    property bool isMissing: false
    property bool isExpanded: false
    property bool isExpandable: false
    property bool isViewed: modelIndex == viewportCurrentMediaContainerIndex
    property bool lineVisible: true

    property alias hovered: ma.containsMouse
    property alias down: ma.pressed

    Connections {
        target: sessionSelectionModel // this bubbles up from XsSessionWindow
        function onSelectedIndexesChanged() {
            isSelected = sessionSelectionModel.isSelected(modelIndex)
        }
    }

    MouseArea {

        id: ma
        anchors.fill: bgDiv
        height: parent.height
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        // flag
        MouseArea {
            id: fma
            y: indicator_height
            height: itemRowStdHeight
            width: flagIndicatorWidth
            hoverEnabled: true
            onClicked: (mouse)=> {
                showFlagMenu(mouse.x, mouse.y, this, modelIndex)
            }
            cursorShape: Qt.PointingHandCursor
            Rectangle{
                anchors.fill: parent
                color: flagColourRole
                border.width: 1
                border.color: fma.containsMouse ? XsStyleSheet.accentColor : "transparent"
            }
        }

        onReleased: (mouse) => {

            if (mouse.button == Qt.RightButton ||
                mouse.modifiers == Qt.ControlModifier ||
                mouse.modifiers == Qt.ShiftModifier) return;

            // If there is a multiselection, and the user has clicked on
            // something in the selection and then released the mouse without
            // holding shift or ctrl, we want to exclusively select the
            // item under the mouse (unless its the end of a drag/drop)
            interactive = true
            if (!drag_drop_handler.dragging) {
                sessionSelectionModel.setCurrentIndex(
                    modelIndex,
                    ItemSelectionModel.ClearAndSelect
                    )
            }
            drag_drop_handler.endDrag(mouse.x, mouse.y)

        }

        onPressed: (mouse) => {

            if (mouse.buttons == Qt.RightButton) {
                if (!isSelected) {
                    // right click does an exclusive select, unless
                    // already selected in which case selection doesn't change
                    sessionSelectionModel.setCurrentIndex(
                        modelIndex,
                        ItemSelectionModel.ClearAndSelect
                        )
                }
                showContextMenu(mouseX, mouseY, ma)
            }

            // Put the content of the playlist into the media browser etc.
            // but don't put it on screen.
            if (mouse.modifiers == Qt.ControlModifier) {
                if (!(sessionSelectionModel.selectedIndexes.length == 1 &&
                    sessionSelectionModel.selectedIndexes[0] == modelIndex)) {
                    sessionSelectionModel.select(modelIndex, ItemSelectionModel.Toggle)

                    // only change current selection if there is one item in selection.
                    if(sessionSelectionModel.selectedIndexes.length == 1) {
                        sessionSelectionModel.setCurrentIndex(sessionSelectionModel.selectedIndexes[0], ItemSelectionModel.NoUpdate)
                    }
                }

            } else if (!isSelected && mouse.modifiers == Qt.ShiftModifier && sessionSelectionModel.selectedIndexes.length) {

                // shift select ... find nearest item already in selection, and select all items
                // in between
                var nearest
                var dist = 1000000
                for (var i in sessionSelectionModel.selectedIndexes) {
                    var idx = sessionSelectionModel.selectedIndexes[i]
                    if (idx.parent == modelIndex.parent) {
                        if (Math.abs(idx.row-modelIndex.row) < dist) {
                            dist = Math.abs(idx.row-modelIndex.row)
                            nearest = idx
                        }
                    }
                }

                if (nearest) {
                    var start_row = nearest.row < modelIndex.row ? nearest.row : modelIndex.row
                    var end_row = nearest.row > modelIndex.row ? nearest.row : modelIndex.row
                    var indexes = []
                    for (var row = start_row; row <= end_row; ++row) {
                        indexes.push(theSessionData.index(row, 0, modelIndex.parent))
                    }
                    sessionSelectionModel.select(
                        helpers.createItemSelection(indexes),
                        ItemSelectionModel.Select
                    )
                }

                if(sessionSelectionModel.selectedIndexes.length == 1) {
                    sessionSelectionModel.setCurrentIndex(sessionSelectionModel.selectedIndexes[0], ItemSelectionModel.NoUpdate)
                }


            } else if (!isSelected) {
                sessionSelectionModel.setCurrentIndex(modelIndex, ItemSelectionModel.ClearAndSelect)
            }

            var valid_drag_selection = true
            for (var i in sessionSelectionModel.selectedIndexes) {
                var idx = sessionSelectionModel.selectedIndexes[i]
                if (idx.parent != sessionSelectionModel.selectedIndexes[0].parent) {
                    valid_drag_selection = false
                }
            }
            if (valid_drag_selection) {
                drag_drop_handler.startDrag(mouseX, mouseY)
            }
        }

        onPositionChanged: (mouse) => {
            if (pressed && sessionSelectionModel.selectedIndexes.length) {
                interactive = false
                drag_drop_handler.doDrag(mouse.x, mouse.y)
            }
        }

        onDoubleClicked: {
            // Put the content of the playlist into the media browser etc.
            // and also put it on screen
            sessionSelectionModel.setCurrentIndex(modelIndex, ItemSelectionModel.ClearAndSelect)
            viewportCurrentMediaContainerIndex = modelIndex
        }

    }

    RowLayout {
        anchors.fill: bgDiv
        anchors.leftMargin: indent ? subitemIndent : 0
        anchors.rightMargin: rightSpacing
        spacing: XsStyleSheet.panelPadding / 2
        visible: ! isDivider

        Item{

            Layout.fillHeight: true
            Layout.margins: 2
            Layout.preferredWidth: indent ? 0 : height

            XsSecondaryButton{
                z: 100
                imgSrc: "qrc:/icons/chevron_right.svg"
                visible: isExpandable != 0
                anchors.fill: parent
                rotation: (expandedRole)? 90:0
                imageSrcSize: width
                Behavior on rotation {NumberAnimation{duration: 150 }}

                onClicked: expandedRole = !expandedRole
            }
        }

        XsIcon{
            Layout.fillHeight: true
            Layout.margins: 2
            width: height
            source: iconSource
            imgOverlayColor: XsStyleSheet.hintColor
        }

        XsText {
            text: nameRole
            color: isMissing ? XsStyleSheet.hintColor : XsStyleSheet.primaryTextColor
            Layout.fillHeight: true
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            font.pixelSize: XsStyleSheet.fontSize + 1
            leftPadding: XsStyleSheet.panelPadding / 2
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            XsToolTip {
                text: nameRole
                visible: hovered && parent.truncated
                x: 0 //#TODO: flex/pointer
                y_reposition: -1.0
            }

        }

        Repeater {
            Layout.fillHeight: true
            model: notificationRole

            // notify widget
            XsNotification {
                Layout.fillHeight: true
                Layout.preferredWidth: height
                text: modelData.text
                type: modelData.type
                percentage: modelData.progress_percent || 0.0
            }
        }

        XsIcon {
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Layout.margins: 2

            source: "qrc:/icons/desktop_windows.svg"
            visible: isViewed
            imgOverlayColor: XsStyleSheet.accentColor
        }

        Repeater {
            model: decoratorModel

            XsPrimaryButton {
                Layout.margins: 2
                Layout.fillHeight: true
                Layout.preferredWidth: height

                imgSrc: modelData[1].icon || modelData[1]
                background: Item {}
                imgOverlayColor: XsStyleSheet.hintColor
                toolTip: modelData[1].tooltip || ""
            }
        }

        XsText {
            Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth + 5
            Layout.preferredHeight: XsStyleSheet.secondaryButtonStdWidth

            text: mediaCountRole ? mediaCountRole : 0
            color: XsStyleSheet.hintColor
        }

        Item {
            Layout.preferredWidth: errorIndicator.visible ? XsStyleSheet.secondaryButtonStdWidth : 0
            Layout.preferredHeight: errorIndicator.visible ? XsStyleSheet.secondaryButtonStdWidth : 0

            XsSecondaryButton{
                id: errorIndicator
                anchors.fill: parent
                visible: errorRole != 0
                imgSrc: "qrc:/icons/error.svg"
                imgOverlayColor: XsStyleSheet.hintColor

                toolTip.text: errorRole +" errors"
                toolTip.visible: hovered
            }
        }
    }

    XsPlaylistItemDragDropHandler {
        id: drag_drop_handler
    }
}