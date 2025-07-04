// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.qmlmodels

import xStudio 1.0
import ShotBrowser 1.0

XsListView{ id: listDiv
    spacing: XsStyleSheet.panelPadding
    property int rightSpacing: listDiv.height < listDiv.contentHeight ? 12 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}
    property bool isPlaylist: resultsBaseModel.entity == "Playlists"

    readonly property int noteCellHeight: (XsStyleSheet.widgetStdHeight * 8) + 5

    ScrollBar.vertical: XsScrollBar {
        visible: listDiv.height < listDiv.contentHeight
        parent: listDiv
        anchors.top: listDiv.top
        anchors.right: listDiv.right
        anchors.bottom: listDiv.bottom
    }

    XsLabel {
        text: !queryRunning ? (presetsSelectionModel.hasSelection ? "No Results Found" : "Select a preset on left to view the results") : ""
        color: XsStyleSheet.hintColor
        visible: results.count == 0

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    XsSBMediaPlayer {
        id: mediaPlayer
        anchors.fill: parent
        visible: false
        onEject: visible = false
        z: 2
    }

    function playMovie(path) {
        if(path != "") {
            mediaPlayer.loadMedia(path)
            mediaPlayer.visible = true
            mediaPlayer.playToggle()
        }
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

    model: DelegateModel { id: chooserModel
        property var notifyModel: results
        onNotifyModelChanged: model = notifyModel
        model: notifyModel

        delegate: DelegateChooser{ id: chooser
            role: "typeRole"

            DelegateChoice{
                roleValue: "Version"

                  ShotHistoryListDelegate{
                    modelDepth: chooserModel.notifyModel.depthAtRow(index)
                    width: listDiv.width - rightSpacing
                    height: XsStyleSheet.widgetStdHeight*4
                    delegateModel: chooserModel
                    popupMenu: versionResultPopup
                    groupingEnabled: resultsBaseModel.isGrouped
                    isPlaylist: listDiv.isPlaylist
                    onPlayMovie: (path) => listDiv.playMovie(path)
                }
            }

            DelegateChoice{
                roleValue: "Note"

                NotesHistoryListDelegate{
                    width: listDiv.width - rightSpacing
                    height: noteCellHeight + (isHovered ? Math.max(textHeightDiff, 0) : 0)

                    delegateModel: chooserModel
                    popupMenu: noteResultPopup
                    onShowImages: (images) => listDiv.viewImages(images)
                }
            }

            DelegateChoice{
                roleValue: "Playlist"

                XsSBRPlaylistViewDelegate{
                    width: listDiv.width - rightSpacing
                    height: XsStyleSheet.widgetStdHeight*2
                    delegateModel: chooserModel
                    popupMenu: playlistResultPopup
                }
            }
        }
    }
}
