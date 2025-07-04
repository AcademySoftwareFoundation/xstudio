// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import ShotBrowser 1.0

XsListView { id: list
    spacing: panelPadding
    property int rightSpacing: list.height < list.contentHeight ? 12 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}

    ScrollBar.vertical: XsScrollBar {
        visible: list.height < list.contentHeight
        parent: list.parent
        anchors.top: list.top
        anchors.right: list.right
        anchors.bottom: list.bottom
    }

    XsSBMediaPlayer {
        id: mediaPlayer
        anchors.fill: parent
        visible: false
        onEject: visible = false
    }

    function playMovie(path) {
        mediaPlayer.loadMedia(path)
        mediaPlayer.visible = true
        mediaPlayer.playToggle()
    }


    XsLabel {
        text: "Select the 'Scope' to view the Shot History."
        color: XsStyleSheet.hintColor
        visible: !activeScopeIndex.valid //#TODO

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    XsLabel {
        text: !queryRunning ? "No Results Found" : ""
        // text: isPaused ? "Updates Paused" : !queryRunning ? "No Results Found" : ""
        color: XsStyleSheet.hintColor
        visible: dataModel &&  activeScopeIndex.valid && !dataModel.count //#TODO

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    model: DelegateModel {
        id: chooserModel
        model: dataModel
        delegate: ShotHistoryListDelegate{
            width: list.width - rightSpacing
            height: XsStyleSheet.widgetStdHeight * 4
            delegateModel: chooserModel
            popupMenu: resultPopup
            onPlayMovie: (path) => list.playMovie(path)
        }
    }
}

