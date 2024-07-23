// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import Qt.labs.qmlmodels 1.0

import xStudio 1.1
import xstudio.qml.module 1.0

XsDialogModal {
    id: dlg
    width: main_layout.width + 80
    height: main_layout.height + 100
    title: "Preferences"

    centerOnOpen: true

    XsHotkeysDialog {
        id: hotkeys_dialog
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            id: main_layout
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            XsLabel {
                text: "Enable Presentation Mode On Startup"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.enable_presentation_mode.value
                onTriggered: {
                    preferences.enable_presentation_mode.value = !preferences.enable_presentation_mode.value
                }
            }

            XsLabel {
                text: "Begin Playback On Startup"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.start_play_on_load.value
                onTriggered: {
                    preferences.start_play_on_load.value = !preferences.start_play_on_load.value
                }
            }

            XsLabel {
                text: "Do Shutdown Animation"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.do_shutdown_anim.value
                onTriggered: {
                    preferences.do_shutdown_anim.value = !preferences.do_shutdown_anim.value
                }
            }

            XsLabel {
                text: "Check unsaved session"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.check_unsaved_session.value
                onTriggered: {
                    preferences.check_unsaved_session.value = !preferences.check_unsaved_session.value
                }
            }

            XsLabel {
                text: "Load Additional Media Sources"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.auto_gather_sources.value
                onTriggered: {
                    preferences.auto_gather_sources.value = !preferences.auto_gather_sources.value
                }
            }



            XsModuleAttributesModel {
                id: media_hooks_attrs
                attributesGroupNames: "media_hook_settings"
            }

            property int num_rows_so_far: 5

            Repeater {

                model: media_hooks_attrs
    
                XsLabel {
                    text: title
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.column: 0
                    Layout.row: index+5
                }
            }


            Repeater {

                model: media_hooks_attrs

                delegate: chooser

                DelegateChooser {
                    id: chooser
                    role: "type"

                    DelegateChoice {
                        roleValue: "FloatScrubber";
                        XsFloatAttrSlider {
                            height: 20
                            Layout.column: 1
                            Layout.row: index+5       
                        }
                    }

                    DelegateChoice {
                        roleValue: "IntegerValue";
                        XsIntAttrSlider {
                            height: 20
                            Layout.column: 1
                            Layout.row: index+5
                        }
                    }

                    DelegateChoice {
                        roleValue: "ColourAttribute";
                        XsColourChooser {
                            height: 20
                            Layout.column: 1
                            Layout.row: index+5        
                        }
                    }
                    

                    DelegateChoice {
                        roleValue: "OnOffToggle";
                        Rectangle {
                            color: "transparent"
                            height: 20
                            Layout.column: 1
                            Layout.row: index+5       
                            XsBoolAttrCheckBox {
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.topMargin: 2
                                anchors.bottomMargin: 2
                                tooltip_text: tooltip ? tooltip : ""
                            }
                        }
                    }

                    DelegateChoice {
                        roleValue: "ComboBox";
                        Rectangle {
                            height: 20
                            color: "transparent"
                            Layout.column: 1
                            Layout.row: index+5       
                            implicitWidth: 200    
                            XsComboBox {
                                model: combo_box_options
                                anchors.fill: parent
                                property var value_: value ? value : null                                
                                onValue_Changed: {
                                    currentIndex = indexOfValue(value_)
                                }
                                Component.onCompleted: currentIndex = indexOfValue(value_)
                                onCurrentValueChanged: {
                                    value = currentValue;
                                }
                                tooltip_text: tooltip ? tooltip : ""
                            }
                        }
                    }

                }
            }

            XsLabel {
                text: "Pause Playback After Scrubbing"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: !preferences.restore_play_state_after_scrub.value
                onTriggered: {
                    preferences.restore_play_state_after_scrub.value = !preferences.restore_play_state_after_scrub.value
                }
            }

            XsLabel {
                text: "Cycle through playlist using keyboard arrows"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.cycle_through_playlist.value
                onTriggered: {
                    preferences.cycle_through_playlist.value = !preferences.cycle_through_playlist.value
                }
            }

            XsLabel {
                text: "Mouse wheel behaviour"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            XsModuleAttributesModel {
                id: vp_mouse_wheel_behaviour_attr
                attributesGroupNames: "viewport_mouse_wheel_behaviour_attr"
            }

            Repeater {

                // Using a repeater here - but 'vp_mouse_wheel_behaviour_attr' only
                // has one row by the way. The use of a repeater means the model role
                // data are all visible in the XsComboBox instance.
                model: vp_mouse_wheel_behaviour_attr

                XsComboBox {

                    implicitWidth: 200
                    implicitHeight: 18
                    model: combo_box_options
                    property var value_: value ? value : null
                    onValue_Changed: {
                        currentIndex = indexOfValue(value_)
                    }
                    Component.onCompleted: currentIndex = indexOfValue(value_)
                    onCurrentValueChanged: {
                        value = currentValue;
                    }
                }
            }

            XsLabel {
                text: "Frame Scrubbing Sensitivity"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            Rectangle {

                color: XsStyle.mediaInfoBarOffsetBgColor
                border.width: 1
                border.color: ma2.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
                width: scrubPixelStepInput.font.pixelSize*6
                height: scrubPixelStepInput.font.pixelSize*1.4

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                }

                TextInput {

                    anchors.fill: parent
                    id: scrubPixelStepInput
                    text: "" + preferences.viewport_scrub_sensitivity.value
                    width: font.pixelSize*2
                    color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
                    selectByMouse: true
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter

                    font {
                        family: XsStyle.fontFamily
                    }

                    onEditingFinished: {
                        focus = false
                        preferences.viewport_scrub_sensitivity.value = parseInt(text)
                    }
                }
            }

            XsLabel {
                text: "UI Accent Colour"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            Rectangle {
                gradient: styleGradient.accent_gradient
                color: XsStyle.highlightColor
                width: XsStyle.menuItemHeight *.66
                height: XsStyle.menuItemHeight *.66
                radius: XsStyle.menuItemHeight *.33
                property alias ma: ma
                border.width: 1 // mycheckableDecorated?1:0
                border.color: ma.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor

                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        colorsMenu.open()
                    }
                }

                XsMenu {
                    y: parent.height/2
                    x: parent.width/2
                    title: qsTr("Accent Colour")
                    id: colorsMenu

                    XsColorsModel {
                        id: the_colours
                    }

                    Instantiator {
                        model: the_colours
                        XsMenuItem {
                            text: model.name
                            iconbg: model.value
                            mycheckable: true
                            property var vv: model.value
                            checked: XsStyle.highlightColor == vv //preferences.accent_color_string == vv
                            onTriggered: {
                                // XsStyle.highlightColor = vv

                                preferences.accent_color_string.value = vv

                                // XsStyle.highlightColorString = vv
                                styleGradient.highlightColorString = vv

                            }

                        }

                        // The trick is on those two lines
                        onObjectAdded: colorsMenu.insertItem(index, object)
                        onObjectRemoved: colorsMenu.removeItem(object)
                    }
                }

            }

            XsLabel {
                text: "New Image Sequence Frame Rate"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            Rectangle {

                color: XsStyle.mediaInfoBarOffsetBgColor
                border.width: 1
                border.color: ma2.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
                width: offsetInput.font.pixelSize*6
                height: offsetInput.font.pixelSize*1.4
                id: fpsInputBox

                MouseArea {
                    id: ma2
                    anchors.fill: parent
                    hoverEnabled: true
                }

                TextInput {

                    anchors.fill: parent
                    id: offsetInput
                    text: "" + app_window.sessionRate
                    width: font.pixelSize*2
                    color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
                    selectByMouse: true
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    property var rate: app_window.sessionRate
                    onRateChanged: {
                        text = "" + Number.parseFloat(rate).toFixed(3)
                        text.replace(/\.0+$/,'')
                    }

                    font {
                        family: XsStyle.fontFamily
                    }

                    onEditingFinished: {
                        let new_rate = parseFloat(text)
                        focus = false
                        app_window.sessionRate = text
                        preferences.new_media_rate.value = new_rate
                    }
                }
            }

            XsLabel {
                text: "Audio Latency / Milliseconds"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            Rectangle {

                color: XsStyle.mediaInfoBarOffsetBgColor
                border.width: 1
                border.color: ma3.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
                width: audioLatencyInput.font.pixelSize*6
                height: audioLatencyInput.font.pixelSize*1.4
                id: audioLatencyInputBox

                MouseArea {
                    id: ma3
                    anchors.fill: parent
                    hoverEnabled: true
                }

                TextInput {

                    anchors.fill: parent
                    id: audioLatencyInput
                    text: "" + preferences.audio_latency_millisecs.value
                    width: font.pixelSize*2
                    color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
                    selectByMouse: true
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter

                    font {
                        family: XsStyle.fontFamily
                    }

                    onEditingFinished: {
                        focus = false
                        preferences.audio_latency_millisecs.value = parseInt(text)
                    }
                }
            }

            XsLabel {
                text: "Image Cache (MB)"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            Rectangle {

                color: XsStyle.mediaInfoBarOffsetBgColor
                border.width: 1
                border.color: ma2.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
                width: offsetInput2.font.pixelSize*6
                height: offsetInput2.font.pixelSize*1.4

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                }

                TextInput {

                    anchors.fill: parent
                    id: offsetInput2
                    text: "" + preferences.image_cache_limit.value
                    width: font.pixelSize*2
                    color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
                    selectByMouse: true
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter

                    font {
                        family: XsStyle.fontFamily
                    }

                    onEditingFinished: {
                        focus = false
                        preferences.image_cache_limit.value = parseInt(text)
                    }
                }
            }

            XsLabel {
                text: "Viewport Pixel Filtering"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            XsModuleAttributesModel {
                id: vp_pixel_filtering_attr
                attributesGroupNames: "viewport_pixel_filter"
            }

            Repeater {

                // Using a repeater here - but 'vp_pixel_filtering_attr' only
                // has one row by the way. The use of a repeater means the model role
                // data are all visible in the XsComboBox instance.
                model: vp_pixel_filtering_attr

                XsComboBox {

                    implicitWidth: 200
                    implicitHeight: 18
                    model: combo_box_options
                    property var value_: value ? value : null
                    onValue_Changed: {
                        currentIndex = indexOfValue(value_)
                    }
                    Component.onCompleted: currentIndex = indexOfValue(value_)
                    onCurrentValueChanged: {
                        value = currentValue;
                    }
                }
            }

            XsLabel {
                text: "Viewport Texture Mode"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            XsModuleAttributesModel {
                id: vp_ptex_mode_attr
                attributesGroupNames: "viewport_texture_mode"
            }

            Repeater {

                // Using a repeater here - but 'vp_pixel_filtering_attr' only
                // has one row by the way. The use of a repeater means the model role
                // data are all visible in the XsComboBox instance.
                model: vp_ptex_mode_attr

                XsComboBox {

                    implicitWidth: 200
                    implicitHeight: 18
                    model: combo_box_options
                    property var value_: value ? value : null
                    onValue_Changed: {
                        currentIndex = indexOfValue(value_)
                    }
                    Component.onCompleted: currentIndex = indexOfValue(value_)
                    onCurrentValueChanged: {
                        value = currentValue;
                    }
                }
            }

            XsLabel {
                text: "Default Compare Mode"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }

            XsComboBox {
                id: compareBox
                implicitWidth: 200
                implicitHeight: 18

                model: ListModel {
                    id: model
                    ListElement { text: "String" }
                    ListElement { text: "A/B" }
                    //ListElement { text: "Vertical" }
                    //ListElement { text: "Horizontal" }
                    //ListElement { text: "Grid" }
                    ListElement { text: "Off" }
                }
                currentIndex: -1

                onCurrentIndexChanged: {
                    if(currentIndex != -1)
                        preferences.default_playhead_compare_mode.value = compareBox.model.get(currentIndex).text
                }
                Component.onCompleted: {
                    currentIndex = compareBox.find(preferences.default_playhead_compare_mode.value)
                }
                Connections {
                    target: preferences.default_playhead_compare_mode
                    function onValueChanged() {
                        compareBox.currentIndex = compareBox.find(preferences.default_playhead_compare_mode.value)
                    }
                }
            }

            XsLabel {
                text: "Launch QuickView window for all incoming media"
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
            XsCheckboxOld {
                checked: preferences.quickview_all_incoming_media.value
                onTriggered: {
                    preferences.quickview_all_incoming_media.value = !preferences.quickview_all_incoming_media.value
                }
            }

        }
        DialogButtonBox {

            id: myFooter

            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 20

            horizontalPadding: 10
            verticalPadding: 10
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            RoundButton {
                id: btnOK
                text: qsTr("Close")
                width: 60
                height: 24
                radius: 5
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                background: Rectangle {
                    radius: 5
    //                color: XsStyle.highlightColor//mouseArea.containsMouse?:XsStyle.controlBackground
                    color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                    gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                    anchors.fill: parent
                }
                contentItem: Text {
                    text: btnOK.text
                    color: XsStyle.hoverColor//:XsStyle.mainColor
                    font.family: XsStyle.fontFamily
                    font.hintingPreference: Font.PreferNoHinting
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: mouseArea
                    hoverEnabled: true
                    anchors.fill: btnOK
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        forceActiveFocus()
                        close()
                    }
                }
            }

            /*RoundButton {
                id: btnHotkeys
                text: qsTr("Hotkeys")
                width: 60
                height: 24
                radius: 5
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                background: Rectangle {
                    radius: 5
    //                color: XsStyle.highlightColor//mouseArea.containsMouse?:XsStyle.controlBackground
                    color: mouseArea2.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                    gradient:mouseArea2.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                    anchors.fill: parent
                }
                contentItem: Text {
                    text: btnHotkeys.text
                    color: XsStyle.hoverColor//:XsStyle.mainColor
                    font.family: XsStyle.fontFamily
                    font.hintingPreference: Font.PreferNoHinting
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: mouseArea2
                    hoverEnabled: true
                    anchors.fill: btnHotkeys
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {

                        if (hotkeys_dialog.visible) {
                            hotkeys_dialog.close()
                        } else {
                            hotkeys_dialog.show()
                        }
                    }
                }
            }*/

        }
    }
}

