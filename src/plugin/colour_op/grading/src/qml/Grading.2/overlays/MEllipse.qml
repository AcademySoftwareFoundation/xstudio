import QtQuick
import QtQuick.Shapes 1.6

import "VecLib.js" as VL

Item {
    id: root

    property var id
    property var canvas
    property real viewScale: 1
    // Qt's shape don't support drawing with sub pixel coordinates very well.
    // We provide an arbitrary scale here that can applied externally.
    property real drawScale: 200
    property real strokeWidth: 2 / viewScale * drawScale
    property real handleSize: 8 / viewScale * drawScale
    property real handleDetectSize: 10 / viewScale
    property real handleRadius: 5 / viewScale * drawScale
    property var handleColor: interacting ? "lime" : "red"
    property bool interacting: activeShape == modelIndex
    readonly property var shape: shape
    readonly property var shapePath: sp


    transform: [
        Scale { xScale: 1.0 / root.drawScale; yScale: 1.0 / root.drawScale }
    ]

    Shape {
        id: shape

        ShapePath {
            id: sp
            strokeColor: "gray"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"

            property var center: Qt.point(
                modelValue.center[2] * drawScale,
                modelValue.center[3] * drawScale * -1.0)
            property var radius: Qt.point(
                modelValue.radius[2] * drawScale,
                modelValue.radius[3] * drawScale)
            property real angle: modelValue.angle

            // Derived properties for convenience
            property var left: VL.rotateVec(Qt.point(center.x - radius.x, center.y), center, angle)
            property var top: VL.rotateVec(Qt.point(center.x, center.y - radius.y), center, angle)
            property var right: VL.rotateVec(Qt.point(center.x + radius.x, center.y), center, angle)
            property var bottom: VL.rotateVec(Qt.point(center.x, center.y + radius.y), center, angle)

            // Used during communication to backend
            readonly property var points: [center, radius, Qt.point(angle, angle)]

            // Use PathArc as PathAngleArc doesn't seem to support rotation
            startX: left.x
            startY: left.y

            PathArc {
                direction : PathArc.Clockwise
                radiusX : sp.radius.x
                radiusY : sp.radius.y
                useLargeArc : false
                x : sp.right.x
                y : sp.right.y
                xAxisRotation : sp.angle
            }

            PathArc {
                direction : PathArc.Clockwise
                radiusX : sp.radius.x
                radiusY : sp.radius.y
                useLargeArc : false
                x : sp.left.x
                y : sp.left.y
                xAxisRotation : sp.angle
            }

        }
    }

    function mouseReleased(buttons) {
        if (hovered_object) {
            attrs.interacting = false
            hovered_object = undefined
        }
    }

    property var hovered_object

    function mouseMoved(mousePosition, buttons, modifiers) {

        if (hovered_object && buttons == 1) {

            hovered_object.drag(mousePosition, modifiers)

        } else if (buttons == 0) {

            // mouse over/hover logic
            var nh = undefined
            var dist_to_handle = handleDetectSize
            for (var i = 0; i < handles.children.length; ++i) {

                var d = VL.subtractVec(
                    Qt.point(mousePosition.x, -mousePosition.y),
                    VL.scaleVec(handles.children[i].pos,1.0/drawScale)
                )
                var l = VL.lengthVec(d)
                if (l < dist_to_handle) {
                    dist_to_handle = l
                    nh = handles.children[i]
                }
            }

            if (nh != hovered_object) {
                hovered_object = nh
            }

        }
    }

    Item {

        id: handles

        Rectangle {
            id: centre_handle
            property bool hovered: hovered_object == centre_handle
            property var pos: sp.center
            x: pos.x - width / 2
            y: pos.y - height / 2
            radius: root.handleRadius
            color: hovered ? "yellow" : root.handleColor
            width: root.handleSize
            height: root.handleSize
            function drag(position, modifiers) {
                var v = modelValue;
                v.center[2] = position.x;
                v.center[3] = position.y;
                updateModelValue(v);
            }
            function dragstart(position, modifiers) {}
        }

        Rectangle {
            id: right_handle
            property bool hovered: hovered_object == right_handle
            property var pos: sp.right
            x: pos.x - width / 2
            y: pos.y - height / 2
            radius: root.handleRadius
            color: hovered ? "yellow" : root.handleColor
            width: root.handleSize
            height: root.handleSize
            property var ref_point
            function dragstart(position, modifiers) {

                var rd = VL.rotateVec(Qt.point(-modelValue.radius[2], 0), Qt.point(0, 0), sp.angle)
                ref_point = {"A": VL.addVec(Qt.point(modelValue.center[2], modelValue.center[3]), rd),
                            "B": Qt.point(modelValue.center[2], modelValue.center[3])}
            }

            function drag(position, modifiers) {
                
                var v = modelValue;
                var d = VL.subtractVec(position, ((modifiers & Qt.ShiftModifier) ? ref_point.A : ref_point.B));
                var norm = VL.rotateVec(Qt.point(1, 0), Qt.point(0, 0), sp.angle);
                var fac = Math.max(VL.dotProduct(d, norm), 0.001);
                if (modifiers & Qt.ShiftModifier) {

                    v.radius[2] = fac*0.5
                    v.center[2] = ref_point.A.x+VL.scaleVec(norm, -fac*0.5).x
                    v.center[3] = ref_point.A.y+VL.scaleVec(norm, -fac*0.5).y
                                        
                } else {                

                    v.radius[2] = fac

                }
                updateModelValue(v);

            }
        }

        Rectangle {
            id: top_handle
            property bool hovered: hovered_object == top_handle
            property var pos: sp.top
            x: pos.x - width / 2
            y: pos.y - height / 2
            radius: root.handleRadius
            color: hovered ? "yellow" : root.handleColor
            width: root.handleSize
            height: root.handleSize
            property var ref_point
            function dragstart(position, modifiers) {

                var rd = VL.rotateVec(Qt.point(0, modelValue.radius[3]), Qt.point(0, 0), sp.angle)
                ref_point = {"A": VL.addVec(Qt.point(modelValue.center[2], modelValue.center[3]), rd),
                            "B": Qt.point(modelValue.center[2], modelValue.center[3])}
            }

            function drag(position, modifiers) {
                
                var v = modelValue;
                var d = VL.subtractVec(position, ((modifiers & Qt.ShiftModifier) ? ref_point.A : ref_point.B));
                var norm = VL.rotateVec(Qt.point(0, -1), Qt.point(0, 0), sp.angle);
                var fac = Math.max(VL.dotProduct(d, norm), 0.001);
                if (modifiers & Qt.ShiftModifier) {

                    v.radius[3] = fac*0.5
                    v.center[2] = ref_point.A.x+VL.scaleVec(norm, fac*0.5).x
                    v.center[3] = ref_point.A.y+VL.scaleVec(norm, fac*0.5).y
                                        
                } else {                

                    v.radius[3] = fac

                }
                updateModelValue(v);

            }
        }

        Rectangle {
            id: rotate_handle
            property bool hovered: hovered_object == rotate_handle
            property var pos: VL.addVec(sp.center, VL.scaleVec(VL.subtractVec(sp.right, sp.center), 0.5))
            x: pos.x - width / 2
            y: pos.y - height / 2
            radius: root.handleRadius
            color: hovered ? "yellow" : root.handleColor
            width: root.handleSize
            height: root.handleSize
            function dragstart(position, modifiers) {}
            function drag(position, modifiers) {
                
                var v = modelValue;
                var centre = Qt.point(v.center[2], v.center[3])
                var d = VL.subtractVec(position, centre);
                v.angle = (Math.atan2(d.y, d.x) * 180) / Math.PI
                updateModelValue(v);

            }
        }
    }

    function mousePressed(position, buttons, modifiers) {
        if (hovered_object && buttons == 1) {
            attrs.interacting = true
            set_interaction_shape(modelIndex)
            hovered_object.dragstart(position, modifiers)
        }
    }

    function mouseDoubleClicked(mousePosition, buttons, modifiers) {
    }

    // Rotation handle
    Item {
        id: rr_rotation

        property bool hovering: false
        property bool interacting: false

        x: sp.center.x
        y: sp.center.y - height / 2

        width: sp.radius.x / 2
        height: root.handleDetectSize / 2
        transformOrigin: Item.Left
        rotation: sp.angle

        Rectangle {
            anchors.centerIn: parent
            width: sp.radius.x / 2
            height: root.handleSize / 2
            radius: root.handleRadius / 4
            color: rr_rotation.hovering || interacting ? "yellow" : root.handleColor
        }

    }

}
