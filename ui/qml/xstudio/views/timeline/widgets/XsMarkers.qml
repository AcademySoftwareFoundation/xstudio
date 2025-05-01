// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import QtQuick.Shapes 1.12
import xStudio 1.0

Rectangle {
	id: control
	property int start: 0
    property int duration: 0
    property real fractionOffset: 0
    property real tickWidth: (control.width / duration)
    property var model
    property int markerWidth: 6

	color: "transparent"

    XsToolTip {
        id: tt
        //visible: hover.hovered && !theTimeline.timelineMarkerMenu.visible
        width: 200
        height: 200
        delay: 0
        x: -width/2
        y: parent.height

    }

    // From QT docs:
    // As a general rule, scenes should avoid using separate Shape items when it is
    // not absolutely necessary. Prefer using one Shape item with multiple
    // ShapePath elements over multiple Shape items.
    //
    // However - it's not possible to drive the contents of a Shape by a model!
    // ShapePath is not an Item type, and therefore can't be generated in a
    // Repeater .... which is infuriating.
    // Hence the below, which is awkward but much better than putting a Shape
    // in a repeater

    property var marker_positions: []

    property var highlight_index: -1
    property var highlight_x: -100.0
    property var highlight_y: -100.0


    Component {

        id: the_shape
        Shape {

            id: shape
            width: control.width
            height: control.height
            containsMode: Shape.FillContains

            Instantiator {

                model: control.model
                onObjectAdded: (index, object) => {
                    shape.data.push(object)
                }

                ShapePath {

                    strokeWidth: 1
                    strokeColor: helpers.saturate(flagRole, 0.4)
                    fillColor: strokeColor
                    startX: (((startRole / rateRole) - start ) * tickWidth) - fractionOffset
                    startY: -layerRole * (shape.height * 0.5) + shape.height
                    property var idxInModel: index

                    PathLine {relativeX: -markerWidth/2; relativeY: -shape.height}
                    PathLine {relativeX: markerWidth; relativeY: 0}
                }
            }

            function under_mouse_idx(mousex) {
                var idx = __under_mouse_idx(mousex)
                if (idx != -1) {
                    return data[idx].idxInModel
                }
                return -1
            }

            function under_mouse_pos(mousex) {
                var idx = __under_mouse_idx(mousex)
                if (idx != -1) {
                    return Qt.point(data[idx].startX, data[idx].startY)
                }
                return Qt.point(-100.0, -100.0)
            }

            function __under_mouse_idx(mousex) {
                // mouse hover detection
                var hoverIdx = -1
                var fmin = 10.0
                for (var i = 0; i < data.length; ++i) {
                    var f = Math.abs(mousex-data[i].startX)
                    if (f < fmin) {
                        fmin = f
                        hoverIdx = i
                    }
                }
                return hoverIdx
            }

        }
    }

    Loader {
        id: loader
        property var d: control.model.length
        onDChanged: {
            // forces rebuild of the Shape. (Shape does not support a dynmically
            // changing number of Paths)
            sourceComponent = undefined
            sourceComponent = the_shape
        }
    }

    property var highlight_pos: Qt.point(-100.0,-100.0)
    onHighlight_posChanged: {
        tt.hide()
    }

    // under mouse highlight shape
    Shape {
        anchors.fill: parent
        height: control.height
        containsMode: Shape.FillContains
        visible: highlight_pos.y > -90.0
        ShapePath {

            id: highlightShape
            strokeWidth: 1
            strokeColor: XsStyleSheet.accentColor
            fillColor: XsStyleSheet.accentColor

            startX: highlight_pos.x
            startY: highlight_pos.y

            PathLine {relativeX: -markerWidth/2; relativeY: -control.height}
            PathLine {relativeX: markerWidth; relativeY: 0}
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onContainsMouseChanged: {
            if (!containsMouse) {
                highlight_pos = Qt.point(-100.0, -100.0)
            }
        }
        onPositionChanged: (mouse)=> {

            if (loader.item) {
                highlight_pos = loader.item.under_mouse_pos(mouseX)
            }

        }
        onPressed: mouse => {
            var highlight_index = loader.item.under_mouse_idx(mouseX)
            if (highlight_index != -1) {
                var idx = model.index(highlight_index, 0)
                if(mouse.buttons == Qt.RightButton) {
                    tt.hide()
                    theTimeline.showMarkerMenu(mouse.x, mouse.y, idx, control)
                } else {
                    if (idx.valid) {
                        var comment = model.get(idx, "commentRole")
                        var commentstr
                        try {
                            commentstr = JSON.stringify(comment, null, 2)
                        } catch (e) {
                            commentstr = "" + comment
                        }
                        var nameRole = model.get(idx, "nameRole")
                        tt.x = mouse.x
                        tt.y = mouse.y
                        tt.text = nameRole + "\n\n" + commentstr
                        tt.show()
                    }
                }
            } else {
                mouse.accepted = false
            }
        }
    }

}