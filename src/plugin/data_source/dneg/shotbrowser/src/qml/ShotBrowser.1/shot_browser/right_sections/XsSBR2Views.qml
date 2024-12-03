// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0

XsListView{ id: listDiv
    spacing: XsStyleSheet.panelPadding

    property int rightSpacing: listDiv.height < listDiv.contentHeight ? 16 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}

    XsLabel {
        text: !queryRunning ? (presetsSelectionModel.hasSelection ? "No Results Found" : "Select a preset on left to view the results") : ""
        color: XsStyleSheet.hintColor
        visible: results.count == 0

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    property bool isPlaylist: resultsBaseModel.entity == "Playlists"

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
                }
            }

            DelegateChoice{
                roleValue: "Note"

                NotesHistoryListDelegate{
                    width: listDiv.width - rightSpacing
                    height: XsStyleSheet.widgetStdHeight*8
                    delegateModel: chooserModel
                    popupMenu: noteResultPopup
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
