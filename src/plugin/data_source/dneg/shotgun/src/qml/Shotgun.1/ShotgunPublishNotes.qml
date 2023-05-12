// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import Qt.labs.qmlmodels 1.0 //for DelegateChooser
import xstudio.qml.helpers 1.0

import xStudio 1.1


XsWindow {
    id: publishNotesDialog

    title: "Publish Notes"

    width: minimumWidth
    minimumWidth: 400
    // maximumWidth: app_window.width

    height: minimumHeight
    minimumHeight: 530
    maximumHeight: 550

    centerOnOpen: true

    property int itemHeight: 22
    property int itemSpacing: 4

    property int padding: 6

    property int notesCount: (payload_obj ? payload_obj["payload"].length : 0)
    onNotesCountChanged:{
        if(notesCount===1) countDisplay.text = "Ready to publish " +notesCount+" note."
        else if(notesCount>1) countDisplay.text = "Ready to publish " +notesCount+" notes."
        else countDisplay.text = "No notes to publish."
    }

    property var projectModel: null
    property var playlists: null
    property var groups: null

    property var playlist_uuid: null

    property var payload: null
    property var payload_obj: null

    property var data_source: null
    property var publish_func: null

    property alias notify_owner: notify_owner_cb.checked
    property alias combine: combine_cb.checked
    property alias add_time: add_time_cb.checked
    property alias add_playlist_name: add_name_cb.checked
    property alias add_type: add_type_cb.checked
    property alias ignore_with_only_drawing: ignore_with_only_drawing_cb.checked
    property string default_type: overrideType.checked ? overrideType.defaultType : ""

    property int notify_group_id: notify_group_cb.currentIndex !==-1 && notify_group_cb.checked && notify_group_cb.model ? notify_group_cb.model.get(notify_group_cb.currentIndex, "idRole") : 0
    // property var notify_group_ids: notify_group_cb.model ? notify_group_cb.checkedIndexes:0 //#TODO



    XsModelNestedPropertyMap {
        id: prefs
        index: app_window.globalStoreModel.search_recursive("/plugin/data_source/shotgun/note_publish_settings", "pathRole")
        property alias properties: prefs.values
    }


    onPlaylist_uuidChanged: {
        // set combo to new value.
        for(let i =0; i<playlists.length; i++) {
            if(playlists[i].uuid == playlist_uuid && i != playlist_cb.currentIndex) {
                playlist_cb.currentIndex = i
                updatePublish()
                break
            }
        }
    }

    onPayloadChanged: {
        payload_obj = JSON.parse(payload)

        // find project id..
        if(payload_obj && payload_obj["payload"].length) {
            let id = payload_obj["payload"][0]["payload"]["project"]["id"]
            if(publishNotesDialog.projectModel) {
                projectsCombo.currentIndex = publishNotesDialog.projectModel.search(id, "idRole")
            }
        }
    }

    function getNotifyGroups() {
        let result = []
        if(notify_group_cb.checked) {
            for(let i =0;i<notify_group_cb.checkedIndexes.length;i++) {
                result.push(notify_group_cb.model.get(notify_group_cb.checkedIndexes[i], "idRole"))
            }
        }
        return result
    }

    function updatePublish() {
        if(playlist_uuid) {
            publishNotesDialog.payload = data_source.preparePlaylistNotes(
                playlist_uuid,
                notify_owner,
                getNotifyGroups(),
                combine,
                add_time,
                add_playlist_name,
                add_type,
                ignore_with_only_drawing,
                default_type
            )
        }

        hideMenu()
    }

    function publishNotes() {

        // onAccepted: push_playlist_note_promise(data_source, payload, playlist, playlist_uuid, error)

        if(playlist_uuid) {
            let tmp = data_source.preparePlaylistNotes(
                playlist_uuid,
                notify_owner,
                getNotifyGroups(),
                combine,
                add_time,
                add_playlist_name,
                add_type,
                ignore_with_only_drawing,
                default_type
            )

            publishNotesDialog.payload = tmp
            // console.log(tmp)
            publish_func(tmp, playlist_uuid)
        // onAccepted: push_playlist_note_promise(data_source, payload, playlist, playlist_uuid, error)
        }
    }

    function hideMenu(){
        notify_group_cb.hide()
    }

    XsFrame{
        id: frame

        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            visible: notify_group_cb.popup.visible
            onClicked: hideMenu()
        }

        ColumnLayout {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: buttonsPanel.top
            anchors.leftMargin: padding * 2
            anchors.rightMargin: padding * 2
            spacing: itemSpacing

            Item {
                Layout.minimumHeight: padding
                Layout.maximumHeight: padding
                Layout.fillWidth: true
            }

            XsComboBoxWithText{
                id: projectsCombo
                text: "Select Project"
                model: publishNotesDialog.projectModel
                editable: true
                textRole: "display"

                onCurrentIndexChanged: {
                    if(currentIndex !== -1){
                        notify_group_cb.model = data_source.groupModel(publishNotesDialog.projectModel.get(currentIndex, "idRole"))
                        notify_group_cb.model.sortRoleName = "nameRole"
                        notify_group_cb.model.sortAscending = true
                    }
                    hideMenu()
                }

                Layout.fillWidth: true
                Layout.minimumHeight: itemHeight*2
                Layout.maximumHeight: itemHeight*2
            }

            XsComboBoxWithText{
                id: playlist_cb
                text: "Select xStudio Playlist"
                model: publishNotesDialog.playlists
                editable: true
                textRole: "text"
                onCurrentIndexChanged: {
                    if(publishNotesDialog.visible && playlist_uuid != model[currentIndex].uuid) {
                        playlist_uuid = model[currentIndex].uuid
                        updatePublish()
                    }
                    hideMenu()
                }
                Layout.fillWidth: true
                Layout.minimumHeight: itemHeight*2
                Layout.maximumHeight: itemHeight*2
            }

            Item {
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.fillWidth: true
            }


            XsCheckBoxWithComboBox {
                id: overrideType
                Layout.fillWidth: true
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight

                property string defaultType: prefs.values.defaultType == undefined  ? "Default" : prefs.values.defaultType
                editable: true
                text: "Default Type: "
                model: app_window.bookmark_categories
                textRole: "textRole"
                currentIndex: model.search(defaultType).row

                font.family: XsStyle.controlTitleFontFamily
                font.hintingPreference: Font.PreferNoHinting
                onCurrentIndexChanged: {
                    defaultType = model.get(model.index(currentIndex,0), "valueRole")
                    prefs.values.defaultType = defaultType
                    hideMenu()
                }

                onCheckedChanged: {
                    if(checked) {
                        prefs.values.defaultType = defaultType
                    } else {
                        prefs.values.defaultType = null
                    }

                }

                checked: {
                    prefs.values.defaultType !== undefined
                }
            }

            Item {
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.fillWidth: true
            }

            XsCheckbox{
                id: notify_owner_cb
                checked: prefs.values.notifyCreator
                text: "Notify version creator"
                Layout.minimumWidth: textItem.contentWidth + indicatorItem.width
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                onCheckedChanged: {
                    updatePublish()
                    prefs.values.notifyCreator = checked
                }
            }

            XsCheckBoxWithMultiComboBox{
                id: notify_group_cb
                z: 1
                text: "Notify: "
                model: null
                hint: "Recipients"
                Layout.fillWidth: true
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
            }

            Item {
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.fillWidth: true
            }


            XsCheckbox{
                id: combine_cb
                text: "Combine multiple notes"
                checked: prefs.values.combine
                Layout.minimumWidth: textItem.contentWidth + indicatorItem.width
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                onCheckedChanged: {
                    updatePublish()
                    prefs.values.combine = checked
                }
            }

            XsCheckbox{
                id: add_time_cb
                text: "Add \"Frame/Timecode Number\" to notes"
                checked: prefs.values.addFrame
                Layout.minimumWidth: textItem.contentWidth + indicatorItem.width
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                onCheckedChanged: {
                    prefs.values.addFrame = checked
                    hideMenu()
                }
            }

            XsCheckbox{
                id: add_name_cb
                text: "Add \"Playlist Name\" to notes"
                checked: prefs.values.addPlaylistName
                Layout.minimumWidth: textItem.contentWidth + indicatorItem.width
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                onCheckedChanged: {
                    prefs.values.addPlaylistName = checked
                    hideMenu()
                }
            }

            XsCheckbox{
                id: add_type_cb
                checked: prefs.values.addType
                text: "Add \"Note Type\" to notes"
                Layout.minimumWidth: textItem.contentWidth + indicatorItem.width
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                onCheckedChanged: {
                    prefs.values.addType = checked
                    hideMenu()
                }
            }

            Item {
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.fillWidth: true
            }


            XsCheckbox{
                id: ignore_with_only_drawing_cb
                checked: prefs.values.ignoreEmpty
                text: "Ignore notes with only drawings"
                onCheckedChanged: {
                    updatePublish()
                    prefs.values.ignoreEmpty = checked
                }
                Layout.minimumWidth: textItem.contentWidth + indicatorItem.width
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
            }

            Item {
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.fillWidth: true
            }

            XsText{
                color: XsStyle.highlightColor
                text: "Only notes attached Shotgun Media are currently supported."
                font.bold: true

                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.fillWidth: true
            }

            XsText{ id: countDisplay
                text: "No notes to publish."
                topPadding: -itemHeight/2
                font.italic: true
                height: itemHeight

                Layout.minimumHeight: itemHeight
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

        }

        Rectangle{ id: buttonsPanel
            color: "transparent"

            anchors.bottom: parent.bottom
            anchors.bottomMargin: padding

            width: parent.width
            height: itemHeight

            XsButton{id: cancelButton
                x: padding

                width: (parent.width - itemSpacing)/2 - padding
                height: itemHeight

                text: "Cancel"
                onClicked: publishNotesDialog.hide()
            }

            XsButton{
                x: cancelButton.x + cancelButton.width + itemSpacing

                width: (parent.width - itemSpacing)/2 - padding
                height: itemHeight

                text: "Publish Notes To SG"
                enabled: notesCount
                onClicked: {
                    publishNotesDialog.hide()
                    publishNotes()
                }
            }
        }
    }
}
