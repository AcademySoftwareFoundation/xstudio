// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQml 2.14

import xstudio.qml.viewport 1.0
import QuickFuture 1.0
import QuickPromise 1.0
import QtGraphicalEffects 1.12
import QtGraphicalEffects 1.12

import xStudio 1.0

import xstudio.qml.properties 1.0
import xstudio.qml.module 1.0

Viewport {

    id: viewport
    objectName: "viewport"
    property bool is_popout_viewport: false
    property bool is_empty: playhead == undefined

    XsOutOfRangeOverlay {
        visible: viewport.frameOutOfRange
        anchors.fill: parent
        imageBox: imageBoundaryInViewport
    }

    XsErrorFrameOverlay {
        anchors.fill: parent
        imageBox: imageBoundaryInViewport
    }

    XsViewportBlankCard {

        id: blank_viewport_card
        anchors.fill: parent
        visible: viewport.is_empty
    }

    DropArea {
        anchors.fill: parent

        onEntered: {
            if(drag.hasUrls || drag.hasText) {
                drag.acceptProposedAction()
            }
        }
        onDropped: {
           if(drop.hasUrls) {
                for(var i=0; i < drop.urls.length; i++) {
                    if(drop.urls[i].toLowerCase().endsWith('.xst')) {
                        session.loadUrl(drop.urls[i])
                        app_window.session.new_recent_path(drop.urls[i])
                        return
                    }
                }
            }

            // prepare drop data
            let data = {}
            for(let i=0; i< drop.keys.length; i++){
                data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
            }

            if(session.selectedSource) {
                Future.promise(session.selectedSource.handleDropFuture(data)).then(
                    function(quuids){
                        playhead.jumpToSource(quuids[0])
                        selectionFilter.newSelection([quuids[0]])
                    }
                )
            } else {
                Future.promise(session.handleDropFuture(data)).then(function(quuids){})
            }
        }
    }

    XsViewerContextMenu {
        id: viewerContextMenu
        is_popout_viewport: viewport.is_popout_viewport
    }

    XsPreferenceSet {
        id: fit_mode_pref
        preferencePath: "/ui/qml/fit_mode"
    }

    XsPreferenceSet {
        id: popout_fit_mode_pref
        preferencePath: "/ui/qml/second_window_fit_mode"
    }

    XsModuleAttributes {
        id: zoom_and_pane_attrs
        attributesGroupName: "viewport_zoom_and_pan_modes"

        onValueChanged: {            
            if(zoom_and_pane_attrs.zoom) viewport.setOverrideCursor( "://cursors/magnifier_cursor.svg", true)
            else if(zoom_and_pane_attrs.pan) viewport.setOverrideCursor(Qt.OpenHandCursor)
            else viewport.setOverrideCursor("", false);
        }

    }
    XsModuleAttributes {
        id: viewport_attrs
        attributesGroupName: "viewport_attributes"

        onAttrAdded: {
            if (attr_name == "fit") fit = fit_mode_pref.properties.value
        }

        onValueChanged: {
            if (key == "fit" && fit != "Off") fit_mode_pref.properties.value = value
        }
    }

    XsModuleAttributes {
        id: popout_viewport_attrs
        attributesGroupName: "popout_viewport_attributes"

        onAttrAdded: {
            if (attr_name == "fit") fit = popout_fit_mode_pref.properties.value
        }

        onValueChanged: {
            if (key == "fit" && fit != "Off") popout_fit_mode_pref.properties.value = value
        }
    }

    function resetViewport() {

        if (viewport_attrs.fit) viewport_attrs.fit = fit_mode_pref.properties.value
        if (popout_viewport_attrs.fit) popout_viewport_attrs.fit = popout_fit_mode_pref.properties.value
        //fitMode = fit_mode_pref.properties.value
        /*if (colourPipeline.exposure != 0.0) {
            colourPipeline.previousSetExposure = colourPipeline.exposure
            colourPipeline.exposure = 0.0

        }*/
        playhead.velocity = 1.0
    }

    onMouseButtonsChanged: {
        if (mouseButtons == Qt.RightButton) {
            viewerContextMenu.x = mouse.x
            viewerContextMenu.y = mouse.y
            viewerContextMenu.visible = true
        }
    }

    XsModuleAttributesModel {
        id: viewport_overlays
        attributesGroupName: "viewport_overlay_plugins"
    }

    Repeater {

        id: viewport_overlay_plugins
        anchors.fill: parent
        model: viewport_overlays

        delegate: Item {

            id: parent_item

            property var dynamic_widget

            property var type_: type ? type : null

            onType_Changed: {
                if (type == "QmlCode") {
                    dynamic_widget = Qt.createQmlObject(qml_code, parent_item)
                }
            }
        }
    }
}
