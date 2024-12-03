// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.14
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import QuickFuture 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

XsWindow{

    title: "Publish "+notesType+" Notes"
    property bool isPlaylistNotes: true
    property string notesType: isPlaylistNotes? "Playlist": "Selected Media"
    property string message: "No notes to publish."

    property real itemHeight: btnHeight
    property real itemSpacing: 1

    property int projectId: -1

    property bool invalidPublish: false

    property alias notifyOwner: notifyCreatorCB.checked
    property alias combineNotes: combineNotesCB.checked
    property alias addFrameTimeCode: addFrameTimeCodeCB.checked
    property alias addPlaylistName: addPlaylistNameCB.checked
    property alias addNoteType: addNoteTypeCB.checked
    property alias ignoreWithOnlyDrawing: ignoreWithOnlyDrawingCB.checked
    property alias skipAlreadyPublished: skipAlreadyPublishedCB.checked
    property string defaultType: typeRenameDiv.checked ? prefs.value.defaultType : ""
    property var playlistUuid: null
    property var mediaUuids: []

    property var payload: null
    property var payload_obj: null

    property int notesCount: (payload_obj ? payload_obj["payload"].length : 0)


    onNotesCountChanged:{
        if(notesCount===1) message = "Ready to publish " +notesCount+" note."
        else if(notesCount>1) message = "Ready to publish " +notesCount+" notes."
        else message = "No notes to publish." + projectId
    }

    onPayloadChanged: {
        try {
            payload_obj = JSON.parse(payload)

            // find project id..
            if(payload_obj && payload_obj["payload"].length) {
                let id = payload_obj["payload"][0]["payload"]["project"]["id"]
                projectDiv.currentIndex = projectDiv.model.search(id, "idRole").row
            }
            invalidPublish = false
        } catch(err) {
            if(isPlaylistNotes) {
                message = "Invalid Playlist, must have ShotGrid Metadata."
            }
            invalidPublish = true
            console.log(err)
        }
    }

    onPlaylistUuidChanged: {
        // update playlist combo, if required.
        for(let i = 0; i< theSessionData.playlists.length; i++) {
            let ustr = helpers.QUuidToQString(playlistUuid)
            if(theSessionData.playlists[i].uuid == ustr) {
                if(playlistDiv.currentIndex != i)
                    playlistDiv.currentIndex = i
                break
            }
        }

        updatePublish()
    }


    width: 400
    height: 660
    minimumWidth: 400
    // maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    // palette.base: XsStyleSheet.panelTitleBarColor

    XsSBPublishNotesFeedback {
        id: publish_notes_feedback
        property real btnHeight: XsStyleSheet.widgetStdHeight + 4
    }


    Connections {
        target: ShotBrowserEngine
        function onReadyChanged() {
            updatePublish()
            prefs.updateWidgets()
        }
    }

    function updatePublish() {
        if(visible) {
            // console.log("playlistUuid", playlistUuid)
            // console.log("notifyOwner", notifyOwner)
            // console.log("combineNotes", combineNotes)
            // console.log("addFrameTimeCode", addFrameTimeCode)
            // console.log("addPlaylistName", addPlaylistName)
            // console.log("addNoteType", addNoteType)
            // console.log("ignoreWithOnlyDrawing", ignoreWithOnlyDrawing)
            // console.log("skipAlreadyPublished", skipAlreadyPublished)
            // console.log("defaultType", defaultType)

            if(playlistUuid) {
                payload = ShotBrowserEngine.preparePlaylistNotes(
                    playlistUuid,
                    mediaUuids,
                    notifyOwner,
                    getNotifyGroups(),
                    combineNotes,
                    addFrameTimeCode,
                    addPlaylistName,
                    addNoteType,
                    ignoreWithOnlyDrawing,
                    skipAlreadyPublished,
                    defaultType
                )
            }
        }
    }

    function publishNotes() {

        if(playlistUuid) {
            payload = ShotBrowserEngine.preparePlaylistNotes(
                playlistUuid,
                mediaUuids,
                notifyOwner,
                getNotifyGroups(),
                combineNotes,
                addFrameTimeCode,
                addPlaylistName,
                addNoteType,
                ignoreWithOnlyDrawing,
                skipAlreadyPublished,
                defaultType
            )

            Future.promise(
                ShotBrowserEngine.pushPlaylistNotesFuture(payload, playlistUuid)
            ).then(function(json_string) {
                console.log(json_string)

                publish_notes_feedback.isPlaylistNotes = isPlaylistNotes
                publish_notes_feedback.parseFeedback(json_string)
                publish_notes_feedback.show()
            })
        }
    }


    function getNotifyGroups() {
        let result = []
        let email_group_names = []
        if(notifyGroupCB.checked) {
            for(let i =0;i<notifyGroupCB.checkedIndexes.length;i++) {
                result.push(notifyGroupCB.model.sourceModel.get(notifyGroupCB.checkedIndexes[i], "idRole"))
                email_group_names.push(notifyGroupCB.model.sourceModel.get(notifyGroupCB.checkedIndexes[i], "nameRole"))
            }
        }

        return result
    }

    function publishFromPlaylist(playlist_uuid) {
        isPlaylistNotes = true
        mediaUuids = []
        playlistUuid = playlist_uuid
    }

    function publishFromMedia(medialist) {
        isPlaylistNotes = false
        mediaUuids = []

        if(medialist.length) {
            let m = medialist[0].model
            let plind = m.getPlaylistIndex(medialist[0])

            for(let i = 0; i< medialist.length; i++) {
                mediaUuids.push(m.get(medialist[i], "actorUuidRole"))
            }
            playlistUuid = m.get(plind, "actorUuidRole")
        }
    }

    XsPreference {
        id: prefs
        path: "/plugin/data_source/shotbrowser/note_publishing/note_publish_settings"

        property string currentSetting: "Default Profile"
        property var settings: Object.keys(value.settings)
        onValueChanged: updateWidgets()

        function storePreference(key="", new_value="") {
            const omit = (data , ids) => {
                const idsSet = new Set(ids);
                const newObj = {};
                for (const [key, val] of Object.entries(data)) {
                    if (!idsSet.has(key)) {
                       newObj[key] = val;
                    }
                }
                return newObj;
            };

            let tmp = JSON.parse(JSON.stringify(value))
            if(key != "")
                tmp[key] = new_value

            if(!("settings" in tmp))
                tmp["settings"] = {}

            if(!("Default Profile" in tmp["settings"]))
                tmp["settings"]["Default Profile"] = omit(tmp, ["settings","__ignore__"])

            tmp["settings"][currentSetting] = omit(tmp, ["settings","__ignore__"])
            value = tmp
        }

        function addSetting(name) {
            currentSetting = name
            storePreference()
        }

        function createDefaultProfile() {
            const omit = (data , ids) => {
                const idsSet = new Set(ids);
                const newObj = {};
                for (const [key, val] of Object.entries(data)) {
                    if (!idsSet.has(key)) {
                       newObj[key] = val;
                    }
                }
                return newObj;
            };

            let tmp = JSON.parse(JSON.stringify(value))
            let changed = false
            if(!("settings" in tmp)) {
                tmp["settings"] = {}
                changed = true
            }

            if(!("Default Profile" in tmp["settings"])) {
                tmp["settings"]["Default Profile"] = omit(tmp, ["settings","__ignore__"])
                changed = true
            }

            if(changed)
                value = tmp
        }

        function changeCurrentSetting(name) {
            currentSetting = name
            let tmp = JSON.parse(JSON.stringify(value))

            for (const [key, val] of Object.entries(tmp["settings"][name])) {
                tmp[key] = val
            }

            value = tmp
            updateWidgets()
        }

        function removeSetting(name) {
            changeCurrentSetting("Default Profile")

            let tmp = JSON.parse(JSON.stringify(value))
            delete tmp["settings"][name]
            value = tmp

            storePreference()
        }

        function updateWidgets() {
            createDefaultProfile()

            typeRenameDiv.checked = value.defaultType != ""
            typeRenameDiv.currentIndex = typeRenameDiv.valueDiv.find(value.defaultType)

            // select indexes..
            let hasGroups = (value.notifyGroups !== undefined && value.notifyGroups.length ? true : false)
            if(hasGroups)
                notifyGroupCB.valueDiv.selectFromNames(value.notifyGroups)

            notifyGroupCB.checked = hasGroups
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        XsTextWithComboBoxFullSize{ id: projectDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight*1.5
            Layout.topMargin: itemHeight/2

            enabled: false

            text: "Select project :"
            model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Project") : []
            valueDiv.textRole: "nameRole"
            onCurrentIndexChanged: {
                if(currentIndex != -1) {
                    let pid = model.get(model.index(currentIndex,0), "idRole")
                    ShotBrowserEngine.cacheProject(pid)
                    projectId = pid
                }
            }
        }

        XsTextWithComboBoxFullSize{ id: playlistDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight*1.5
            Layout.topMargin: itemHeight/4

            enabled: isPlaylistNotes

            text: "Select XSTUDIO playlist :"
            valueDiv.textRole: "text"
            model: theSessionData.playlists
            onCurrentIndexChanged: {
                if(currentIndex != -1) {
                    if(model[currentIndex].uuid != playlistUuid)
                        playlistUuid = model[currentIndex].uuid
                }
                else
                    playlistUuid = ""
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight*1.5
            Layout.topMargin: itemHeight/2
            Layout.bottomMargin: itemHeight/2
            spacing: 1

            XsTextWithComboBoxFullSize{ id: settingsDiv
                Layout.fillWidth: true
                Layout.preferredHeight: itemHeight*1.5

                valueDiv.textColorNormal: model[currentIndex] == "Default Profile" ? XsStyleSheet.secondaryTextColor : palette.text

                text: "Profiles :"
                model: prefs.settings
                onActivated: prefs.changeCurrentSetting(model[currentIndex])
            }

            XsPrimaryButton {
                Layout.topMargin: itemHeight-2
                Layout.bottomMargin: 2
                Layout.preferredWidth: itemHeight
                Layout.preferredHeight: itemHeight
                imgSrc: "qrc:/icons/add.svg"

                function addSettingCallback(name, button) {
                    if(button == "Add Profile") {
                        prefs.addSetting(name)
                        settingsDiv.currentIndex = settingsDiv.model.length-1
                    }
                }

                onClicked: dialogHelpers.textInputDialog(
                        addSettingCallback,
                        "Add Profile",
                        "Enter a name for your profile.",
                        "",
                        ["Cancel", "Add Profile"])

            }

            XsPrimaryButton {
                Layout.topMargin: itemHeight-2
                Layout.bottomMargin: 2
                Layout.preferredWidth: itemHeight
                Layout.preferredHeight: itemHeight

                imgSrc: "qrc:/icons/delete.svg"
                enabled: prefs.currentSetting != "Default Profile"
                onClicked: prefs.removeSetting(prefs.currentSetting)
            }
        }


        XsTextWithCheckAndComboBoxes { id: typeRenameDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight
            Layout.topMargin: itemHeight


            valueDiv.textRole: "nameRole"
            text: "Rename all note types :"

            checked: false
            currentIndex: -1
            model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Note Type") : []

            onCurrentIndexChanged: {
                if(!checked && currentIndex != -1)
                    currentIndex = -1
            }

            onActivated: {
                if(index != -1) {
                    let dt = model.get(model.index(index, 0), "nameRole")
                    if(dt != prefs.value.defaultType)
                        prefs.storePreference("defaultType", dt)
                }
            }

            onCheckedChanged: {
                if(!checked) {
                    prefs.storePreference("defaultType", "")
                    currentIndex = -1
                }
            }
        }

        XsTextWithCheckBox { id: notifyCreatorCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight
            Layout.topMargin: itemHeight


            text: "Notify version creator"
            checked: prefs.value.notifyCreator
            onCheckedChanged: {
                if(prefs.value.notifyCreator != checked) {
                    updatePublish()
                    prefs.storePreference("notifyCreator", checked)
                }
            }
        }
        XsTextWithComboBoxMultiSelectable{ id: notifyGroupCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            z: 100
            text: "Notify :"
            hintText: "Recipients"
            checked: false
            model: ShotBrowserFilterModel {
                sourceModel: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Group", "", projectId) : null
            }

            onCheckedChanged: {
                if(!checked) {
                    valueDiv.theSelectionModel.clearSelection()
                }
            }

            onCheckedIndexesChanged: {
                let values = []
                for(let i =0;i<checkedIndexes.length;i++) {
                    values.push(model.sourceModel.get(checkedIndexes[i], "nameRole"))
                }
                prefs.storePreference("notifyGroups", values)
            }
        }

        XsTextWithCheckBox {
            id: combineNotesCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight
            Layout.topMargin: itemHeight

            text: "Combine multiple notes"
            checked: prefs.value.combine

            onCheckedChanged: {
                if(prefs.value.combine != checked) {
                    updatePublish()
                    prefs.storePreference("combine", checked)
                }
            }
        }

        XsTextWithCheckBox {
            id: addFrameTimeCodeCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            text: "Add 'Frame/Timecode Number' to notes"
            checked: prefs.value.addFrame
            onCheckedChanged: {
                if(prefs.value.addFrame != checked) {
                    updatePublish()
                    prefs.storePreference("addFrame", checked)
                }
            }
        }
        XsTextWithCheckBox {
            id: addPlaylistNameCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            text: "Add 'Playlist Name' to notes"
            checked: prefs.value.addPlaylistName
            onCheckedChanged: {
                if(prefs.value.addPlaylistName != checked) {
                    updatePublish()
                    prefs.storePreference("addPlaylistName", checked)
                }
            }
        }
        XsTextWithCheckBox {
            id: addNoteTypeCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            text: "Add 'Note Type' to notes"
            checked: prefs.value.addType
            onCheckedChanged: {
                if(prefs.value.addType != checked) {
                    updatePublish()
                    prefs.storePreference("addType", checked)
                }
            }
        }

        XsTextWithCheckBox {
            id: ignoreWithOnlyDrawingCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight
            Layout.topMargin: itemHeight

            text: "Ignore notes with only drawings"
            checked: prefs.value.ignoreEmpty
            onCheckedChanged: {
                if(prefs.value.ignoreEmpty != checked) {
                    updatePublish()
                    prefs.storePreference("ignoreEmpty", checked)
                }
            }
        }
        XsTextWithCheckBox {
            id: skipAlreadyPublishedCB
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            text: "Skip notes already published"
            checked: prefs.value.skipAlreadyPublished
            onCheckedChanged: {
                if(prefs.value.skipAlreadyPublished != checked) {
                    updatePublish()
                    prefs.storePreference("skipAlreadyPublished", checked)
                }
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Item{ id: msgDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            XsText{
                width: parent.width - itemSpacing*2
                height: message? itemHeight : 0
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                color: XsStyleSheet.accentColor //errorColor
                wrapMode: Text.Wrap
                text: message
            }
        }
        Item{ id: buttonsDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            RowLayout{
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10


                XsPrimaryButton{
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width/2
                    Layout.fillHeight: true
                    text: "Cancel"
                    onClicked: {
                        close()
                    }
                }
                XsPrimaryButton{
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width/2
                    Layout.fillHeight: true
                    text: "Publish Notes To SG"
                    enabled: !invalidPublish
                    onClicked: {
                        publishNotes()
                        close()
                    }
                }

            }
        }
        Item{ id: infoDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight
            Layout.bottomMargin: itemHeight/3


            XsText{
                x: 20
                width: parent.width - x*2
                height: itemHeight
                color: XsStyleSheet.secondaryTextColor
                font.pixelSize: XsStyleSheet.fontSize *0.9
                wrapMode: Text.Wrap
                text: "(Only notes attached to ShotGrid Media are currently supported.)"
            }
        }
    }
}

