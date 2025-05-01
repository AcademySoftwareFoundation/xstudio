// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick
import QuickFuture
import QuickPromise

// These imports are necessary to have access to custom QML types that are
// part of the xSTUDIO UI implementation.
import xStudio 1.0
import xstudio.qml.models 1.0

// This overlay is as simple as it gets. We're just putting a circle in a corner
// of the screen that shows the 'flag' colour that is set on the media item that
// is on screen. If no flag is set, we don't show anything
Item {

    id: root
    width: visible ? text_metrics.width : 0
    height: visible ? text_metrics.height : 0

    // Turning this off for QuickView windows, which won't work due to use
    // of currentOnScreenMediaData
    visible: isQuickview ? false : version_name != undefined && opacity != 0.0

    Rectangle {
        color: "black"
        opacity: 0.5
        anchors.fill: parent
    }

    property var media_info: currentOnScreenMediaData.values.mediaDisplayInfoRole
    property var version_name: media_info ? media_info[3] : ""

    XsText {

        id: thetext
        anchors.fill: parent
        font.pixelSize: font_size*view.width/1920
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: version_name
        color: font_colour
        onTextChanged: (text)=> {
            root.opacity = 1.0
            if (auto_hide && text != "") {
                auto_hide_timer.running = false
                auto_hide_timer.running = true
            }
        }

    }

    XsTimer {
        id: auto_hide_timer
        interval: hide_timeout*1000
        running: false
        repeat: false
        onTriggered: root.opacity = 0
    }

    TextMetrics {
        id:     text_metrics
        font:   thetext.font
        text:   thetext.text
    }


    // access the 'dnmask_settings' attribute group
    XsModuleData {
        id: onscreen_attrs
        modelDataName: "on_screen_version_name"
    }


    XsAttributeValue {
        id: __font_colour
        attributeTitle: "Font Colour"
        model: onscreen_attrs
    }
    property alias font_colour: __font_colour.value

    XsAttributeValue {
        id: __hide_timeout
        attributeTitle: "Auto Hide Timeout (seconds)"
        model: onscreen_attrs
    }
    property alias hide_timeout: __hide_timeout.value

    XsAttributeValue {
        id: __auto_hide
        attributeTitle: "Auto Hide"
        model: onscreen_attrs
    }
    property alias auto_hide: __auto_hide.value

    XsAttributeValue {
        id: __the_text
        attributeTitle: "Display Text"
        model: onscreen_attrs
    }
    property alias display_text: __the_text.value

    XsAttributeValue {
        id: __font_size
        attributeTitle: "Font Size"
        model: onscreen_attrs
    }
    property alias font_size: __font_size.value

    /*XsAttributeValue {
        id: __flag_colours
        attributeTitle: "Flag Colours"
        model: vp_flag_indicator_settings
    }
    property alias flagColours: __flag_colours.value*/

}