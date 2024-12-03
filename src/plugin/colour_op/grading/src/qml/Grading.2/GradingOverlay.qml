// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Shapes 1.6
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

import xStudio 1.0

Item {
    id: root

    GTAttributes {
        id: grd_attrs
    }

    MAttributes {
        id: attrs
    }

    XsModuleData {
        id: grading_tool_overlay_shapes
        modelDataName: "grading_tool_overlay_shapes"
    }

    property var viewportName: view.name
    property var ib: view.imageBoundariesInViewport[viewportPlayhead.keySubplayheadIndex]
    property var imageBox: ib ? ib : Qt.rect(0,0,0,0)
    property var ir: view.imageResolutions[viewportPlayhead.keySubplayheadIndex]
    property var imageResolution: ir ? ir : Qt.size(0, 0)
    property real imageAspectRatio: imageBox.width/imageBox.height
    property real viewportScale: 1.0
    property var viewportOffset: Qt.point(0, 0)

    property var tool_opened_count: grd_attrs.tool_opened_count
    property var mask_shapes_visible: attrs.mask_shapes_visible
    property var mask_selected_shape: attrs.mask_selected_shape
    property var drawing_action: attrs.drawing_action

    visible: mask_shapes_visible && tool_opened_count > 0

    property var polygon_init: attrs.polygon_init
    property var polygon_points: []
    property var polygon_shape

    property var activeShape: -1

    onActiveShapeChanged: {
        attrs.mask_selected_shape = activeShape;
    }

    onMask_selected_shapeChanged: {
        if (mask_selected_shape != activeShape) {
            activeShape = mask_selected_shape;
        }
    }

    onImageBoxChanged: {
        // OpenGL normalized device coordinates to window pixel coordinates
        viewportScale = imageBox.width / 2.0;
        viewportOffset =  Qt.point(
            imageBox.x + imageBox.width / 2.0,
            imageBox.y + imageBox.height / 2.0
        );
    }

    onPolygon_initChanged: {
        if (polygon_init) {
            startPolygon();
        } else {
            cleanupPolygon();
        }
    }

    function updateAttrItem(name, value) {
        if (activeShape >= 0 && activeShape < repeater.count) {
            repeater.itemAt(activeShape).updateAttr(name, value);
        }
    }

    function handleDoubleClick(mouse) {
        if (activeShape >= 0 && activeShape < repeater.count) {
            repeater.itemAt(activeShape).handleDoubleClick(mouse);
        }
    }

    // Event handling

    XsHotkey {
        sequence: "Escape"
        name: "unselect"
        context: "any"
        onActivated: {
            if (polygon_init) {
                cleanupPolygon();
            } else {
                activeShape = -1;
            }
        }
    }

    // indicates the 'current' (hero) image in multi image layours
    Rectangle {
        visible: viewportPlayhead.numSubPlayheads > 1
        x: imageBox.x        
        y: imageBox.y
        width: imageBox.width
        height: imageBox.height
        color: "transparent"
        border.color: palette.highlight
        border.width: 2
    }

    // If we use a MouseArea in the overlay, it stops the Viewport having 
    // full control over the pointer (like setting the Cursor shape). It's
    // better to listen to mouse events coming from the Viewport itself.
    Connections {
        target: view
        function onMousePress(buttons) {
            if (polygon_init) {
                genPolygonPoint(view.mousePosition.x, view.mousePosition.y);
            }
        }
        function onMouseDoubleClick(buttons) {
            root.handleDoubleClick(view.mousePosition);
        }
    }

    /*MouseArea {
        anchors.fill: root
        propagateComposedEvents: true

        onPressed: {
            if (polygon_init) {
                genPolygonPoint(mouse.x, mouse.y);
            }
            else if (mouse.flags & Qt.MouseEventCreatedDoubleClick) {
                root.handleDoubleClick(mouse);
            }
            mouse.accepted = false;
        }
    }*/

    // Overlay shapes

    Repeater {
        id: repeater
        model: grading_tool_overlay_shapes

        onItemAdded: {
            activeShape = index;
        }

        onItemRemoved: {
            if (activeShape >= count) {
                activeShape = count - 1;
            }
        }

        delegate: Loader {
            property var modelIndex: index
            property var modelValue: value

            sourceComponent: {
                if (value.type === "quad")
                    quad;
                else if (value.type === "polygon")
                    polygon;
                else if (value.type === "ellipse") {
                    ellipse;
                }
                else
                    console.log("Unknown shape type: " + value.type);
            }

            // Update logic
            function updateModelValue(v) {
                value = v;
            }

            function updateAttr(name, val) {
                var v = value;
                v[name] = val;
                value = v;
            }

            // Events
            function handleDoubleClick(mouse) {
                if (item.handleDoubleClick) {
                    item.handleDoubleClick(mouse);
                }
            }

        }
    }

    Component {
        id: quad

        MQuad {
            canvas: root
            viewScale: viewportScale

            transform: [
                Scale { xScale: viewportScale; yScale: viewportScale },
                Translate { x: viewportOffset.x; y: viewportOffset.y }
            ]

            // Selection logic
            function isSelected() {
                return root.activeShape === modelIndex;
            }

            onInteractingChanged: {
                if (interacting) {
                    root.activeShape = modelIndex;
                }
            }

            // Attribute update logic
            // Backend in OpenGL normalized device coordinate (bottom left -1,-1)
            // QML in normalised image coordinate (bottom left -1, 1/imageAspectRatio)

            property var backendValue: modelValue
            onBackendValueChanged: {
                if (!interacting && backendValue) {
                    shapePath.bl = Qt.point(backendValue.bl[2], -1.0 * (backendValue.bl[3] / imageAspectRatio));
                    shapePath.tl = Qt.point(backendValue.tl[2], -1.0 * (backendValue.tl[3] / imageAspectRatio));
                    shapePath.tr = Qt.point(backendValue.tr[2], -1.0 * (backendValue.tr[3] / imageAspectRatio));
                    shapePath.br = Qt.point(backendValue.br[2], -1.0 * (backendValue.br[3] / imageAspectRatio));
                }
            }

            function updateBackend(force) {
                if (force || interacting) {
                    var v = modelValue;
                    v.bl[2] = shapePath.bl.x;
                    v.bl[3] = -1.0 * (shapePath.bl.y * imageAspectRatio);
                    v.tl[2] = shapePath.tl.x;
                    v.tl[3] = -1.0 * (shapePath.tl.y * imageAspectRatio);
                    v.tr[2] = shapePath.tr.x;
                    v.tr[3] = -1.0 * (shapePath.tr.y * imageAspectRatio);
                    v.br[2] = shapePath.br.x;
                    v.br[3] = -1.0 * (shapePath.br.y * imageAspectRatio);
                    updateModelValue(v);
                }
            }
        }
    }

    Component {
        id: polygon

        MPolygon {
            canvas: root
            viewScale: viewportScale

            transform: [
                Scale { xScale: viewportScale; yScale: viewportScale },
                Translate { x: viewportOffset.x; y: viewportOffset.y }
            ]

            // Selection logic
            function isSelected() {
                return root.activeShape === modelIndex;
            }

            onInteractingChanged: {
                if (interacting) {
                    root.activeShape = modelIndex;
                }
            }

            // Attribute update logic
            // Backend in OpenGL normalized device coordinate (bottom left -1,-1)
            // QML in normalised image coordinate (bottom left -1, 1/imageAspectRatio)

            property var backendValue: modelValue
            onBackendValueChanged: {
                if (!interacting && backendValue) {
                    var newPoints = []
                    for (var i = 0; i < backendValue.count; ++i) {
                        newPoints.push(Qt.point(
                            backendValue.points[i][2],
                            -1.0 * backendValue.points[i][3] / imageAspectRatio
                        ));
                    }
                    shapePath.points = newPoints;
                    shapePath.refresh();
                }
            }

            function updateBackend(force) {
                if (force || interacting) {
                    var v = modelValue;
                    v.points = []
                    for (var i = 0; i < shapePath.points.length; ++i) {
                        v.points.push([
                            "vec2",
                            "1",
                            shapePath.points[i].x,
                            -1.0 * shapePath.points[i].y * imageAspectRatio
                        ]);
                    }
                    v.count = shapePath.points.length;
                    updateModelValue(v);
                }
            }

            // Events
            function handleDoubleClick(mouse) {
                if (!hovering) {
                    addPoint(mapFromItem(canvas, Qt.point(mouse.x, mouse.y)));
                }
            }
        }
    }

    Component {
        id: ellipse

        MEllipse {
            canvas: root
            viewScale: viewportScale

            transform: [
                Scale { xScale: viewportScale; yScale: viewportScale },
                Translate { x: viewportOffset.x; y: viewportOffset.y }
            ]

            // Selection logic
            function isSelected() {
                return activeShape === modelIndex;
            }

            onInteractingChanged: {
                if (interacting) {
                    activeShape = modelIndex;
                }
            }

            // Attribute update logic
            // Backend in OpenGL normalized device coordinate (bottom left -1,-1)
            // QML in normalised image coordinate (bottom left -1, 1/imageAspectRatio)

            property var backendValue: modelValue
            onBackendValueChanged: {
                if (!interacting && backendValue) {
                    shapePath.center = Qt.point(
                        backendValue.center[2] * drawScale,
                        -1.0 * (backendValue.center[3] * drawScale / imageAspectRatio));
                    shapePath.radius = Qt.point(
                        backendValue.radius[2] * drawScale,
                        backendValue.radius[3] * drawScale / imageAspectRatio);
                    shapePath.angle  = backendValue.angle;
                }
            }

            function updateBackend(force) {
                if (force || interacting) {
                    var v = modelValue;
                    v.center[2] = shapePath.center.x / drawScale;
                    v.center[3] = -1.0 * (shapePath.center.y / drawScale * imageAspectRatio);
                    v.radius[2] = shapePath.radius.x / drawScale;
                    v.radius[3] = shapePath.radius.y / drawScale * imageAspectRatio;
                    v.angle     = shapePath.angle;
                    updateModelValue(v);
                }
            }
        }
    }

    // Polygon creation handling

    property Component polygon_shape_component: Component {
        Shape {
            property var points: []

            id: shape

            ShapePath {
                id: sp
                strokeColor: "gray"
                strokeWidth: 2
                fillColor: "transparent"

                PathPolyline {
                    id: ppl
                    path: shape.points
                }
            }
        }
    }

    property Component polygon_point_component: Component {
        Rectangle {
            property bool starting_point: false
            property var point_x
            property var point_y

            width: 10
            height: 10
            radius: 5
            x: point_x - width / 2
            y: point_y - height / 2

            id: rr
            color: ma.containsMouse ? "red" : starting_point ? "lime" : "yellow"

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true

                onPressed: {
                    if (rr.starting_point && polygon_points.length >= 3) {
                        finalizePolygon();
                    }
                }
            }
        }
    }

    function genPolygonShape(x, y) {
        polygon_shape = polygon_shape_component.createObject(root, {});
    }

    function genPolygonPoint(x, y) {

        // first poly point must be inside the active image. The reason is that
        // in grid mode it would be possible for user to create a polygon
        // way outside the active image by mistake, as they try and draw a poly
        // on the wrong image altogether
        var tx = (x - viewportOffset.x)/viewportScale;
        var ty = (y - viewportOffset.y)/viewportScale;
        if (!polygon_points.length && (tx < -1.0 || tx > 1.0 || ty < -1.0/imageAspectRatio || ty > 1.0/imageAspectRatio)) return;

        var pc = polygon_point_component.createObject(root, {
            point_x: x,
            point_y: y,
            starting_point: polygon_points.length == 0
        });
        polygon_points.push(pc);

        var all_points = polygon_shape.points;
        all_points.push(Qt.point(x, y));
        polygon_shape.points = all_points;
    }

    function startPolygon() {
        activeShape = -1;
        polygon_points = []
        genPolygonShape();
    }

    function finalizePolygon() {
        var str_action = "Add Polygon ";

        // Construct action string with list of point coordinates
        for (var i = 0; i < polygon_points.length; i++) {
            var point = Qt.point(
                (polygon_points[i].point_x - viewportOffset.x) / viewportScale,
                (polygon_points[i].point_y - viewportOffset.y) / viewportScale * -imageAspectRatio
            );

            str_action += point.x + "," + point.y + ",";
        }

        attrs.drawing_action = str_action;

        cleanupPolygon();
    }

    function cleanupPolygon() {
        // Cleanup the temporary points
        for (var i = 0; i < polygon_points.length; i++) {
            polygon_points[i].destroy();
        }
        polygon_points = [];

        // Cleanup the temporary shape
        if (polygon_shape)
            polygon_shape.destroy();
        polygon_shape = null;

        attrs.polygon_init = false;
    }

}