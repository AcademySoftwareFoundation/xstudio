// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

Item {

    id: selector

    property var media_item_model_index: null
    property var columns_model
    property var media_index_in_playlist

    width: itemRowWidth
    height: itemRowHeight

    // This Item is instanced for every Media object. Media objects contain
    // one or more MediaSource objects. We want instance a XsMediaItemDelegate 
    // for the 'current' MediaSource. So here we track the UUID of the 'image actor',
    // which is a property of the Media object and tells us the ID of the 
    // current MediaSource
    property var imageActorUuid: imageActorUuidRole

    // here we follow the Media object metadata fields. They are accessed deeper
    // down in the 'data_indicators' that are instanced by the 'XsMediaItemDelegate'
    // created here.
    property var metadataFieldValues: metadataSet0Role

    Repeater {
        model: 
        DelegateModel {

            // this DelegateModel is set-up to iterate over the contents of the Media
            // node (i.e. the MediaSource objects)
            model: media_item_model_index.model
            rootIndex: media_item_model_index
            delegate: 
            DelegateChooser {

                id: chooser

                // Here we employ a chooser and check against the uuid of the
                // MediaSource object
                role: "actorUuidRole"

                DelegateChoice {
                    // we only instance the XsMediaItemDelegate when the 
                    // MediaSource object uuid matches the imageActorUuid - 
                    // Hence we have filtered for the active MediaSource
                    roleValue: imageActorUuid
                    XsMediaItemDelegate {
                        width: selector.width
                        height: selector.height
                        columns_model: selector.columns_model                    
                    }
                }
            }
        }
    }
}