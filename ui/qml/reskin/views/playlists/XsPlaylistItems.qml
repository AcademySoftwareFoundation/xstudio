// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

XsListView { id: playlist

    model: playlistsModel

    property var itemsDataModel: null

    property real itemRowWidth: 200
    property real itemRowStdHeight: XsStyleSheet.widgetStdHeight -2

    DelegateModel {
        id: playlistsModel

        // this is required as "model" doesn't issue notifications on change
        property var notifyModel: theSessionData

        // we use the main session data model
        model: notifyModel

        // point at session 0,0, it's children are the playlists.
        rootIndex: notifyModel.index(0, 0, notifyModel.index(-1, -1))
        delegate: chooser
    }

    DelegateChooser {
        id: chooser
        role: "typeRole"

        DelegateChoice {
            roleValue: "ContainerDivider";

            XsPlaylistDividerDelegate{
                width: itemRowWidth
                height: itemRowStdHeight +(4+1)
            }
        }
        DelegateChoice {
            roleValue: "Playlist";

            XsPlaylistItemDelegate{
                width: itemRowWidth
                modelIndex: playlistsModel.modelIndex(index)
            }
        }

    }

}
