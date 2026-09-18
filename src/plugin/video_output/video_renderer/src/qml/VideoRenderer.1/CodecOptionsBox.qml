// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import xstudio.qml.models 1.0
import xStudio 1.0


RowLayout {
    
    id: root
    property string attr_name
    property string default_attr_name

    XsAttributeValue {
        id: __options
        attributeTitle: attr_name
        model: video_render_attrs
    }
    property alias user_options: __options.value

    XsAttributeValue {
        id: __default_options
        attributeTitle: default_attr_name
        model: video_render_attrs
    }
    property alias default_options: __default_options.value

    onUser_optionsChanged: rebuild()
    onDefault_optionsChanged: rebuild()

    property var attr_options: []
    function rebuild() {
        
        if (user_options == undefined || default_options == undefined) return

        var opts = []
        if (default_options) {
            for (var i = 0; i < default_options.length; ++i) {
                opts.push(default_options[i][0])
            }
        }
        for (var i = 1; i < user_options.length; ++i) {
            opts.push(user_options[i][0])
        }
        attr_options = opts

        // pick the last profile that the user chose
        var idx = attr_options.indexOf(user_options[0])
        if (idx != -1) {
            combo_box.currentIndex = idx
        }
    }

    function get_bit_depth() {
        var rt = "8 bits"
        for (var i = 1; i < user_options.length; ++i) {
            if (user_options[i][0] == combo_box.currentText) {
                if (user_options[i].length == 3) {
                    rt = user_options[i][2]
                }
            }
        }
        return rt;
    }

    function get_codec_options() {
        var rt
        for (var i = 0; i < default_options.length; ++i) {
            if (default_options[i][0] == combo_box.currentText) {

                rt = [default_options[i][1], default_options[i][2]]
                // store the chosen option in the first element
                // of the 'user_options' attribute value so that it
                // persists between sessions
                var opts = user_options
                opts[0] = combo_box.currentText
                user_options = opts
                break;
            }
        }
        if (rt == undefined) {
            for (var i = 1; i < user_options.length; ++i) {
                if (user_options[i][0] == combo_box.currentText) {

                    rt = [user_options[i][1], user_options[i][2]]
                    // store the chosen option in the first element
                    // of the 'user_options' attribute value so that it
                    // persists between sessions
                    var opts = user_options
                    opts[0] = combo_box.currentText
                    user_options = opts

                }
            }
        }
        if (!rt) {
            dialogHelpers.errorDialogFunc("Render Video Error", "Failed to match chosen codec setting to available list of presets.")
        }
        return rt
    }

    function get_preset_name() {
        return combo_box.currentText;
    }

    XsComboBox { 

        id: combo_box
        model: attr_options
        Layout.preferredHeight: widgetHeight
        Layout.minimumWidth: 200
        Layout.fillHeight: true
    }

    Loader {
        id: loader        
    }

    Component {
        id: vid_config_dlg
        VidCodecOptionsConfigDialog {
            title: "Configure FFMPEG Video Encoding Presets"
            attr_name: root.attr_name
            default_attr_name: root.default_attr_name
        }
    }

    function openCodecConfigDlg() {
        loader.sourceComponent = vid_config_dlg
        loader.item.visible = true
    }

    function get_choice() {
        var opts = user_options
        opts[0] = combo_box.currentText
        user_options = opts
        return combo_box.currentText
    }

    XsPrimaryButton{ 
        
        id: editBtn
        Layout.preferredWidth: height
        Layout.fillHeight: true

        imgSrc: "qrc:///icons/edit.svg"

        onClicked: {
            openCodecConfigDlg()
        }
    }

}