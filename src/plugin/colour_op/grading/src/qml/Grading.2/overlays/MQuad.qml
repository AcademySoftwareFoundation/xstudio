import QtQuick 2.14
import QtQuick.Shapes 1.6

import "VecLib.js" as VL

Item {
    id: root

    property var id
    property var canvas
    property real viewScale: 1
    property real strokeWidth: 2 / viewScale
    property real handleSize: 8 / viewScale
    property real handleRadius: 5 / viewScale
    property var handleColor: isSelected() ? "lime" : "red"
    property bool interacting: rr_bl.interacting || rr_tl.interacting || rr_tr.interacting || rr_br.interacting || rr_center.interacting
    readonly property var shape: shape
    readonly property var shapePath: sp

    // Default implementation
    function isSelected() {
        return true;
    }

    Shape {
        id: shape

        ShapePath {
            id: sp
            strokeColor: "gray"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"

            property var bl
            property var tl
            property var tr
            property var br

            // For convenience
            readonly property var points: [bl, tl, tr, br]

            PathPolyline {
                id: ppl
                path: [
                    sp.bl,
                    sp.tl,
                    sp.tr,
                    sp.br,
                    sp.bl,
                ]

                onPathChanged: sp.updateBackend()
            }

            onBlChanged: sp.updateBackend()
            onTlChanged: sp.updateBackend()
            onTrChanged: sp.updateBackend()
            onBrChanged: sp.updateBackend()

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

        // TODO: Way to deduplicated the corners?
        // Maybe using a Repeater / Loader?

        // Bottom left corner
        Rectangle {
            id: rr_bl

            property bool hovering: false
            property bool interacting: false

            x: sp.bl.x - width / 2
            y: sp.bl.y - height / 2

            width: root.handleSize
            height: root.handleSize
            radius: root.handleRadius
            color: hovering || interacting ? "yellow" : root.handleColor

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_bl.hovering = true
                onExited: rr_bl.hovering = false
                onPressed: {
                    rr_bl.interacting = true

                    if (mouse.modifiers & Qt.ShiftModifier) {
                        cursorShape = Qt.SizeBDiagCursor;
                    } else if (mouse.modifiers & Qt.AltModifier) {
                        cursorShape = Qt.OpenHandCursor;
                    } else {
                        cursorShape = Qt.ArrowCursor;
                    }
                }
                onReleased: {
                    rr_bl.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_bl.interacting) {
                        var pt = mapToItem(rr_bl.parent, mouse.x, mouse.y);

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            var prevprev = sp.tr;
                            var prev = sp.br;
                            var curr = sp.bl;
                            var next = sp.tl;
                            var nextnext = prevprev;
                            var [new_prev, new_next] = VL.extendSides(pt, prevprev, prev, curr, next, nextnext, root.handleRadius);

                            sp.br = new_prev;
                            sp.tl = new_next;
                        }
                        else if (mouse.modifiers & Qt.AltModifier) {
                            var offset = VL.subtractVec(pt, sp.bl);
                            sp.bl = VL.addVec(sp.bl, offset);
                            sp.tl = VL.addVec(sp.tl, offset);
                            sp.tr = VL.addVec(sp.tr, offset);
                            sp.br = VL.addVec(sp.br, offset);
                        }

                        var prevprev = sp.tr;
                        var prev = sp.br;
                        var next = sp.tl;
                        var nextnext = prevprev;
                        VL.checkConvex(pt, prevprev, prev, next, nextnext);

                        sp.bl = Qt.point(pt.x, pt.y);
                    }
                }
            }
        }

        // Top left corner
        Rectangle {
            id: rr_tl

            property bool hovering: false
            property bool interacting: false

            x: sp.tl.x - width / 2
            y: sp.tl.y - height / 2

            width: root.handleSize
            height: root.handleSize
            radius: root.handleRadius
            color: hovering || interacting ? "yellow" : root.handleColor

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_tl.hovering = true
                onExited: rr_tl.hovering = false
                onPressed: {
                    rr_tl.interacting = true

                    if (mouse.modifiers & Qt.ShiftModifier) {
                        cursorShape = Qt.SizeFDiagCursor;
                    } else if (mouse.modifiers & Qt.AltModifier) {
                        cursorShape = Qt.OpenHandCursor;
                    } else {
                        cursorShape = Qt.ArrowCursor;
                    }
                }
                onReleased: {
                    rr_tl.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_tl.interacting) {
                        var pt = mapToItem(rr_tl.parent, mouse.x, mouse.y);

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            var prevprev = sp.br;
                            var prev = sp.bl;
                            var curr = sp.tl;
                            var next = sp.tr;
                            var nextnext = prevprev;
                            var [new_prev, new_next] = VL.extendSides(pt, prevprev, prev, curr, next, nextnext, root.handleRadius);

                            sp.bl = new_prev;
                            sp.tr = new_next;
                        }
                        else if (mouse.modifiers & Qt.AltModifier) {
                            var offset = VL.subtractVec(pt, sp.tl);
                            sp.bl = VL.addVec(sp.bl, offset);
                            sp.tl = VL.addVec(sp.tl, offset);
                            sp.tr = VL.addVec(sp.tr, offset);
                            sp.br = VL.addVec(sp.br, offset);
                        }

                        var prevprev = sp.br;
                        var prev = sp.bl;
                        var next = sp.tr;
                        var nextnext = prevprev;
                        VL.checkConvex(pt, prevprev, prev, next, nextnext);

                        sp.tl = Qt.point(pt.x, pt.y);
                    }
                }
            }
        }

        // Top right corner
        Rectangle {
            id: rr_tr

            property bool hovering: false
            property bool interacting: false

            x: sp.tr.x - width / 2
            y: sp.tr.y - height / 2

            width: root.handleSize
            height: root.handleSize
            radius: root.handleRadius
            color: hovering || interacting ? "yellow" : root.handleColor

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_tr.hovering = true
                onExited: rr_tr.hovering = false
                onPressed: {
                    rr_tr.interacting = true

                    if (mouse.modifiers & Qt.ShiftModifier) {
                        cursorShape = Qt.SizeBDiagCursor;
                    } else if (mouse.modifiers & Qt.AltModifier) {
                        cursorShape = Qt.OpenHandCursor;
                    } else {
                        cursorShape = Qt.ArrowCursor;
                    }
                }
                onReleased: {
                    rr_tr.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_tr.interacting) {
                        var pt = mapToItem(rr_tr.parent, mouse.x, mouse.y);

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            var prevprev = sp.bl;
                            var prev = sp.tl;
                            var curr = sp.tr;
                            var next = sp.br;
                            var nextnext = prevprev;
                            var [new_prev, new_next] = VL.extendSides(pt, prevprev, prev, curr, next, nextnext, root.handleRadius);

                            sp.tl = new_prev;
                            sp.br = new_next;
                        }
                        else if (mouse.modifiers & Qt.AltModifier) {
                            var offset = VL.subtractVec(pt, sp.tr);
                            sp.bl = VL.addVec(sp.bl, offset);
                            sp.tl = VL.addVec(sp.tl, offset);
                            sp.tr = VL.addVec(sp.tr, offset);
                            sp.br = VL.addVec(sp.br, offset);
                        }

                        var prevprev = sp.bl;
                        var prev = sp.tl;
                        var next = sp.br;
                        var nextnext = prevprev;
                        VL.checkConvex(pt, prevprev, prev, next, nextnext);

                        sp.tr = Qt.point(pt.x, pt.y);
                    }
                }
            }
        }

        // Bottom right corner
        Rectangle {
            id: rr_br

            property bool hovering: false
            property bool interacting: false

            x: sp.br.x - width / 2
            y: sp.br.y - height / 2

            width: root.handleSize
            height: root.handleSize
            radius: root.handleRadius
            color: hovering || interacting ? "yellow" : root.handleColor

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: rr_br.hovering = true
                onExited: rr_br.hovering = false
                onPressed: {
                    rr_br.interacting = true

                    if (mouse.modifiers & Qt.ShiftModifier) {
                        cursorShape = Qt.SizeFDiagCursor;
                    } else if (mouse.modifiers & Qt.AltModifier) {
                        cursorShape = Qt.OpenHandCursor;
                    } else {
                        cursorShape = Qt.ArrowCursor;
                    }
                }
                onReleased: {
                    rr_br.interacting = false
                    cursorShape = Qt.ArrowCursor;
                }
                onPositionChanged: {
                    if (rr_br.interacting) {
                        var pt = mapToItem(rr_br.parent, mouse.x, mouse.y);

                        if (mouse.modifiers & Qt.ShiftModifier) {
                            var prevprev = sp.tl;
                            var prev = sp.tr;
                            var curr = sp.br;
                            var next = sp.bl;
                            var nextnext = prevprev;
                            var [new_prev, new_next] = VL.extendSides(pt, prevprev, prev, curr, next, nextnext, root.handleRadius);

                            sp.tr = new_prev;
                            sp.bl = new_next;
                        }
                        else if (mouse.modifiers & Qt.AltModifier) {
                            var offset = VL.subtractVec(pt, sp.br);
                            sp.bl = VL.addVec(sp.bl, offset);
                            sp.tl = VL.addVec(sp.tl, offset);
                            sp.tr = VL.addVec(sp.tr, offset);
                            sp.br = VL.addVec(sp.br, offset);
                        }

                        var prevprev = sp.tl;
                        var prev = sp.tr;
                        var next = sp.bl;
                        var nextnext = prevprev;
                        VL.checkConvex(pt, prevprev, prev, next, nextnext);

                        sp.br = Qt.point(pt.x, pt.y);
                    }
                }
            }
        }

        // Center
        Rectangle {
            id: rr_center

            property bool hovering: false
            property bool interacting: false

            x: center.x - width / 2
            y: center.y - height / 2

            property point center: VL.centerOfPoints(sp.points)

            width: root.handleSize
            height: root.handleSize
            radius: root.handleRadius
            color: hovering || interacting ? "yellow" : root.handleColor

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
                        sp.bl = VL.addVec(sp.bl, offset);
                        sp.tl = VL.addVec(sp.tl, offset);
                        sp.tr = VL.addVec(sp.tr, offset);
                        sp.br = VL.addVec(sp.br, offset);
                    }
                }
            }
        }
    }
}
