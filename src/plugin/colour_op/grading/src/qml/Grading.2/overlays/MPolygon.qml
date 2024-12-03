import QtQuick 2.14
import QtQuick.Shapes 1.6

import "VecLib.js" as VL

Item {
    id: root

    property var id
    property var canvas
    property real viewScale: 1
    property real strokeWidth: 2 / viewScale
    property real handleDetectSize: 32 / viewScale
    property real handleSize: 8 / viewScale
    property real handleRadius: 5 / viewScale
    property var handleColor: isSelected() ? "lime" : "red"
    property bool interacting: repeater.interacting || rr_center.interacting
    property bool hovering: repeater.hovering || rr_center.hovering
    readonly property var shape: shape
    readonly property var shapePath: sp

    // Default implementation
    function isSelected() {
        return true;
    }

    function addPoint(pt) {
        var segmentsDist = [];
        for (var i = 0; i < shapePath.points.length; ++i) {
            segmentsDist.push(VL.distToSegment2(
                shapePath.points[i],
                shapePath.points[(i+1)%shapePath.points.length],
                pt));
        }

        var indexMin = segmentsDist.indexOf(Math.min(...segmentsDist));
        shapePath.points.splice(indexMin+1, 0, pt);

        shapePath.refresh();
        shapePath.updateBackend(true);
    }

    function removePoint(idx) {
        // Polygon needs 3 edges minimum
        if (sp.points.length >= 4) {
            var newPoints = [];
            for (var i = 0; i < sp.points.length; ++i) {
                if (i != idx) {
                    newPoints.push(sp.points[i]);
                }
            }
            sp.points = newPoints;
        }

        shapePath.refresh();
        shapePath.updateBackend(true);
    }

    Shape {
        id: shape

        ShapePath {
            id: sp
            strokeColor: "gray"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"

            property var points: []
            property var pointsLoop: []

            PathPolyline {
                id: ppl
                path: sp.pointsLoop

                onPathChanged: sp.updateBackend(false)
            }

            function refresh() {
                refreshLoop();
                points = points;
            }

            function refreshLoop() {
                if (points.length > 0) {
                    pointsLoop = points.slice();
                    pointsLoop.push(points[0]);
                }
                pointsLoop = pointsLoop;
            }

            onPointsChanged: {
                refreshLoop();
                sp.updateBackend(false)
            }

            function updateBackend(force) {
                root.updateBackend(force)
            }

            Component.onCompleted: {
                // Force backend update on init
                updateBackend(true)
            }
        }
    }

    Item {
        id: selector

        Repeater {
            id: repeater

            model: sp.points.length

            property var hovering: false
            property var interacting: false

            Item {
                id: rr

                property var modelIndex: index

                property bool hovering: false
                property bool interacting: false

                onHoveringChanged: {
                    repeater.hovering = hovering;
                }
                onInteractingChanged: {
                    repeater.interacting = interacting;
                }

                x: sp.points[modelIndex].x - width / 2
                y: sp.points[modelIndex].y - height / 2

                width: root.handleDetectSize
                height: root.handleDetectSize

                Rectangle {
                    anchors.centerIn: parent
                    radius: root.handleRadius
                    color: hovering || interacting ? "yellow" : root.handleColor
                    width: root.handleSize
                    height: root.handleSize
            
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    onEntered: rr.hovering = true
                    onExited: rr.hovering = false
                    onPressed: {
                        rr.interacting = true

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            rr.interacting = false;
                            rr.hovering = false;
                            removePoint(rr.modelIndex);
                        }
                        else if (mouse.modifiers & Qt.AltModifier) {
                            cursorShape = Qt.OpenHandCursor;
                        }
                        else {
                            cursorShape = Qt.ArrowCursor;
                        }
                    }
                    onDoubleClicked: {
                        rr.interacting = false;
                        rr.hovering = false;
                        removePoint(rr.modelIndex);
                    }
                    onReleased: {
                        rr.interacting = false
                        cursorShape = Qt.ArrowCursor;
                    }
                    onPositionChanged: {
                        if (rr.interacting) {
                            var pt = mapToItem(rr.parent, mouse.x, mouse.y);

                            if (mouse.modifiers & Qt.AltModifier) {
                                var offset = VL.subtractVec(pt, sp.points[rr.modelIndex]);
                                for (var i = 0; i < sp.points.length; ++i) {
                                    sp.points[i] = VL.addVec(sp.points[i], offset);
                                }
                            }
                            else {
                                sp.points[rr.modelIndex] = Qt.point(pt.x, pt.y);
                            }

                            sp.refresh();
                        }
                    }
                }
            }
        }

        // Center
        Item {
            id: rr_center

            property bool hovering: false
            property bool interacting: false

            x: center.x - width / 2
            y: center.y - height / 2

            property point center: VL.centerOfPoints(sp.points)

            width: root.handleDetectSize
            height: root.handleDetectSize

            Rectangle {
                anchors.centerIn: parent
                radius: root.handleRadius
                color: hovering || interacting ? "yellow" : root.handleColor
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

                        var offset = VL.subtractVec(pt, rr_center.center);
                        for (var i = 0; i < sp.points.length; ++i) {
                            sp.points[i] = VL.addVec(sp.points[i], offset);
                        }

                        sp.refresh();
                    }
                }
            }
        }
    }
}
