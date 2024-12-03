// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QuickFuture 1.0
import QuickPromise 1.0

// These imports are necessary to have access to custom QML types that are
// part of the xSTUDIO UI implementation.
import xStudio 1.0
import xstudio.qml.models 1.0

// This overlay is as simple as it gets. We're just putting a circle in a corner
// of the screen that shows the 'flag' colour that is set on the media item that
// is on screen. If no flag is set, we don't show anything
Rectangle {

    width: indicatorSize
    height: width
    radius: indicatorSize/2
    color: flag ? flag : "transparent"

    property var flag: currentOnScreenMediaData.values.flagColourRole

    

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

    /*XsAttributeValue {
        id: __flag_colours
        attributeTitle: "Flag Colours"
        model: vp_flag_indicator_settings
    }
    property alias flagColours: __flag_colours.value*/

}