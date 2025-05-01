// SPDX-License-Identifier: Apache-2.0

import QtQuick
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

    // Note: Viewport has property imageBoundariesInViewport - this is a vector
    // of QRect desribing the coordinates of the image(s) in the viewport.
    // We use this, with the keySubplayheadIndex of the playhead, to work out
    // the geometry of the image being modified in the viewport.
    // If multiple images are visibe (e.g. contact sheet/grid layout) then we
    // are modifying the image in the vector with an index that is the
    // keySubplayheadIndex. If only one image is visible, this image is coming
    // from keySubplayheadIndex so we use index zero.
    property var keyPlayheadIdx: viewportPlayhead.keySubplayheadIndex
    property var imBoundaries: view.imageBoundariesInViewport ? view.imageBoundariesInViewport : []
    property var imResolutions: view.imageResolutions

    property var viewportName: view.name
    property var imageBox: keyPlayheadIdx < imBoundaries.length ? imBoundaries[keyPlayheadIdx] : imBoundaries.length ? imBoundaries[0] : Qt.rect(0,0,0,0)
    property var imageResolution: keyPlayheadIdx < imResolutions.length ? imResolutions[keyPlayheadIdx] : imResolutions.length ? imResolutions[0] : Qt.size(0, 0)
    property real imageAspectRatio: imageBox.width/imageBox.height
    property real viewportScale: 1.0
    property var viewportOffset: Qt.point(0, 0)

    property var tool_opened_count: grd_attrs.tool_opened_count
    property var mask_shapes_visible: attrs.mask_shapes_visible
    property var mask_selected_shape: attrs.mask_selected_shape
    property var drawing_action: attrs.drawing_action

    visible: mask_shapes_visible && tool_opened_count > 0 && !isQuickview

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

    function handleDoubleClick(mouse) {
        if (activeShape >= 0 && activeShape < repeater.count) {
            repeater.itemAt(activeShape).handleDoubleClick(mouse);
        }
    }

    function set_interaction_shape(modelIndex) {
        activeShape = modelIndex
    }
    // Event handling

    XsHotkey {
        sequence: "Escape"
        name: "unselect"
        context: "any"
        onActivated: {
            construction_polygon.item.cleanupPolygon()
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

        target: view // the viewport

        function onMouseRelease(buttons) {
            
            if (!attrs.polygon_init) attrs.interacting = false
            construction_polygon.item.mouseReleased(buttons)
            for (var i = 0; i < repeater.count; ++i) {
                repeater.itemAt(i).item.mouseReleased(buttons)
            }

        }

        function onMousePress(position, buttons, modifiers) {

            if (construction_polygon.item.mousePressed(position, buttons, modifiers)) return

            for (var i = 0; i < repeater.count; ++i) {
                if (repeater.itemAt(i).item.mousePressed(position, buttons, modifiers)) {
                    return
                }
            }

        }

        function onMouseDoubleClick(position, buttons, modifiers) {
            if (activeShape >= 0) {
                repeater.itemAt(activeShape).item.mouseDoubleClicked(position, buttons, modifiers)
            }
        }

        function onMousePositionChanged(position, buttons, modifiers) {

            construction_polygon.item.mouseMoved(position, buttons, modifiers)
            for (var i = 0; i < repeater.count; ++i) {
                repeater.itemAt(i).item.mouseMoved(position, buttons, modifiers)
            }

        }
    }

    // Overlay shapes

    Repeater {
        id: repeater
        model: grading_tool_overlay_shapes

        onItemAdded: (index) =>  {
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
                if (value.type === "polygon")
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
        id: polygon

        MPolygon {
            canvas: root
            viewScale: viewportScale

            transform: [
                Scale { xScale: viewportScale; yScale: viewportScale },
                Translate { x: viewportOffset.x; y: viewportOffset.y }
            ]

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

        }
    }

    Component {

        id: cp
        MPolygon {

            canvas: root
            viewScale: viewportScale
            under_construction: attrs.polygon_init
            mv: parent.modelValue

            transform: [
                Scale { xScale: viewportScale; yScale: viewportScale },
                Translate { x: viewportOffset.x; y: viewportOffset.y }
            ]

            onUnder_constructionChanged: {
                if (under_construction) {
                    startPolygon()
                } else {
                    finalizePolygon()
                }
            }

            function startPolygon() {
                activeShape = -2;
                updateModelValue({"points": []})
                attrs.interacting = true
            }
        
            function finalizePolygon() {
        
                if (modelValue.points.length >= 3) {
                    var str_action = "Add Polygon ";
                    var new_pts = modelValue.points
            
                    // Construct action string with list of point coordinates
                    for (var i = 0; i < new_pts.length; i++) {
            
                        str_action += new_pts[i][2] + "," + new_pts[i][3] + ",";
                    }
                    attrs.drawing_action = str_action;
                }
                cleanupPolygon();
        
            }
        
            function cleanupPolygon() {
                updateModelValue({"points": []})
                attrs.polygon_init = false;
                activeShape = -2;
                attrs.interacting = false

            }
        
        }
    }
    
    Loader {

        id: construction_polygon

        property var modelIndex: -2
        property var modelValue: {"points": []}

        sourceComponent: cp

        // Update logic
        function updateModelValue(v) {
            modelValue = v;
        }

    }
}