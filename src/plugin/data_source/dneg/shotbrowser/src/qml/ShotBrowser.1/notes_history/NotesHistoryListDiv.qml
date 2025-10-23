// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic


import xStudio 1.0
import ShotBrowser 1.0

XsListView{
    id: list
    anchors.fill: parent
    spacing: panelPadding

    property int rightSpacing: list.height < list.contentHeight ? 12 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}
    readonly property int cellHeight: (XsStyleSheet.widgetStdHeight * 8) + 5

    property alias delegateModel: chooserModel

    ScrollBar.vertical: XsScrollBar {
        visible: list.height < list.contentHeight
        parent: list.parent
        anchors.top: list.top
        anchors.right: list.right
        anchors.bottom: list.bottom
    }


    XsSBImageViewer {
        id: imagePlayer
        anchors.fill: parent
        visible: false
        onEject: visible = false
    }

    function viewImages(images) {
        imagePlayer.images = images
        imagePlayer.visible = true
    }

    XsLabel {
        text: "Select the 'Scope' and the 'Note Type' to view the Note History."
        color: XsStyleSheet.hintColor
        visible: !activeScopeIndex.valid || !activeTypeIndex.valid

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    XsLabel {
        text: !queryRunning ? "No Results Found" : ""
        // text: isPaused ? "Updates Paused" : !queryRunning ? "No Results Found" : ""
        color: XsStyleSheet.hintColor
        visible: dataModel && activeScopeIndex.valid && activeTypeIndex.valid && !dataModel.count //#TODO

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    DelegateModel {
        id: chooserModel
        model: dataModel
        delegate: NotesHistoryListDelegate{
            property bool wasHovered: false
            width: list.width - rightSpacing

            onIsHoveredChanged: {
                if(isHovered)
                    wasHovered = true
            }
            height: cellHeight + (wasHovered ? Math.max(textHeightDiff, 0) : 0)

            delegateModel: chooserModel
            popupMenu: resultPopup
            onShowImages: (images) => list.viewImages(images)
        }
    }

    model: chooserModel
}
