// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

XsListView { 
    
    id: mediaList
    model: mediaListModelData

    property var columns_model: null

    property real itemRowHeight: 0
    property real itemRowWidth: 0

    XsMediaListModelData {           
        id: mediaListModelData
        delegate: chooser
    }

    DelegateChooser {

        id: chooser
        role: "typeRole"

        DelegateChoice {

            roleValue: "Media"; 

            XsMediaSourceSelector {

                width: itemRowWidth
                height: itemRowHeight
                media_index_in_playlist: index
                media_item_model_index: theSessionData.index(index, 0, mediaListModelData.rootIndex)
                columns_model: mediaList.columns_model
                
            } 
        }
    }    
}