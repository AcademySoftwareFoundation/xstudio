// SPDX-License-Identifier: Apache-2.0
import QtQml 2.14
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.impl 2.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
import QtQuick.Controls.Styles 1.2

import xStudio 1.0

Rectangle  {

    id: control

    property string title_abbr: title_
    property int gap: 3
    property var attr_activated: activated ? activated : false
    property var title_: title ? title : ""
    property var abbr_title_: abbr_title ? abbr_title : title_.slice(0,3) + "..."
    property var toolbar_position_: toolbar_position ? toolbar_position : -1.0
    property bool is_overridden: override_value ? (override_value != "" ? true : false) : false
    property var value_text: value ? value : ""
    property var display_value: is_overridden ? override_value : value_text
    property var short_display_value: display_value ? display_value.slice(0,3) + "..." : ""
    property bool fixed_width_font: false
    property var min_pad: 2
    property bool collapse: false
    property bool inactive : attr_enabled != undefined ? !attr_enabled : false
    property var custom_message_: custom_message != undefined ? custom_message : undefined
    property bool hovered: false
    property var actual_text: collapse_mode <= 1 ? display_value : short_display_value

    property var showHighlighted: hovered | (activated != undefined && activated)

    gradient: inactive ? null : showHighlighted ? styleGradient.accent_gradient : null
    color: showHighlighted ? XsStyle.indevColor : XsStyle.controlBackground

    onShowHighlightedChanged: {
        // TODO: expand button to collapseMode=0 when mouse hovered 
        if (showHighlighted) {
            status_bar.normalMessage(tooltip ? tooltip : "No Tooltip", title_, inactive)
        } else {
        }
    }

    function widget_clicked() {
        if (inactive && custom_message_) {
            if (popupOverride.opacity == 0) {
                popupOverride.x = control.width/2 - popupOverride.width/2
                popupOverride.target_y = control.y
                popupOverride.opacity = 1
            } else {
                popupOverride.opacity = 0
            }
        }
    }

    onHoveredChanged: {
        if (!hovered && popupOverride.opacity) {
            popupOverride.opacity = 0
        }
    }

    XsPopupInfoBox {
        id: popupOverride
        text: custom_message_
    }

    // opacity: parent.down ? 0.5 : 1.0
    Behavior on color {
        ColorAnimation {
            duration: 100
        }
    }

    property real fullyExpandedWidth: full_title_metrics.width + full_value_metrics.width + gap + min_pad
    property real shortTitleFullValueWidth: short_title_metrics.width + full_value_metrics.width + gap + min_pad
    property real shortTitleShortValueWidth: short_title_metrics.width + short_value_metrics.width + gap + min_pad
    property real shortTitleOnlyWidth: short_title_metrics.width

    property int suggestedCollapseMode: {
        if (width > fullyExpandedWidth) return 0;
        if (width > shortTitleFullValueWidth) return 1;
        if (width > shortTitleShortValueWidth) return 2;
        return 3;
    }

    property int collapse_mode: collapse ? 3 : suggestedCollapseMode

    TextMetrics {
        id:     full_title_metrics
        font:   title_widget.font
        text:   control.title_
    }

    TextMetrics {
        id:     short_title_metrics
        font:   title_widget.font
        text:   control.abbr_title_
    }

    TextMetrics {
        id:     full_value_metrics
        font:   value_widget.font
        text:   control.display_value ? control.display_value : ""
    }

    TextMetrics {
        id:     short_value_metrics
        font:   value_widget.font
        text:   control.short_display_value ? control.short_display_value : ""
    }

    Rectangle {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        color: "transparent"
        width: title_widget.width+value_widget.width*value_widget.opacity+gap*value_widget.opacity

        Text {

            id: title_widget
            text: collapse_mode == 0 ? title_ : abbr_title_

            anchors.left: parent.left

            // not sure why, but can't get the text to centre the usual way i.e. with centre anchors
            y: parent.height/2-height/2+1.5 

            font.pixelSize: XsStyle.popupControlFontSize
            font.family: XsStyle.controlTitleFontFamily
            color: showHighlighted ? XsStyle.controlColor : XsStyle.controlTitleColor
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

        }

        Text {

            id: value_widget

            text: actual_text ? actual_text : ""

            opacity: collapse_mode != 3
            visible: opacity > 0.2
            scale: opacity
            Behavior on opacity {
                NumberAnimation {duration:200}
            }

            anchors.bottom: title_widget.bottom
            anchors.right: parent.right

            font.pixelSize: XsStyle.popupControlFontSize+(fixed_width_font ? -1 : 0)
            font.family: fixed_width_font ? XsStyle.fixWidthFontFamily : XsStyle.controlTitleFontFamily
            color: XsStyle.controlColor
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter            

        }
    }
}
