// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

import xStudioReskin 1.0

Rectangle{ id: header
    width: parent.width
    height: XsStyleSheet.widgetStdHeight

    color: XsStyleSheet.widgetBgNormalColor

    property bool isSomeColumnResizedByDrag: false
    property alias model: repeater.model

    property alias columns_model: columns_model

    XsMediaListColumnsModel {
        id: columns_model
    }

    property var metadataPaths: []

    onMetadataPathsChanged: {
        // this call means that Media items in the session model will
        // have metadataSet0Role data consisting of an array of strings
        // corresponding to the metadata values found at the paths defined
        // by the array metadataPaths
        theSessionData.updateMetadataSelection(0, metadataPaths)
    }

    RowLayout{ id: titleBar
        // width: parent.width
        height: parent.height
        spacing: 0

        Repeater{ 
            
            id: repeater
            model: columns_model

            XsMediaHeaderColumn{
                
                //width: size

                // preferredWidth
                Layout.preferredWidth: size
                Layout.minimumHeight: XsStyleSheet.widgetStdHeight 
                
                // Why this ... ?
                z: columns_model.length-index

                titleBarTotalWidth: titleBar.width

                onHeaderColumnResizing:{
                    if(isDragged) {
                        isSomeColumnResizedByDrag = true
                    }
                    else {
                        isSomeColumnResizedByDrag = false
                    }
                }

                property var metadataPath: metadata_path
                // the 'metadata_path' property is injected from the 'columns_model'
                // data model. Here we update the metadataPaths property which
                // will in turn update the global session model data so it knows
                // how to populate the dynamic metadata fields for Media items.
                //
                // Each column in the media list view has a metadata path ... this
                // will define the metadata fetched for the Nth array item in 
                // the 
                onMetadataPathChanged: {
                    var _paths = metadataPaths;
                    while (_paths.length < index) {
                        _paths.push("")
                    }
                    _paths[index] = metadata_path ? metadata_path : ""
                    metadataPaths = _paths
                }

            }

        }

    }

}