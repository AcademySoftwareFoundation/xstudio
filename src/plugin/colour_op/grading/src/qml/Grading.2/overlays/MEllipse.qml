import QtQuick 2.14
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
    property real handleDetectSize: 32 / viewScale * drawScale
    property real handleRadius: 5 / viewScale * drawScale
    property var handleColor: isSelected() ? "lime" : "red"
    property bool interacting: rr_rotation.interacting || rr_center.interacting || rr_right.interacting || rr_top.interacting
    readonly property var shape: shape
    readonly property var shapePath: sp

    // Default implementation
    function isSelected() {
        return true;
    }

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

            property var center
            property var radius
            property real angle

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

            onRadiusChanged: updateBackend()
            onCenterChanged: updateBackend()
            onAngleChanged: updateBackend()

            function updateBackend() {
                root.updateBackend(false)
            }

            Component.onCompleted: {
                // Force backend update on init
                root.updateBackend(true)
            }
        }
    }

    Item {
        id: selector

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

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_rotation.hovering = true
                onExited: rr_rotation.hovering = false
                onPressed: rr_rotation.interacting = true
                onReleased: rr_rotation.interacting = false
                onPositionChanged: {
                    if (rr_rotation.interacting) {
                        var pt = mapToItem(rr_rotation.parent, mouse.x, mouse.y);

                        var newDir = VL.normVec(VL.subtractVec(pt, sp.center));
                        var currDir = VL.normVec(VL.subtractVec(sp.right, sp.center));
                        var rotDir = VL.crossProduct(newDir, currDir) >= 0 ? 1 : -1;
                        var angle = Math.acos(VL.dotProduct(newDir, currDir));
                        var degrees = angle * 180.0 / Math.PI;
                        sp.angle = (sp.angle + rotDir * degrees) % 360;
                    }
                }
                onDoubleClicked: {
                    sp.angle = 0;
                }
            }
        }

        // Center move
        Item {
            id: rr_center

            property bool hovering: false
            property bool interacting: false

            x: sp.center.x - width / 2
            y: sp.center.y - height / 2

            width: root.handleDetectSize
            height: root.handleDetectSize

            Rectangle {
                anchors.centerIn: parent
                radius: root.handleRadius
                color: rr_center.hovering || rr_center.interacting ? "yellow" : root.handleColor
                width: root.handleSize
                height: root.handleSize
        
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_center.hovering = true
                onExited: rr_center.hovering = false
                onPressed: {
                    rr_center.interacting = true
                    cursorShape = Qt.OpenHandCursor;
                }
                onReleased: {
                    rr_center.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_center.interacting) {
                        var pt = mapToItem(rr_center.parent, mouse.x, mouse.y);
                        sp.center = Qt.point(pt.x, pt.y);
                    }
                }
            }
        }

        // Right extend
        Item {
            id: rr_right

            property bool hovering: false
            property bool interacting: false

            x: sp.right.x - width / 2
            y: sp.right.y - height / 2

            width: root.handleDetectSize
            height: root.handleDetectSize

            Rectangle {
                anchors.centerIn: parent
                radius: root.handleRadius
                color: rr_right.hovering || rr_right.interacting ? "yellow" : root.handleColor
                width: root.handleSize
                height: root.handleSize
        
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_right.hovering = true
                onExited: rr_right.hovering = false
                onPressed: {
                    rr_right.interacting = true
                    cursorShape = Qt.SizeHorCursor;
                }
                onReleased: {
                    rr_right.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_right.interacting) {
                        var pt = mapToItem(rr_right.parent, mouse.x, mouse.y);

                        // Project onto ellipse "horizontal" axis
                        var toNewRight = VL.subtractVec(pt, sp.center);
                        var hDir = VL.normVec(VL.subtractVec(sp.right, sp.center));
                        // Make sure the ellipse is still well behaved (axis size >= 1)
                        var dot = Math.max(VL.dotProduct(toNewRight, hDir), 1.0);
                        pt = VL.addVec(sp.center, VL.scaleVec(hDir, dot));

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            sp.center = VL.addVec(sp.left, VL.scaleVec(VL.subtractVec(pt, sp.left), 0.5));
                            sp.radius = Qt.point(VL.lengthVec(VL.subtractVec(pt, sp.center)), sp.radius.y);
                        }
                        else {
                            var newRadius = VL.lengthVec(VL.subtractVec(pt, sp.center));
                            sp.radius = Qt.point(Math.max(newRadius, 1.0), sp.radius.y);
                        }
                    }
                }
            }
        }

        // Top extend
        Item {
            id: rr_top

            property bool hovering: false
            property bool interacting: false

            x: sp.top.x - width / 2
            y: sp.top.y - height / 2

            width: root.handleDetectSize
            height: root.handleDetectSize

            Rectangle {
                anchors.centerIn: parent
                radius: root.handleRadius
                color: rr_top.hovering || rr_top.interacting ? "yellow" : root.handleColor
                width: root.handleSize
                height: root.handleSize
        
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_top.hovering = true
                onExited: rr_top.hovering = false
                onPressed: {
                    rr_top.interacting = true
                    cursorShape = Qt.SizeVerCursor;
                }
                onReleased: {
                    rr_top.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_top.interacting) {
                        var pt = mapToItem(rr_top.parent, mouse.x, mouse.y);

                        // Project onto ellipse "vertical" axis
                        var toNewTop = VL.subtractVec(pt, sp.center);
                        var vDir = VL.normVec(VL.subtractVec(sp.top, sp.center));
                        // Make sure the ellipse is still well behaved (axis size >= 1)
                        var dot = Math.max(VL.dotProduct(toNewTop, vDir), 1.0);
                        pt = VL.addVec(sp.center, VL.scaleVec(vDir, dot));

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            sp.center = VL.addVec(sp.bottom, VL.scaleVec(VL.subtractVec(pt, sp.bottom), 0.5));
                            sp.radius = Qt.point(sp.radius.x, VL.lengthVec(VL.subtractVec(pt, sp.center)));
                        }
                        else {
                            var newRadius = VL.lengthVec(VL.subtractVec(pt, sp.center));
                            sp.radius = Qt.point(sp.radius.x, Math.max(newRadius, 1.0));
                        }

                    }
                }
            }
        }
    }
}
