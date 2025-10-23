// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic

import xstudio.qml.models 1.0
import xStudio 1.0

XsWindow {

	id: dialog
	width: 1024
	height: 400
    title: "Configure FFMPEG Encoding Presets"
    property string attr_name
    property string default_attr_name

    // access 'attribute' data exposed by our C++ plugin
    XsModuleData {
        id: video_render_attrs
        modelDataName: "video_render_attrs"
    }
    property alias video_render_attrs: video_render_attrs

    XsAttributeValue {
        id: __codec_options
        attributeTitle: attr_name
        model: video_render_attrs
    }
    property alias user_codec_options: __codec_options.value

    XsAttributeValue {
        id: __default_codec_options
        attributeTitle: default_attr_name
        model: video_render_attrs
    }
    property alias default_codec_options: __default_codec_options.value

    onUser_codec_optionsChanged: rebuild()
    onDefault_codec_optionsChanged: rebuild()

    ListModel{
        id: codec_presets
    }

    function rebuild() {

        codec_presets.clear()
        
        if (user_codec_options) {
            for (var i = 1; i < user_codec_options.length; ++i) {
                codec_presets.append(
                    {
                        "preset_name": user_codec_options[i][0],
                        "preset_ffmpeg_opts": user_codec_options[i][1],
                        "preset_ffmpeg_audio_opts": user_codec_options[i][2],
                        "preset_bit_depth": user_codec_options[i][3],
                        "preset_editable": true
                    })
            }
        }

        if (default_codec_options) {
            for (var i = 0; i < default_codec_options.length; ++i) {
                codec_presets.append(
                    {
                        "preset_name": default_codec_options[i][0],
                        "preset_ffmpeg_opts": default_codec_options[i][1],
                        "preset_ffmpeg_audio_opts": default_codec_options[i][2],                    
                        "preset_bit_depth": default_codec_options[i][3],
                        "preset_editable": false
                    })
            }
        }


    }

    function deletePreset(button, index) {
        if (button == "Delete") {
            var opts = user_codec_options
            opts.splice(index,1)
            user_codec_options = opts
        }
    }

    ColumnLayout {

        anchors.fill: parent

        XsLabel {
            Layout.fillWidth: true
            Layout.margins: 30
            font.italic: true
            horizontalAlignment: Qt.AlignLeft
            text: "The second column provides the output encoding options for ffmpeg when rendering videos out of xSTUDIO. The third column specifies ffmpeg options for the audio encoding (this can be blank if Render Audio is not checked). For guidelines on specifying codecs and their options try an internet search as there is a lot of advice and guidance available on tuning ffmpeg encoders. Add your own encoding presets with the + button. The fourth column determines the bitdepth of the intermediate image stream that is passed from xSTUDIO to FFMPEG. Use 8 bits if your output has an 8 bits-per-channel format. If your output has more than 8 bits per channel (e.g. ProRes codec) then select 16 bits in this column to ensure colour accuracy is maximised in the encoding. Using 8 bits will generally result in faster rendering."
        }

        Item {

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 30
            Layout.rightMargin: 30

            XsScrollBar {
                id: verticalScroll
                width: 8
                anchors.bottom: parent.bottom
                anchors.top: parent.top
                anchors.left: parent.right
                anchors.leftMargin: 3
                orientation: Qt.Vertical
                visible: target.height < target.contentHeight
                property var target: flickable

                property var yPos: target.visibleArea.yPosition
                onYPosChanged: {
                    if (!pressed) {
                        position = yPos
                    }
                }
                
                property var heightRatio: target.visibleArea.heightRatio
                onHeightRatioChanged: {
                    verticalScroll.size = heightRatio
                }

                onPositionChanged: {
                    if (pressed) {
                        target.contentY = (position * target.contentHeight) + target.originY
                    }
                }                

            }    

            XsPrimaryButton{ 
                    
                imgSrc: "qrc:///icons/add.svg"
                anchors.topMargin: 5
                anchors.top: flickable.bottom
                width: 24
                height: 24

                onClicked: {
                    var extd_opts = user_codec_options
                    extd_opts.push(["New Preset", "", "8 bits"])
                    user_codec_options = extd_opts
                }
            }

            Flickable {

                width: parent.width
                height: Math.min(parent.height, contentHeight)
                clip: true
                contentWidth: grid_layout.width
                contentHeight: grid_layout.height
                id: flickable

                GridLayout {

                    id: grid_layout
                    rows: codec_presets.count + 1
                    flow: GridLayout.TopToBottom
                    rowSpacing: 0
                    columnSpacing: 0
                    width: flickable.width

                    XsText {
                        text: "Preset Name"
                        font.weight: Font.Bold                        
                    }

                    Repeater {
                        model: codec_presets
                        XsTextField {
                            id: input
                            text: preset_name
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            horizontalAlignment: Qt.AlignLeft
                            enabled: preset_editable
                            onEditingFinished: {
                                var mod_opts = user_codec_options
                                mod_opts[index+1][0] = text
                                user_codec_options = mod_opts
                            }
                            background: Rectangle{
                                color: input.activeFocus? Qt.darker(palette.highlight, 1.5): input.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
                                border.width: input.hovered || input.active? 1:0
                                border.color: palette.highlight
                                opacity: enabled? 0.7 : 0.3
                            }
                        }
                    }

                    XsText {
                        text: "Video Encoder Parameters"
                        font.weight: Font.Bold
                    }

                    Repeater {
                        model: codec_presets
                        XsTextField {
                            id: input2
                            text: preset_ffmpeg_opts
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            placeholderText: "Enter ffmpeg VIDEO codec encoding parameters"
                            horizontalAlignment: Qt.AlignLeft
                            enabled: preset_editable
                            onEditingFinished: {
                                var mod_opts = user_codec_options
                                mod_opts[index+1][1] = text
                                user_codec_options = mod_opts
                            }
                            background: Rectangle{
                                color: input2.activeFocus? Qt.darker(palette.highlight, 1.5): input2.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
                                border.width: input2.hovered || input2.active? 1:0
                                border.color: palette.highlight
                                opacity: enabled? 0.7 : 0.3
                            }
                        }
                    }

                    XsText {
                        text: "Audio Encoder Parameters"
                        font.weight: Font.Bold
                    }

                    Repeater {
                        model: codec_presets
                        XsTextField {
                            id: input2
                            text: preset_ffmpeg_audio_opts
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            placeholderText: "Enter ffmpeg AUDIO codec encoding parameters"
                            horizontalAlignment: Qt.AlignLeft
                            enabled: preset_editable
                            onEditingFinished: {
                                var mod_opts = user_codec_options
                                mod_opts[index+1][2] = text
                                user_codec_options = mod_opts
                            }
                            background: Rectangle{
                                color: input2.activeFocus? Qt.darker(palette.highlight, 1.5): input2.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
                                border.width: input2.hovered || input2.active? 1:0
                                border.color: palette.highlight
                                opacity: enabled? 0.7 : 0.3
                            }
                        }
                    }

                    XsText {
                        text: "Src. Bit Depth"
                        font.weight: Font.Bold
                    }

                    Repeater {
                        model: codec_presets
                        XsComboBox{ 
                    
                            model: ["8 bits", "16 bits"]
                            Layout.preferredHeight: 24
                            Layout.margins: 2
                            currentIndex: model.indexOf(preset_bit_depth)
                            enabled: preset_editable

                            onActivated: (uindex) => {
                                // adjust the backend dictionary that carries the
                                // codec settings data to reflect user selected 
                                // intermediate bit depth
                                var mod_opts = user_codec_options
                                mod_opts[index+1][3] = model[uindex]
                                user_codec_options = v;
                            }

                        }
                    }

                    Item {}

                    Repeater {
                        model: codec_presets
                        XsSecondaryButton{ 
                    
                            imgSrc: "qrc:///icons/delete.svg"
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            enabled: preset_editable

                            onClicked: {

                                var idx = index+1-default_codec_options.length

                                dialogHelpers.multiChoiceDialog(
                                    deletePreset,
                                    "Delete FFMpeg Preset",
                                    "Remove FFMpeg video codec preset \""+codec_preset_names[index]+"\"?",
                                    ["Cancel", "Delete"],
                                    idx)
                            }
                        }
                    }

                }

            }

        }

        RowLayout {

            spacing: 5
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            Layout.margins: 10

            XsSimpleButton {
                id: btnCancel
                text: qsTr("Close")
                width: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: dialog.visible = false
            }

        }

    }

}