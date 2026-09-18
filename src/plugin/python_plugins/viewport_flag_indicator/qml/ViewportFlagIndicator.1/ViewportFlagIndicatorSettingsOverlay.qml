// SPDX-License-Identifier: Apache-2.0

import QtQuick.Layouts
import QtQuick
import QuickFuture 1.0
import QuickPromise 1.0

// These imports are necessary to have access to custom QML types that are
// part of the xSTUDIO UI implementation.
import xStudio 1.0
import xstudio.qml.models 1.0

// This overlay is as simple as it gets. We're just putting a circle in a corner
// of the screen that shows the 'flag' colour that is set on the media item that
// is on screen. If no flag is set, we don't show anything
Item {
    
    width: indicatorSize*scalingFactor
    height: width

    Rectangle {

        width: indicatorSize*scalingFactor
        height: width
        radius: width/2
        color: media_item_hud_data ? media_item_hud_data : "transparent"
        x: xOffset*scalingFactor
        y: yOffset*scalingFactor
    }

    XsModuleData {
        id: flags_data
        modelDataName: view.name + " flags"
    }

    function setCWidth(ww) {
        if (ww > cwidth) cwidth = ww
    }

    // access the 'dnmask_settings' attribute group
    XsModuleData {
        id: vp_flag_indicator_settings
        modelDataName: "vp_flag_indicator"
    }

    XsAttributeValue {
        id: __indicator_size
        attributeTitle: "Indicator Size"
        model: vp_flag_indicator_settings
    }
    property alias indicatorSize: __indicator_size.value

    XsAttributeValue {
        id: __x_offset
        attributeTitle: "X Offset"
        model: vp_flag_indicator_settings
    }
    property alias xOffset: __x_offset.value

    XsAttributeValue {
        id: __y_offset
        attributeTitle: "Y Offset"
        model: vp_flag_indicator_settings
    }
    property alias yOffset: __y_offset.value

    /*XsAttributeValue {
        id: __flag_colours
        attributeTitle: "Flag Colours"
        model: vp_flag_indicator_settings
    }
    property alias flagColours: __flag_colours.value*/

}