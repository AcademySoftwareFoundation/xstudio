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

import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0


Viewport {

    id: viewport
    objectName: "viewport"
    property bool is_popout_viewport: false

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
        visible: playhead.media.mediaSource == undefined
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

    XsModelProperty {
        id: fit_mode_pref
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/fit_mode", "pathRole")
    }

    XsModelProperty {
        id: popout_fit_mode_pref
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/second_window_fit_mode", "pathRole")
    }

    XsModuleAttributes {
        id: zoom_and_pane_attrs
        attributesGroupName: "viewport_zoom_and_pan_modes"

        onValueChanged: {
            if(zoom_and_pane_attrs.zoom_z) viewport.setOverrideCursor("://cursors/magnifier_cursor.svg", true)
            else if(zoom_and_pane_attrs.pan_x) viewport.setOverrideCursor(Qt.OpenHandCursor)
            else viewport.setOverrideCursor("", false);
        }

    }
    XsModuleAttributes {
        id: viewport_attrs
        attributesGroupName: "viewport_attributes"

        onAttrAdded: {
            if (attr_name == "fit") fit = fit_mode_pref.value
        }

        onValueChanged: {
            if (key == "fit" && fit != "Off") fit_mode_pref.value = value
        }
    }

    XsModuleAttributes {
        id: popout_viewport_attrs
        attributesGroupName: "popout_viewport_attributes"

        onAttrAdded: {
            if (attr_name == "fit") fit = popout_fit_mode_pref.value
        }

        onValueChanged: {
            if (key == "fit" && fit != "Off") popout_fit_mode_pref.value = value
        }
    }

    function resetViewport() {

        if (viewport_attrs.fit) viewport_attrs.fit = fit_mode_pref.value
        if (popout_viewport_attrs.fit) popout_viewport_attrs.fit = popout_fit_mode_pref.value
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

    Item {
        id: hud
        anchors.fill: parent

        XsModuleAttributesModel {
            id: viewport_overlays
            attributesGroupName: "viewport_overlay_plugins"
        }

        XsModuleAttributesModel {
            id: hud_elements_bottom_left
            attributesGroupName: "hud_elements_bottom_left"
        }

        XsModuleAttributesModel {
            id: hud_elements_bottom_center
            attributesGroupName: "hud_elements_bottom_center"
        }

        XsModuleAttributesModel {
            id: hud_elements_bottom_right
            attributesGroupName: "hud_elements_bottom_right"
        }

        XsModuleAttributesModel {
            id: hud_elements_top_left
            attributesGroupName: "hud_elements_top_left"
        }

        XsModuleAttributesModel {
            id: hud_elements_top_center
            attributesGroupName: "hud_elements_top_center"
        }

        XsModuleAttributesModel {
            id: hud_elements_top_right
            attributesGroupName: "hud_elements_top_right"
        }

        XsModuleAttributes {
            id: hud_toggle
            attributesGroupName: "hud_toggle"
        }

        visible: hud_toggle.hud ? hud_toggle.hud : false

        property var hud_margin: 10

        Column {

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: hud.hud_margin
            Repeater {

                model: hud_elements_bottom_left

                delegate: Item {

                    id: parent_item
                    width: dynamic_widget.width
                    height: dynamic_widget.height

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

        Column {

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: hud.hud_margin
            Repeater {

                model: hud_elements_bottom_center

                delegate: Item {

                    id: parent_item
                    width: dynamic_widget.width
                    height: dynamic_widget.height

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

        Column {

            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: hud.hud_margin
            Repeater {

                model: hud_elements_bottom_right

                delegate: Item {

                    id: parent_item
                    width: dynamic_widget.width
                    height: dynamic_widget.height

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
            
        Column {

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: hud.hud_margin
            Repeater {

                model: hud_elements_top_left

                delegate: Item {

                    id: parent_item
                    width: dynamic_widget.width
                    height: dynamic_widget.height

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

        Column {

            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: hud.hud_margin
            Repeater {

                model: hud_elements_top_center

                delegate: Item {

                    id: parent_item
                    width: dynamic_widget.width
                    height: dynamic_widget.height

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

        Column {

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: hud.hud_margin
            Repeater {

                model: hud_elements_top_right

                delegate: Item {

                    id: parent_item
                    width: dynamic_widget.width
                    height: dynamic_widget.height

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
    }

    Repeater {

        id: viewport_overlay_plugins
        anchors.fill: parent
        model: viewport_overlays

        delegate: Item {

            id: parent_item
            anchors.fill: parent

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
