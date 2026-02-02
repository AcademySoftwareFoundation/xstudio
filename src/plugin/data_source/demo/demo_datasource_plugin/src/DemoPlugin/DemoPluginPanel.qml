// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// this import allows us to use the DemoPluginDatamodel
import demoplugin.qml 1.0

Item{

    id: panel
    anchors.fill: parent

    // Background to match xSTUDIO theme - this is used in most xSTUDIO panels
    // and widgets but is, of course, optional!
    XsGradientRectangle{
        anchors.fill: parent
    }

    // here is our instance of the data model to drive the UI - it's an
    // instance of the 'DataModel' defined in demo_plugin_ui.hpp
    DemoPluginDatamodel {
        id: dataModel
        property var root_index: dataModel.index(-1, -1)
        onModelReset: {
            root_index = dataModel.index(-1, -1)
        }
    }

    /* This connects to the backend model data named demo_plugin_attributes to which
    we can add backend plugin attributes*/
    XsModuleData {
        id: demo_plugin_attrs
        modelDataName: "demo_plugin_attributes"
    }

    ///////////////////////////////////////////////////////////////////////
    // to DIRECTLY expose attribute role data we use XsAttributeValue and
    // give it the title (name) of the attribute. By default it will expose
    // the 'value' role data of the attribute but you can override this to
    // get to other role datas such as 'combo_box_options' (the string
    // choices in a StringChoiceAttribute) or 'default_value' etc.

    XsAttributeValue {
        id: __current_project
        attributeTitle: "Current Project"
        model: demo_plugin_attrs
    }
    property alias current_project: __current_project.value

    XsAttributeValue {
        id: __projects_list
        attributeTitle: "Current Project"
        model: demo_plugin_attrs
        role: "combo_box_options"
    }
    property alias projects_list: __projects_list.value

    /*XsFilterModel {
        id: filterModel
        sourceModel: dataModel
        sortRoleName: "sequence"
        onSourceModelChanged: {
            //setRoleFilter(["ABC123", "FORTRESS"], "job")
        }
        rootIndex: current_show_index

    }*/

    property var indent: 50
    property var current_show_index: dataModel.index(0, 0, dataModel.root_index)
    property var gridLineColour: XsStyleSheet.menuDividerColor
    property var sequenceIsSelected: false

    // This ItemSelectionModel manages selection of the render items in our
    // dataset
    ItemSelectionModel {
        id: demoSelectionModel
        model: dataModel
        onSelectedIndexesChanged: {
            // communicate with the C++ plugin that the user has changed the
            // selection of sequences and shots
            dataModel.setSelection(selectedIndexes)
        }
    }
    property alias demoSelectionModel: demoSelectionModel

    ColumnLayout {

        anchors.fill: parent
        Layout.margins: 4
        spacing: 0

        // Load buttons
        RowLayout {
            spacing: 8
            Layout.margins: 4
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            XsText {
                text: "Select Production:"
            }
            XsComboBox {
                Layout.margins: 4
                Layout.preferredHeight: 24
                model: projects_list
                onCurrentIndexChanged: {
                    current_project = projects_list[currentIndex]
                }
                currentIndex: projects_list.indexOf(current_project)
                textField.horizontalAlignment: Text.alignRight
            }
        }

        Item {

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4

            SequenceShotTree {
                id: shot_tree
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: divider.left
                production_index: 0
            }

            Item {
                id: divider
                width: 10
                x: parent.width*0.3 - width/2
                anchors.top: parent.top
                anchors.bottom: parent.bottom                
                property real percent: 50
                Rectangle {
                    height: parent.height
                    x: parent.width/2-width/2
                    width: 2
                    color: ma2.pressed ? "white" : ma2.containsMouse ? palette.highlight : XsStyleSheet.menuDividerColor
                }
                MouseArea {
                    id: ma2
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    drag.target: divider
                    drag.axis: Drag.XAxis
                }
            }

            LoadableAssetsList {
                id: asset_list
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: divider.right
                anchors.right: parent.right
                tree_selection: demoSelectionModel.selectedIndexes
            }
        }

        // Load buttons
        RowLayout {
            spacing: 2
            Layout.margins: 4
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            XsSimpleButton {
                id: first_button
                Layout.fillWidth: true
                text: "Add to Current Playlist"
                // currentMediaContainerIndex is exposed by top level UI item in
                // xSTUDIO. See notes further down
                enabled: currentMediaContainerIndex.valid && asset_list.can_load_media
                onClicked: {
                    add_selection_to_playlist(false)
                }
            }
            XsSimpleButton {
                Layout.fillWidth: true
                text: "Add to New Playlist"
                enabled: asset_list.can_load_media
                onClicked: {
                    add_selection_to_playlist(true)
                }
            }
            XsSimpleButton {
                Layout.fillWidth: true
                text: "Load Sequence"
                enabled: asset_list.can_load_sequence
                onClicked: {
                    load_selected_sequences()
                }

            }
        }
    }

    function add_to_new_playlist(new_name, button) {
        if (button == "Add") {
            add_selection_to_playlist(false, new_name)
        }
    }

    function load_selected_sequences() {

        // Now we have the index to the item to add media to, get the
        // uuids of the items to add
        var sequence_uuids_to_add = asset_list.selected_sequences()
        if (sequence_uuids_to_add.length > 1) {
            // pack the array into a single value, otherwise python
            // callback function will unpack the array into multiple
            // arguments
            sequence_uuids_to_add = [sequence_uuids_to_add]
        }

        // Here's yet another mechanism for interacting with the backend.
        // In this case we will run a python callback by specifying the 
        // name of the python plugin and the method to execute. Our arguments
        // must be packed into a valid JSON list to match the arguments 
        // of the python plugin method.
        Future.promise(
            helpers.pythonAsyncCallback("DemoPluginPython", "loadSequences", sequence_uuids_to_add)
        ).then(function(sequence_object_ids) {
            // python plugin should have generated playlist with child sequence objects. It has
            // returned the UUIDs of the sequence objects. Let's use the first one to select the
            // corresponding object and put it on screen in xSTUDIO (it's also easy to do this
            // via the Python API)
            if (sequence_object_ids.length) {
                let seq_id = sequence_object_ids[0]
                // we can recursively search the session data model to find the index of the
                // sequence object
                let seq_index = theSessionData.searchRecursive(seq_id, "actorUuidRole")
                if (seq_index.valid) {
                    // set the index for the 'current' container (what shows in the MediaList
                    // and the Timeline interface)
                    currentMediaContainerIndex = seq_index
                    // set the index for the viewport container (what shows in the Viewport)
                    viewportCurrentMediaContainerIndex = seq_index
                }

            }
        },
	    function() {
            console.log("ERROR", __rr)
	    })

       
    }

    function add_selection_to_playlist(get_new_name, name, put_on_screen) {
        if (get_new_name || name == undefined && !currentMediaContainerIndex.valid) {
            // we use the get text helper to get a name from the user
            dialogHelpers.textInputDialog(
                add_to_new_playlist,                   // callback for result
                "Add To New Playlist",                      // title
                "Enter New Playlist Name.",                 // instructions
                theSessionData.getNextName("Playlist {}"),  // default text
                ["Cancel", "Add"])                          // Buttons
            return
        }
        var playlistIndex
        if (name != undefined) {
            // 'theSessionData' is the main datamodel for xSTUDIO and allows
            // us access to playlists, playlist children, media lists, timelines#
            // etc. It also has utility functions for creation of playlists etc.
            playlistIndex = theSessionData.createPlaylist(name)
        } else {
            // 'currentMediaContainerIndex' is made available at the top level
            // UI Item in xSTUDIO - this is an index into 'theSessionData' for
            // the current selected playlist, subset, contact sheet etc that
            // is showing its contents in the MediaList
            playlistIndex = currentMediaContainerIndex
        }

        // Now we have the index to the item to add media to, get the
        // uuids of the items to add
        var media_uuids_to_add = asset_list.selected_media()

        // Now we're going to do a SLOW database query. This demonstrates how to
        // use QFuture mechanism to run a callback some time later when a method
        // is executed asynchronously
        Future.promise(
            dataModel.getRecordsByUuid(media_uuids_to_add)
        ).then(function(selected_media_records) {

            // Now we are going to run a callback in our backend plugin with
            // the necessary data to add the new media to our playlist.            

            // we also need to put the target playlist ID into our message
            var message_data = {}
            message_data["playlist_id"] = theSessionData.get(playlistIndex, "actorUuidRole")

            message_data["media_to_add"] = []
            for (var i = 0; i < selected_media_records.length; ++i) {
                message_data["media_to_add"].push(selected_media_records[i]);
            }
            message_data["action"] = "LOAD_MEDIA_INTO_PLAYLIST"

            if (put_on_screen) {
                message_data["put_on_screen"] = true;
            }

            // For this example, we will use this helper function to trigger
            // a callback to DemoPlugin::qml_item_callback in our plugin backend.
            // The helper requires the UUID of our plugin and QVariant argument
            // (which is converted to json when passed into xstudio)
            helpers.pluginCallback("28813519-aa6e-42a5-a201-a55f07136565", message_data)

            // Note that there are other ways to load media and build playlists,
            // you can do it all from QML by interacting with the 'theSessionData'
            // object.


        })

    }

}