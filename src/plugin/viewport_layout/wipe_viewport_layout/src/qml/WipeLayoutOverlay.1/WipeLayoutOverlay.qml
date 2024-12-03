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
import "."

// Our Overlay is based on a transparent rectangle that simply fills the
// xSTUDIO view. Within this we draw the overlay graphics as required.
Item {

    // Note that viewport overlays are instanced by the Viewport QML instance
    // which has the id 'viewport' and is visible to us here. To learn more
    // about the viewport see files Xsview.qml and qml_view.cpp from
    // the xSTUDIO source code.

    property var crosshair_size: 74
    property var circle_size: 12
    opacity: 0.7

    id: control
    width: crosshair_size
    height: crosshair_size
    x: wipe_position.x*parent.width-crosshair_size/2
    y: wipe_position.y*parent.height-crosshair_size/2
    property bool hovered: ma.containsMouse || ma.pressed

    visible: title == viewportPlayhead.compare_mode && viewportPlayhead.numSubPlayheads > 1

    WipeShadowLine {
        height: crosshair_size/2-circle_size/2
        width: 1
        x: crosshair_size/2
    }

    WipeShadowLine {
        height: crosshair_size/2-circle_size/2
        width: 1
        y: crosshair_size-height
        x: crosshair_size/2
    }

    WipeShadowLine {
        width: crosshair_size/4-circle_size/2
        height: 1
        y: crosshair_size/2
        x: crosshair_size/2-crosshair_size/5
    }

    WipeShadowLine {
        width: crosshair_size/4-circle_size/2
        height: 1
        x: crosshair_size/2+circle_size/2
        y: crosshair_size/2
    }

    Rectangle {
        height: circle_size
        width: circle_size
        radius: circle_size/2
        x: parent.width/2+1-radius
        y: parent.width/2+1-radius
        color: "transparent"
        border.color: "black"
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            onPressed: {
                x0=mouse.x-width/2
                y0=mouse.y-height/2
            }
            property real x0
            property real y0
            onPositionChanged: {
                if (pressed) {
                    // wipe_position can't go outside the viewport or we'd lose
                    // it offscreen and wouldn't be able to get it back
                    var pt = mapToItem(view, mouse.x-x0, mouse.y-y0);
                    pt.x = Math.max(0.01, Math.min(0.99, pt.x/view.width))
                    pt.y = Math.max(0.01, Math.min(0.99, pt.y/view.height))
                    var a = wipe_position
                    a.x = pt.x
                    a.y = pt.y
                    wipe_position = a
                }
            }
        }
    
    }

    Rectangle {
        id: target
        height: circle_size
        width: circle_size
        radius: circle_size/2
        x: parent.width/2-radius
        y: parent.width/2-radius
        color: hovered ? palette.highlight : "transparent"
        border.color: "white"
    }

    // This accesses the attribute group called wipe_layout_attrs. It contains
    // just one attr ... see the cpp code for where this comes from
    XsModuleData {
        id: wipe_attrs
        modelDataName: "wipe_layout_attrs"
    }

    // Here we make an alias to attach to the Wipe attr that we need
    XsAttributeValue {
        id: __wipe_position
        attributeTitle: "Wipe Position"
        model: wipe_attrs
    }
    property alias wipe_position: __wipe_position.value

}
