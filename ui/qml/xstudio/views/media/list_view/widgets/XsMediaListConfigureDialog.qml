// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xStudio 1.0

import xstudio.qml.helpers 1.0
import "."

XsWindow {

    id: dialog
    title: "Configure Media List Column"
    minimumWidth: 512
    minimumHeight: 320
    property bool is_backup: false

    property string regex_tooltip: `Regular expressions can be used to find and capture patterns in text data.
This can be useful when you have metadata that is an ugly big string of text which contains
a smaller section of text which is useful. Regular expressions (or 'regex') is quite a
powerful but esoteric tool. Ask you dev team for help or do some research on the web to find out more.`

property string backup_tooltip: `You can add or configure a backup metadata field. If you main metadata field
does not exist on the media item then the backup will be searched for instead. This can be useful if you have
a field that relies on metadata that might not always be there when you are mixing media that has pipeline
metadata with media that isn't from your pipeline.`

    ListModel {
        id: type_choices
        ListElement{
            type_name: "metadata"
            display_text: "Metadata Value"
        }
        ListElement{
            type_name: "flag"
            display_text: "Flag Indicator"
        }
        ListElement{
            type_name: "media_standard_details" //- "red"
            display_text: "Media Detail"
        }
        ListElement{
            type_name: "index" //- "red"
            display_text: "Selction Index"
        }
        ListElement{
            type_name: "notes" //- "red"
            display_text: "Notes Indicator"
        }
        ListElement{
            type_name: "thumbnail" //- "red"
            display_text: "Thumbnail"
        }
    }

    ListModel {
        id: metadata_owner_choices
        ListElement{
            media_owner: "Media"
        }
        ListElement{
            media_owner: "MediaSource (Image)"
        }
        ListElement{
            media_owner: "MediaSource (Audio)"
        }
    }

    ListModel {
        id: media_standard_details_options
        ListElement { detail_item: "Duration / seconds - MediaSource (Image)" }
        ListElement { detail_item: "Duration / frames - MediaSource (Image)"}
        ListElement { detail_item: "Frame Rate - MediaSource (Image)"}
        ListElement { detail_item: "Name - MediaSource (Image)"}
        ListElement { detail_item: "Resolution - MediaSource (Image)"}
        ListElement { detail_item: "Pixel Aspect - MediaSource (Image)"}
        ListElement { detail_item: "Duration / seconds - MediaSource (Audio)"}
        ListElement { detail_item: "Name - MediaSource (Audio)"}
        ListElement { detail_item: "File Path - MediaSource (Image)"}
        ListElement { detail_item: "Timecode - MediaSource (Image)"}
        ListElement { detail_item: "Frame Range - MediaSource (Image)"}
        ListElement { detail_item: "File Path - MediaSource (Audio)"}
        ListElement { detail_item: "Timecode - MediaSource (Audio)"}
        ListElement { detail_item: "Frame Range - MediaSource (Image)"}
    }

    property var model_index: columns_model.index(-1, -1)

    XsModelProperty {
        id: __column_title
        role: "title"
        index: model_index
    }
    property alias column_title: __column_title.value

    XsModelProperty {
        id: __column_data_type
        role: "data_type"
        index: model_index
    }
    property alias column_data_type: __column_data_type.value

    XsModelProperty {
        id: __info_key
        role: "info_key"
        index: model_index
    }
    property alias info_key: __info_key.value

    XsModelProperty {
        id: __metadata_source_object
        role: "object"
        index: model_index
    }
    property alias metadata_source_object: __metadata_source_object.value

    XsModelProperty {
        id: __metadata_path
        role: "metadata_path"
        index: model_index
    }
    property alias metadata_path: __metadata_path.value

    XsModelProperty {
        id: __regex_match
        role: "regex_match"
        index: model_index
    }
    property alias regex_match: __regex_match.value

    XsModelProperty {
        id: __regex_format
        role: "regex_format"
        index: model_index
    }
    property alias regex_format: __regex_format.value


    onMetadata_pathChanged: {
        if (donk.text != metadata_path) {
            donk.text = metadata_path
        }
    }

    onRegex_matchChanged: {
        if (regex_match_editor.text != regex_match) {
            regex_match_editor.text = regex_match ? regex_match : ""
        }
    }

    onRegex_formatChanged: {
        if (regex_format_editor.text != regex_format) {
            regex_format_editor.text = regex_format ? regex_format : ""
        }
    }


    function removeCallback(result) {

        if (result == "Remove Column") {
            model_index.model.removeRows(
                model_index.row,
                1,
                model_index.parent)
        }
    }


    function flattenJSON(obj, path, result) {

        for (var key in obj) {
            if (typeof obj[key] == 'object') {
                flattenJSON(obj[key], path + key + "/", result)
            } else {
                result.push([path + key, obj[key]])
            }
        }

    }

    Connections {
        target: mediaSelectionModel // this bubbles up from XsSessionWindow
        function onSelectedIndexesChanged() {
            if (loader.item && loader.item.visible) {
                show_metadata_digest()
            }
        }
    }

    function show_metadata_digest() {

        var source_object_index
        if (mediaSelectionModel.selectedIndexes.length) {
            source_object_index = mediaSelectionModel.selectedIndexes[0]
        } else {
            var media_idx = theSessionData.index(0,0,viewportCurrentMediaContainerIndex)
            if (theSessionData.rowCount(media_idx)) {
                source_object_index = theSessionData.index(0,0,media_idx)
            } else {
                dialogHelpers.errorDialogFunc("Error", "No media available to show metadata.")
                return
            }
        }

        // get the uuid of the object that we want to get metadata for (Media,
        // MediaSource (image), MediaSource (audio), MediaStream (image), MediaStream (audio))
        var actoruuid = source_object_index.model.get(source_object_index, "actorUuidRole")
        if (metadata_source_object == "MediaSource (Image)") {
            actoruuid = source_object_index.model.get(source_object_index, "imageActorUuidRole")
        } else if (metadata_source_object == "MediaSource (Audio)") {
            actoruuid = source_object_index.model.get(source_object_index, "audioActorUuidRole")
        }
        // now get the index for the object
        let idx = source_object_index.model.searchRecursive(actoruuid, "actorUuidRole", source_object_index)

        // get the metadata!
        var result = []
        if (idx.valid) {
            flattenJSON(JSON.parse(idx.model.getJSON(idx, "")),"/", result)
        } else {
            result = [["No " + metadata_source_object + " Object", "No Metadata!"]]
        }
        loader.sourceComponent = metadata_digest_dialog
        loader.item.metadata_digest = result
        loader.item.title = "Metadata for " + metadata_source_object
        loader.item.visible = true
    }

    Loader {
        id: loader
    }

    Component {
        id: metadata_digest_dialog
        XsMediaMetadataWindow {
            onMetadataSelected: {
                metadata_path = metadataPath
            }
        }
    }

    function configure() {
        loader.sourceComponent = configureDialog
        loader.item.visible = true
    }

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 10

        GridLayout {

            id: main_layout
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            XsLabel {
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                text: "Title : "
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 140
                visible: !is_backup
            }

            XsTextField {

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                text: column_title
                onTextChanged: column_title = text
                visible: !is_backup

            }

            XsLabel {
                text: "Type : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            XsComboBox {
                model: type_choices
                textRole: "display_text"
                valueRole: "type_name"
                Layout.fillWidth: true
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                onCurrentValueChanged: {
                    if (visible)
                        column_data_type = currentValue
                }
                property var backend: column_data_type
                onBackendChanged: {
                    for (var i = 0; i < type_choices.count; ++i) {
                        if (type_choices.get(i).type_name == backend) {
                            currentIndex = i
                        }
                    }
                }
            }

            XsLabel {
                text: "Media Detail : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                visible: column_data_type == "media_standard_details"
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            }

            XsComboBox {
                model: media_standard_details_options
                textRole: "detail_item"
                valueRole: "detail_item"
                Layout.fillWidth: true
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                visible: column_data_type == "media_standard_details"
                onCurrentValueChanged: {
                    if (visible && currentValue)
                        info_key = currentValue
                }
                property var backend: info_key
                onBackendChanged: {
                    for (var i = 0; i < media_standard_details_options.count; ++i) {
                        if (media_standard_details_options.get(i).detail_item == backend) {
                            currentIndex = i
                        }
                    }
                }
            }

            XsLabel {
                text: "Metadata Owner : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                visible: column_data_type == "metadata"
                Layout.preferredHeight: visible ? XsStyleSheet.widgetStdHeight : 0
            }

            XsComboBox {
                model: metadata_owner_choices
                textRole: "media_owner"
                valueRole: "media_owner"
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? XsStyleSheet.widgetStdHeight : 0
                visible: column_data_type == "metadata"
                onCurrentValueChanged: {
                    if (visible && currentValue) {
                        metadata_source_object = currentValue
                        if (loader.item && loader.item.visible) {
                            show_metadata_digest()
                        }
                    }
                }
                property var backend: metadata_source_object
                onBackendChanged: {
                    for (var i = 0; i < metadata_owner_choices.count; ++i) {
                        if (metadata_owner_choices.get(i).media_owner == backend) {
                            currentIndex = i
                        }
                    }
                }
            }

            XsLabel {
                text: "Metadata Path : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                Layout.preferredHeight: visible ? XsStyleSheet.widgetStdHeight : 0
                visible: column_data_type == "metadata"
            }

            RowLayout {
                Layout.fillWidth: true
                visible: column_data_type == "metadata"
                XsTextField {

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                    Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                    onTextChanged: {
                        if (metadata_path != text) {
                            metadata_path = text
                        }
                    }
                    text: metadata_path
                    id: donk

                }

                XsPrimaryButton {
                    id: show_metadata
                    Layout.preferredWidth: XsStyleSheet.widgetStdHeight
                    Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                    onClicked: {
                        show_metadata_digest()
                    }
                    Image {
                        anchors.fill: parent
                        source: "qrc:/icons/quick_reference_all.svg"
                        layer {
                            enabled: true
                            effect: ColorOverlay {
                                color: show_metadata.down || show_metadata.hovered ? palette.brightText : palette.text
                            }
                        }
                    }
                }
            }

            XsLabel {
                text: "Show Advanced Options : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                tooltipText: regex_tooltip
            }

            XsCheckBox{
                id: advanced
                Layout.preferredWidth: XsStyleSheet.widgetStdHeight
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            }

            XsLabel {
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                text: "Capture Expression : "
                horizontalAlignment: Text.AlignRight
                visible: advanced.checked
                tooltipText: regex_tooltip
            }

            XsTextField {

                id: regex_match_editor
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                text: regex_match
                onTextChanged: regex_match = text
                visible: advanced.checked

            }

            XsLabel {
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                text: "Format Expression : "
                horizontalAlignment: Text.AlignRight
                visible: advanced.checked
                tooltipText: regex_tooltip
            }

            XsTextField {

                id: regex_format_editor
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                text: regex_format
                onTextChanged: regex_format = text
                visible: advanced.checked

            }

            XsLabel {
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                text: ""
                horizontalAlignment: Text.AlignRight
                visible: advanced.checked
            }

            XsSimpleButton {

                text: model_index.model.rowCount(model_index) ? qsTr("Configure Backup") : qsTr("Add Backup")
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                tooltipText: backup_tooltip
                visible: advanced.checked && !is_backup
                onClicked: {
                    if (model_index.model.rowCount(model_index)) {
                        model_index = model_index.model.index(0, 0, model_index)
                        is_backup = true
                    } else {
                        model_index.model.insertRows(0,1,model_index)
                        model_index = model_index.model.index(0, 0, model_index)
                        is_backup = true
                    }
                }
            }

        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {

            id: myFooter
            Layout.fillWidth: true

            focus: true
            Keys.onReturnPressed: accept()
            Keys.onEscapePressed: reject()

            XsPrimaryButton {
                text: qsTr("Remove")
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                visible: !is_backup
                onClicked: {
                    dialogHelpers.multiChoiceDialog(
                        removeCallback,
                        "Remove Column",
                        "Are you sure you want to remove the " + column_title + " column?",
                        ["Remove Column", "Cancel"],
                        undefined)
                }
            }

            XsPrimaryButton {
                text: qsTr("Move Left")
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                visible: !is_backup
                onClicked: {
                    var r = model_index.row
                    if (r) {
                        var p = model_index.parent
                        model_index.model.moveRows(
                            model_index.parent,
                            model_index.row,
                            1,
                            model_index.parent,
                            model_index.row-1
                            )
                        model_index = p.model.index(r-1, 0, p)
                    }
                }
            }

            XsPrimaryButton {
                text: qsTr("Move Right")
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                visible: !is_backup
                onClicked: {
                    var r = model_index.row
                    if (r < (model_index.model.rowCount(model_index.parent)-1)) {
                        var p = model_index.parent
                        model_index.model.moveRows(
                            model_index.parent,
                            model_index.row,
                            1,
                            model_index.parent,
                            model_index.row+2
                            )
                        model_index = p.model.index(r+1, 0, p)
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }
            XsSimpleButton {

                text: is_backup ? qsTr("Return") : qsTr("Close")
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight

                onClicked: {
                    if (is_backup) {
                        model_index = model_index.parent
                        is_backup = false
                    } else {
                        dialog.visible = false
                    }
                }
            }

        }
    }
}