import QtQuick
import QtQuick.Shapes 1.6

import "VecLib.js" as VL

Item {
    id: root
    property var canvas
    property real viewScale: 1
    property real strokeWidth: 2 / viewScale
    property real handleDetectSize: 10 / viewScale
    property real handleSize: 8 / viewScale
    property real handleRadius: 5 / viewScale
    property var handleColor: interacting ? "lime" : "red"
    readonly property var shape: shape
    readonly property var shapePath: sp
    property bool under_construction: false
    property bool interacting: activeShape == modelIndex

    function addPoint(pt) {

        var rpt = Qt.point(pt.x, -pt.y)
        var segmentsDist = 0.5
        var idx = -1
        var n_segments = loop_points.length-1
        for (var i = 0; i < n_segments; ++i) {
            var r = VL.distToSegment2(
                loop_points[i],
                loop_points[i+1],
                rpt)
            if (r < segmentsDist) {
                segmentsDist = r
                idx = i;
            }
        }
        if (idx != -1) {
                
            var vv = modelValue
            vv.points.splice(idx+1, 0, [
                "vec2",
                "1",
                pt.x,
                pt.y
            ])
            updateModelValue(vv)

        }

    }


    function removePoint(idx) {
        // Polygon needs 3 edges minimum
        if (modelValue.points.length >= 4) {
            var vv = modelValue
            vv.points.splice(idx, 1)
            updateModelValue(vv)
        }

    }

    property var points: []
    property var loop_points: []
    property var mv: modelValue.points
    onMvChanged: updatePoints()
        
    function updatePoints() {
        var pp = []
        for (var i = 0; i < modelValue.points.length; ++i) {
            pp.push(Qt.point(modelValue.points[i][2], -modelValue.points[i][3]))
        }

        if (!under_construction && pp.length) {

            // add centre point
            var p = VL.centerOfPoints(pp)
            pp.push(p)
            points = pp
            // have to avoid copy by reference here as we manipulate pp for loop_points
            var pp2 = []
            for (var i = 0; i < pp.length; ++i) pp2.push(pp[i]);
            if (pp2.length) {
                // for the shape path loop, replace centre point with
                // the first point
                pp2[pp2.length-1] = pp2[0]
            }
            loop_points = pp2

        } else {

            points = pp
            loop_points = pp

        }

    }

    Shape {
        id: shape

        ShapePath {
            id: sp
            strokeColor: "gray"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"

            PathPolyline {
                path: loop_points
            }

        }
    }

    property var under_mouse_pt_index

    function pointUnderMouseIndex(mousePosition) {

        var d = root.handleDetectSize
        var idx = -1;
        for (var i = 0; i < points.length; ++i) {
            var dx = points[i].x-mousePosition.x
            var dy = points[i].y+mousePosition.y
            var dist = Math.sqrt(dx*dx + dy*dy);
            if (dist < d) {
                d = dist
                idx = i
            }

        }
        return idx
    }

    function mouseReleased(buttons) {
        under_mouse_pt_index = -1
    }

    property var drag_start
    property var points_before_drag

    function mouseMoved(mousePosition, buttons, modifiers) {

        if (buttons != 1) {
            under_mouse_pt_index = pointUnderMouseIndex(mousePosition)
        } else if (under_mouse_pt_index != -1 && !under_construction) {
            var delta = Qt.point(mousePosition.x-drag_start.x, mousePosition.y-drag_start.y)
            var vv = modelValue
            if (under_mouse_pt_index == vv.points.length || (modifiers & Qt.AltModifier)) {
                // center point / Alt drag to move all points
                for (var i = 0; i < vv.points.length; ++i) {
                    vv.points[i] = [
                        "vec2",
                        "1",
                        points_before_drag[i].x + delta.x,
                        -points_before_drag[i].y + delta.y
                    ]
                }
                updateModelValue(vv)
            } else {
                vv.points[under_mouse_pt_index] = [
                    "vec2",
                    "1",
                    points_before_drag[under_mouse_pt_index].x + delta.x,
                    -points_before_drag[under_mouse_pt_index].y + delta.y
                ]
                updateModelValue(vv)
            }
        }
    }

    function mousePressed(position, buttons, modifiers) {

        if (!under_construction) {
            if (under_mouse_pt_index == -1) return
            attrs.interacting = true
            set_interaction_shape(modelIndex)
            if ((modifiers & Qt.ShiftModifier) && under_mouse_pt_index < modelValue.points.length) {
                removePoint(under_mouse_pt_index)
            } else {
                points_before_drag = points
                drag_start = position
            }
            return true
        } else {
            if (under_mouse_pt_index == 0 && modelValue.points.length >= 3) {
                finalizePolygon()
                return true
            } else if (under_mouse_pt_index == -1) {
                // add a point
                var vv = modelValue
                vv.points.push([
                    "vec2",
                    "1",
                    position.x,
                    position.y])
                updateModelValue(vv)
                updatePoints()
                return true;
            }
        }
        return false
    }

    function mouseDoubleClicked(mousePosition, buttons, modifiers) {
        if (under_mouse_pt_index != -1) {
            removePoint(under_mouse_pt_index)
        } else {
            addPoint(mousePosition)
        }
    }

    Repeater {

        model: points

        Rectangle {

            property bool hovering: under_mouse_pt_index == index
            x: points[index].x-width/2
            y: points[index].y-width/2
            radius: width/2
            color: hovering ? "yellow" : root.handleColor
            width: root.handleSize
            height: root.handleSize            
    
        }
    }

}
