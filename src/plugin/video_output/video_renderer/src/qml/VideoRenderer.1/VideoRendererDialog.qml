// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import xstudio.qml.models 1.0
import xStudio 1.0
import "."

XsWindow {

	id: dialog
	minimumWidth: 456
	minimumHeight: 600
    width: 560
    title: "Render Video"

    flags: Qt.platform.os === "windows" ? Qt.Window : Qt.Dialog

    property var numSelected: sessionSelectionModel.selectedIndexes.length
    property var currentItem: 0
    property var itemToRender: numSelected ? sessionSelectionModel.selectedIndexes[currentItem] : undefined

    property var selectedContainerType: itemToRender ? theSessionData.get(itemToRender, "typeRole") : ""
    property var selectedContainerName: itemToRender ? theSessionData.get(itemToRender, "nameRole") : ""
    property var selectedContainerUuid: itemToRender ? theSessionData.get(itemToRender, "actorUuidRole") : ""

    onItemToRenderChanged: {
        if (itemToRender && itemToRender.valid) {
            var renderTargetId = theSessionData.get(itemToRender, "actorUuidRole")
            if (renderTargetId) {
                xstudio_callback(["update_ocio_choices", renderTargetId])
            }

            if (theSessionData.get(itemToRender, "typeRole") == "Timeline") {
                render_range = theSessionData.getTimelineFullRangeAndLoopRangeAndFPS(itemToRender);
                if (render_range.length == 4) {
                    inPoint.text = ""+0
                    outPoint.text = ""+render_range[0]
                    fps_best_guess = render_range[3]
                }
            } else {
                inPoint.text = "--"
                outPoint.text = "--"                
            }
            
        }
    }

    property var render_range
    property bool apply_render_range: selectedContainerType == "Timeline"


    property var fps_best_guess

    onFps_best_guessChanged: {
        if (frame_rates == undefined) return
        var frateIdx = -1
        for (var fridx = 0; fridx < frame_rates.length; ++fridx) {
            if (Math.abs(parseFloat(frame_rates[fridx])-fps_best_guess) < 0.001) {
                frateIdx = fridx
                break
            }
        }
        if (frateIdx != -1) {
            frame_rate.currentIndex = frateIdx
        }
    }

    onSelectedContainerUuidChanged: {

        // Here we want to auto-set the frame rate to match the timeline frame
        // rate or the rate of the media in the playlist (or the firs bit of 
        // media in the playlist)

        if (itemToRender == undefined || !itemToRender.valid) return;
        if (theSessionData.get(itemToRender, "typeRole") == "Timeline") {

            // use call to getTimelineFullRangeAndLoopRangeAndFPS made above to 
            // retrieve the timeline frame rate.

        } else {

            // Otherwise we want to auto-set the FrameRate to match the rate of
            // the first bit of media in the selected container.

            // get the model index for the Media List under the playlist
            var media_list_idx = theSessionData.index(0, 0, itemToRender)
            var n_media = theSessionData.rowCount(media_list_idx)
            for (var i = 0; i < n_media; ++i) {
                var media_item_idx = theSessionData.index(i, 0, media_list_idx)
                // now get the index for the object
                var actoruuid = theSessionData.get(media_item_idx, "imageActorUuidRole")
                let image_source = theSessionData.searchRecursive(actoruuid, "actorUuidRole", media_item_idx)
                if (image_source.valid) {
                    var _fps = theSessionData.get(image_source, "rateFPSRole")
                    if (_fps) {
                        // set the best guess, triggering the combo-box selection
                        fps_best_guess = _fps;
                        break;
                    }
                }
            }
        }

        // we also want to turn on audio output if any of the media has 
        // audio ... actually, this doesn't work
                
        /*var has_audio = false
        for (var i = 0; i < n_media; ++i) {
            var media_item_idx = theSessionData.index(i, 0, media_list_idx)
            // now get the index for the object
            var actoruuid = theSessionData.get(media_item_idx, "audioActorUuidRole")
            if (!actoruuid.isNull) {
                has_audio = true
                break
            }
        }
        render_audio.checked = has_audio*/

    }

    // access 'attribute' data exposed by our C++ plugin
    XsModuleData {
        id: video_render_attrs
        modelDataName: "video_render_attrs"
    }
    property alias video_render_attrs: video_render_attrs

    XsAttributeValue {
        id: __render_queue_visible
        attributeTitle: "render_queue_visible"
        model: video_render_attrs
    }
    property alias render_queue_visible: __render_queue_visible.value

    XsAttributeValue {
        id: __ffmpeg_stdout
        attributeTitle: "ffmpeg_stdout"
        model: video_render_attrs
    }
    property alias ffmpeg_stdout: __ffmpeg_stdout.value

    XsAttributeValue {
        id: __frame_rate_attr
        attributeTitle: "frame_rate"
        model: video_render_attrs
    }
    property alias rame_rate_attr: __frame_rate_attr.value

    XsAttributeValue {
        id: __resolution_options
        attributeTitle: "resolution_options"
        model: video_render_attrs
    }
    property alias resolution_options: __resolution_options.value
    property var resOptWatcher: __resolution_options.value
    property var resolution_names: []
    onResOptWatcherChanged: {
        // resolution_options is an array. Each element is a 2 element array, the name
        // of the resoluition and whether it can be deleted (defaults can't
        // be deleted from the list)
        var opts = []
        for (var i = 1; i < resolution_options.length; ++i) {
            opts.push(resolution_options[i][0])
        }
        resolution_names = opts

        // pick the last res that the user chose
        var idx = resolution_names.indexOf(resolution_options[0])
        if (idx != -1) {
            output_resolution.currentIndex = idx
        }
    }

    XsAttributeValue {
        id: __render_audio
        attributeTitle: "render_audio"
        model: video_render_attrs
    }
    property alias render_audio: __render_audio.value

    XsAttributeValue {
        id: __load_on_completion
        attributeTitle: "load_on_completion"
        model: video_render_attrs
    }
    property alias load_on_completion: __load_on_completion.value

    XsAttributeValue {
        id: __frame_rates
        attributeTitle: "frame_rates"
        model: video_render_attrs
    }
    property alias frame_rates: __frame_rates.value

    property string last_render_folder

    function choose_output(fileUrl, undefined, func) {
        outputFile.text = fileUrl

        // Without this, on MacOS, the dialog dissapears behind
        // the main window when the file browser is closed
        dialog.hide()
        dialog.show()
        dialog.raise()
        dialog.raise()
    }

    property var widgetHeight: 24

    ColumnLayout {

        width: parent.width

        GridLayout {

            Layout.fillWidth: true
            Layout.margins: 20
            columnSpacing: 12
            rowSpacing: 8
            columns: 2

            VidRenderDlgDivider {
                Layout.preferredHeight: widgetHeight
                Layout.fillWidth: true
                Layout.columnSpan: 2
                label: "Output"
            }

            XsText {
                text: "Item to Render"
                Layout.alignment: Qt.AlignRight
            }

            XsTextCopyable {
                horizontalAlignment: Text.AlignLeft
                text: selectedContainerName + " (" + selectedContainerType + ")"
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            XsText {
                text: "Output File"
                Layout.alignment: Qt.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                XsTextField{ 

                    id: outputFile
                    Layout.fillWidth: true
                    Layout.preferredHeight: widgetHeight
                    clip: true
                    placeholderText: "Enter or browse for ouput file"
                    background: Rectangle{
                        color: outputFile.activeFocus? Qt.darker(palette.highlight, 1.5): outputFile.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
                        border.width: outputFile.hovered || outputFile.active? 1:0
                        border.color: palette.highlight
                        opacity: 0.7
                    }
                    //text: "file:///user_data/.tmp/test.mp4"
                }
                
                XsPrimaryButton {
                    Layout.preferredHeight: widgetHeight
                    text: "Browse ..."
                    onClicked: {
                        dialogHelpers.showFileDialog(
                            choose_output,
                            last_render_folder ? last_render_folder : file_functions.defaultSessionFolder(),
                            "Export Render",
                            ".mov",
                            ["All Files (*.*)", "Media Files (*.mov *.mp4 *.mkv *.webm)"],
                            false,
                            false,
                            undefined
                        )
                    }
                }
            }

            XsText {
                text: "Render Range"
                Layout.alignment: Qt.AlignRight
                enabled: apply_render_range
            }

            RowLayout {

                XsTextField{ 
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: widgetHeight
                    id: inPoint
                    enabled: apply_render_range
                    validator: IntValidator
                }

                XsText {
                    text: "to"
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    enabled: apply_render_range
                }

                XsTextField{ 
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: widgetHeight
                    id: outPoint
                    enabled: apply_render_range
                    validator: IntValidator
                }

                XsPrimaryButton {
                    Layout.preferredHeight: widgetHeight
                    Layout.preferredWidth: 70
                    Layout.leftMargin: 10
                    text: "Use Full"
                    enabled: apply_render_range
                    onClicked: {
                        if (theSessionData.get(itemToRender, "typeRole") == "Timeline") {
                            render_range = theSessionData.getTimelineFullRangeAndLoopRangeAndFPS(itemToRender);
                        }
                        if (render_range.length == 4) {
                            inPoint.text = ""+0
                            outPoint.text = ""+render_range[0]
                        }
                    }
                    toolTip: "Set render range to full timeline range"
                }

                XsPrimaryButton {
                    Layout.preferredHeight: widgetHeight
                    Layout.preferredWidth: 70
                    text: "Use Loop"
                    enabled: apply_render_range
                    onClicked: {
                        if (theSessionData.get(itemToRender, "typeRole") == "Timeline") {
                            render_range = theSessionData.getTimelineFullRangeAndLoopRangeAndFPS(itemToRender);
                        }
                        if (render_range.length == 4) {
                            inPoint.text = ""+render_range[1]
                            outPoint.text = ""+render_range[2]
                        }
                    }
                    toolTip: "Set render range to current playhead loop range"
                }

            }


            XsText {
                text: "Load on Completion"
                Layout.alignment: Qt.AlignRight
            }

            XsCheckBox {
                
                Layout.alignment: Qt.AlignLeft
                Layout.preferredHeight: widgetHeight
                checked: load_on_completion
                onClicked: {
                    load_on_completion = !load_on_completion
                }
            }
            
            VidRenderDlgDivider {
                Layout.preferredHeight: widgetHeight
                Layout.topMargin: 20
                Layout.fillWidth: true
                Layout.columnSpan: 2
                label: "Video Options"
            }

            XsText {
                text: "Output Resolution"
                Layout.alignment: Qt.AlignRight
            }

            XsComboBoxEditable{ 
        
                id: output_resolution
                model: resolution_names
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 200
                Layout.preferredHeight: widgetHeight
                onAccepted: {
                    // note, accepted() is emitted rather often as we have linked
                    // it to onEditingFinished on XsComboBox.
                    const regex = /([0-9]+)x([0-9]+)/;
                    var resmatch = regex.exec(editText)
                    if (resmatch && resmatch.length >= 3) {
                        for (var i = 0; i < resolution_names.length; ++i) {
                            var rf = regex.exec(resolution_names[i])
                            if (rf.length >= 3 && rf[1] == resmatch[1] && rf[2] == resmatch[2]) {
                                // already have this res
                                return
                            }
                        }
                        var v = resolution_options
                        v.push([editText, true])
                        resolution_options = v
                        currentIndex = v.length-2
                    }
                }

                function get_resolution() {

                    // store the selected res in the first element, so we can
                    // 'remember' it
                     var v = resolution_options
                     v[0] = currentText
                     resolution_options = v

                    const regex = /([0-9]+)x([0-9]+)/;
                    var resmatch = regex.exec(currentText)
                    if (resmatch && resmatch.length >= 3) {
                        return ["ivec2", 1, parseInt(resmatch[1]), parseInt(resmatch[2])]
                    }
                    return ["ivec2", 1, 1290, 345]
                }

                onClearButtonPressed: (clearedIndex)=> {
                    if (resolution_options[clearedIndex+1][1]) {
                        var v = resolution_options
                        v.splice(clearedIndex+1,1)
                        resolution_options = v
                        currentIndex = 0
                    }
                }

            }

            XsText {
                text: "Frame Rate"
                Layout.alignment: Qt.AlignRight
            }

            XsComboBoxEditable{ 
        
                id: frame_rate
                model: frame_rates
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 200
                Layout.preferredHeight: widgetHeight
                function get_rate() {
                    rame_rate_attr = currentText
                    return parseFloat(currentText)
                }
                onModelChanged: {
                    currentIndex = model.indexOf(rame_rate_attr)
                }
            }

            XsText {
                text: "Video & Audio Encoding Preset"
                Layout.alignment: Qt.AlignRight
            }

            CodecOptionsBox {
                
                Layout.alignment: Qt.AlignLeft
                Layout.preferredHeight: widgetHeight
                attr_name: "video_codec_options"
                default_attr_name: "video_codec_default_options"
                id: vid_codec_opts
            }

            XsText {
                text: "Render Audio"
                Layout.alignment: Qt.AlignRight
            }

            XsCheckBox {
                
                Layout.alignment: Qt.AlignLeft
                Layout.preferredHeight: widgetHeight
                id: ra
                checked: render_audio
                onClicked: {
                    render_audio = !render_audio
                }
            }

            VidRenderDlgDivider {
                Layout.preferredHeight: widgetHeight
                Layout.topMargin: 20
                Layout.fillWidth: true
                Layout.columnSpan: 2
                label: "Colour Settings"
            }

            XsText {
                text: "OCIO Display"
                Layout.alignment: Qt.AlignRight
            }

            XsAttrComboBox {
                id: ocio_display
                Layout.alignment: Qt.AlignLeft
                Layout.preferredHeight: widgetHeight
                Layout.minimumWidth: 200
                attr_title: "Display"
                attr_model_name: "video_renderer_ocio_opts"
            }

            XsText {
                text: "OCIO View"
                Layout.alignment: Qt.AlignRight
            }

            XsAttrComboBox {
                id: ocio_view
                Layout.alignment: Qt.AlignLeft
                Layout.preferredHeight: widgetHeight
                Layout.minimumWidth: 200
                attr_title: "View"
                attr_model_name: "video_renderer_ocio_opts"
            }

        }

    }

    RowLayout {

        spacing: 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10

        XsSimpleButton {
            id: helpBtn
            Layout.alignment: Qt.AlignLeft
            text: qsTr("Help")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                var url = studio.docsSectionUrl("docs/user_docs/workflow/video_output.html")
                if (url == "") {
                    dialogHelpers.errorDialogFunc("Error", "Unable to resolve location of user docs.")
                } else if (!helpers.openURL(url)) {
                    dialogHelpers.errorDialogFunc("Error", "Failed to launch webbrowser, this is the link, please manually visit this.<BR><BR>"+url+"<BR><BR>")
                }        
            }
            XsToolTip {
                text: "Check for the help page in your open web browser after clicking this button."
                visible: helpBtn.hovered
            }
        }

        Item {
            Layout.fillWidth: true
        }

        XsSimpleButton {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Cancel")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.hide()
                dialog.destroy()
            }
        }

        XsSimpleButton {
            Layout.alignment: Qt.AlignRight
            Layout.leftMargin: visible ? 5 : 0
            text: qsTr("Skip")
            width: visible ? XsStyleSheet.primaryButtonStdWidth*2 : 0
            visible: numSelected-currentItem > 1
            onClicked: {
                currentItem = currentItem+1
                if (currentItem == numSelected) {
                    dialog.hide()
                    dialog.destroy()
                }
            }
        }

        XsSimpleButton {
            Layout.alignment: Qt.AlignRight
            Layout.leftMargin: 5
            text: qsTr("Render")
            width: XsStyleSheet.primaryButtonStdWidth*2
            enabled: outputFile.text != ""
            onClicked: {
    
                var parentPlaylistUuiid = theSessionData.get(itemToRender, "actorUuidRole")                
                var renderName = selectedContainerName
                if (["Subset", "Timeline", "ContactSheet"].includes(selectedContainerType)) {
                    parentPlaylistUuiid = theSessionData.get(itemToRender.parent.parent, "actorUuidRole")
                    var parentPlaylistName = theSessionData.get(itemToRender.parent.parent, "nameRole")
                    renderName = parentPlaylistName + " / " + renderName
                }

                renderName = renderName + " ("+ selectedContainerType + ")"

                var cb_args = [
                        renderName,
                        parentPlaylistUuiid,
                        selectedContainerUuid,
                        outputFile.text,
                        output_resolution.get_resolution(),
                        inPoint.text != "--" ? parseInt(inPoint.text) : -1,
                        outPoint.text != "--" ? parseInt(outPoint.text) : -1,
                        frame_rate.get_rate(),
                        vid_codec_opts.get_codec_options()[0],
                        vid_codec_opts.get_bit_depth(),
                        render_audio ? vid_codec_opts.get_codec_options()[1] : "",
                        vid_codec_opts.get_preset_name(),
                        ocio_display.currentText,
                        ocio_view.currentText,
                        load_on_completion
                    ]

                render_queue_visible = true

                xstudio_callback(cb_args)
                currentItem = currentItem+1
                if (currentItem == numSelected) {
                    dialog.hide()
                    dialog.destroy()
                }

            }
        }
    }


}