// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

// Experimental / WIP - this widget allows us to load the xSTUDIO viewport widget
// into a QQuickWidget, such that xSTUDIO can be embedded into a PySide interface

XsViewport {

    id: viewport
    anchors.fill: parent
    has_context_menu: false
    
    XsPlayhead {
        id: playhead
        uuid: viewport.playheadUuid
    }
    property alias viewportPlayhead: playhead

}