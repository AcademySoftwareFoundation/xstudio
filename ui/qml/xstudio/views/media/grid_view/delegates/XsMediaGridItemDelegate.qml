// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtGraphicalEffects 1.15
import QtQml.Models 2.14
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "."
import "../../common_delegates"
import "../../functions"
import "../../widgets"
import "../../list_view/data_indicators"

Rectangle{

    id: gridItem

    width: cellSize-10
    height: width / (16/9) //to keep 16:9 ratio
    color: "transparent"

    property real itemSpacing: 1
    property real headerThumbWidth: 1
    property real itemPadding: XsStyleSheet.panelPadding/2

    property bool isActive: isOnScreen
    property bool isSelected: false
    property bool isDragTarget: false
    property bool isHovered: toolTipMArea.containsMouse
    property bool showDetails: cellSize > standardCellSize

    property var gotBookmark: false
    property var gotBookmarkAnnotation: false
    property var gotBookmarkGrade: false
    property var gotBookmarkTransform: false
    property var gotBookmarkUuids_: bookmarkUuidsRole

    onGotBookmarkUuids_Changed: {
        var bm = false
        var anno = false
        var grade = false
        if (bookmarkUuidsRole && bookmarkUuidsRole.length) {
            for (var i in bookmarkUuidsRole) {
                var idx = bookmarkModel.searchRecursive(bookmarkUuidsRole[i], "uuidRole")
                if (idx.valid) {
                    if(bookmarkModel.get(idx,"userTypeRole") == "Grading") {
                        grade = true
                    } else if (bookmarkModel.get(idx,"hasAnnotationRole")) {
                        anno = true
                    }
                    if(bookmarkModel.get(idx,"noteRole") != "")
                        bm = true
                }
            }
        }
        gotBookmark = bm
        gotBookmarkGrade = grade
        gotBookmarkAnnotation = anno
    }

    // these are referenced by XsMediaThumbnailImage and XsMediaListMouseArea
    property real mouseX
    property real mouseY
    property bool hovered: false


    property bool playOnClick: false
    property bool isOnScreen: actorUuidRole == currentPlayhead.mediaUuid

    property var actorUuidRole__: actorUuidRole
    onActorUuidRole__Changed: setSelectionIndex( modelIndex() )

    Connections {
        target: mediaSelectionModel
        function onSelectedIndexesChanged() {
            setSelectionIndex( modelIndex() )
        }
    }




    /**************************************************************
    Item Functions
    ****************************************************************/

    XsMediaItemFunctions {
        id: functions
    }

    property var setSelectionIndex: functions.setSelectionIndex
    property var toggleSelection: functions.toggleSelection
    property var exclusiveSelect: functions.exclusiveSelect
    property var inclusiveSelect: functions.inclusiveSelect

    property int selectionIndex: 0

    function modelIndex() {
        return helpers.makePersistent(mediaListModelData.rowToSourceIndex(index))
    }


    property color highlightColor: palette.highlight
    property color bgColorPressed: Qt.darker(palette.highlight, 3)
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor

    Rectangle {
        id: itemBg
        anchors.fill: tileItems //parent
        border.color: hovered ? highlightColor : mediaStatusRole && mediaStatusRole != "Online" ? "red" : "transparent"
        border.width: 1
        color: isSelected ? bgColorPressed : bgColorNormal
    }

    Rectangle {
        id: drag_target_indicator
        width: visible ? 5 : 0
        height: visible ? parent.height : 0

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: tileItems.left
        anchors.rightMargin: visible ? 5*2 : 5
        color: palette.highlight
        Behavior on anchors.rightMargin { enabled: drag_drop_handler.dragging; NumberAnimation{duration: 250} }

        visible: isDragTarget
        z: 11
    }
    Rectangle {
        id: drag_item_bg
        anchors.fill: parent
        visible: dragTargetIndex != undefined && isSelected
        opacity: 0.5
        color: "white"
        z: 10
    }



    ColumnLayout{ id: tileItems
        width: isDragTarget? parent.width-drag_target_indicator.width-drag_target_indicator.anchors.rightMargin
            : parent.width
        height: parent.height
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        Behavior on width { enabled: drag_drop_handler.dragging; NumberAnimation{duration: 250} }
        spacing: itemSpacing

        RowLayout{
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: width
            Layout.margins: itemSpacing
            spacing: 0 //itemSpacing

            Rectangle{ id: thumbDiv
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: showDetails? 0 : 1
                color: "transparent"
                border.width: 2
                border.color: XsStyleSheet.panelBgColor
                clip: true

                XsMediaThumbnailImage { id: thumb
                    anchors.fill: parent
                    showBorder: isOnScreen
                    highlightBorderThickness: 10
                }
                Rectangle{ id: flagIndicator
                    width: 5 * cellSize/standardCellSize
                    height: parent.height
                    color: flagColourRole == undefined ? "transparent" : flagColourRole
                }
                Rectangle{
                    anchors.left: thumb.left
                    anchors.leftMargin: -width/2
                    anchors.top: thumb.top
                    anchors.topMargin: -width/2
                    color: isSelected ? palette.highlight : "black"
                    width: 50
                    height: width
                    // radius: width/2
                    border.width: 0
                    border.color: palette.text
                    scale: cellSize/standardCellSize
                    opacity: isSelected? 0.9 : 0.5
                    clip: true
                    rotation: -45

                    visible: indexDiv.text != ""
                }
                XsText{ id: indexDiv
                    text: selectionIndex ? selectionIndex : ""
                    anchors.left: thumb.left
                    anchors.leftMargin: showDetails? 5 : 2.5
                    anchors.top: thumb.top
                    anchors.topMargin: showDetails? 5 : 1.5
                    font.bold: true
                    // color: isSelected ? palette.highlight : palette.text

                    layer.enabled: true
                    layer.effect: DropShadow{
                        verticalOffset: 1
                        horizontalOffset: 1
                        color: "#010101"
                        radius: 1
                        samples: 3
                        spread: 0.5
                    }
                }

            }

            Rectangle{ id: iconsDiv
                Layout.preferredWidth: visible? divHeight<60? 60 : divHeight : 0
                Layout.maximumWidth: visible? divHeight : 0
                Layout.fillHeight: true
                color: "transparent"
                visible: showDetails //&& cellSize>130

                property real divHeight: (height- itemSpacing*4)/4

                ColumnLayout{
                    anchors.fill: parent
                    spacing: itemSpacing

                    property real divHeight: parent.divHeight

                    XsSecondaryButton {
                        enabled: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.divHeight
                        imgSrc: "qrc:/icons/sticky_note.svg"
                        isColoured: gotBookmark
                        onlyVisualyEnabled: gotBookmark
                    }
                    XsSecondaryButton {
                        enabled: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.divHeight
                        imgSrc: "qrc:/icons/brush.svg"
                        isColoured: gotBookmarkAnnotation
                        onlyVisualyEnabled: gotBookmarkAnnotation
                    }
                    XsSecondaryButton {
                        enabled: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.divHeight
                        imgSrc: "qrc:/icons/tune.svg"
                        isColoured: gotBookmarkGrade
                        onlyVisualyEnabled: gotBookmarkGrade
                    }
                    XsSecondaryButton {
                        enabled: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.divHeight
                        imgSrc: "qrc:/icons/open_with.svg"
                        isColoured: gotBookmarkTransform
                        onlyVisualyEnabled: gotBookmarkTransform
                    }

                }
            }

        }

        XsText{ id: fileNameDiv
            text: nameRole
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            Layout.minimumHeight: visible? divHeight/1.5 : 0
            Layout.preferredHeight: visible? divHeight/1.5 : 0
            Layout.maximumHeight: visible? divHeight/1.5 : 0
            // visible: cellSize > 140
            visible: showDetails

            property real divHeight: (btnHeight - 2)

            isHovered: toolTipMArea.containsMouse
            MouseArea{
                id: toolTipMArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
            }
        }

    }


}