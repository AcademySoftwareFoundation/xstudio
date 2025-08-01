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
    property bool viewing_alpha_channel: false

    onPointerEntered: {
        focus = true;
        forceActiveFocus()
    }

    XsButtonDialog {
        id: snapshotResultDialog
        // parent: sessionWidget
        width: text.width + 20
        title: "Snapshot export fail"
        text: {
            return "The snapshot could not be exported. Please check the parameters"
        }
        buttonModel: ["Ok"]
        onSelected: {
            snapshotResultDialog.close()
        }
    }

    onSnapshotRequestResult: {
        if (resultMessage != "") {
            snapshotResultDialog.title = "Snapshot export failed"
            snapshotResultDialog.text = resultMessage
            snapshotResultDialog.open()
        }
    }

    XsOutOfRangeOverlay {
        visible: viewport.frameOutOfRange
        anchors.fill: parent
        imageBox: imageBoundaryInViewport
    }

    XsErrorFrameOverlay {
        anchors.fill: parent
        imageBox: imageBoundaryInViewport
    }

    XsErrorFrameOverlay {
        visible: viewport.noAlphaChannel && viewing_alpha_channel
        frame_error_message: "No alpha channel available"
        anchors.fill: parent
        imageBox: imageBoundaryInViewport
    }

    XsModuleAttributes {
        id: colour_settings
        attributesGroupNames: "colour_pipe_attributes"
        onValueChanged: {
            if(key == "channel") {
                viewport.viewing_alpha_channel = value == "Alpha"
            }
        }
    }

    XsViewportBlankCard {

        id: blank_viewport_card
        anchors.fill: parent
        visible: false//playhead.mediaUuid == "{00000000-0000-0000-0000-000000000000}"
    }

    onQuickViewBackendRequest: {
        app_window.launchQuickViewer(mediaActors, compareMode)
    }

    onQuickViewBackendRequestWithSize: {
        app_window.launchQuickViewerWithSize(mediaActors, compareMode, position, size)
    }

    DropArea {
        anchors.fill: parent
        keys: [
            "text/uri-list",
            "xstudio/media-ids",
            "application/x-dneg-ivy-entities-v1"
        ]

        onDropped: {
           if(drop.hasUrls) {
                for(var i=0; i < drop.urls.length; i++) {
                    if(drop.urls[i].toLowerCase().endsWith('.xst') || drop.urls[i].toLowerCase().endsWith('.xsz')) {
                        Future.promise(studio.loadSessionRequestFuture(drop.urls[i])).then(function(result){})
                        app_window.sessionFunction.newRecentPath(drop.urls[i])
                        return;
                    }
                }
            }

            // prepare drop data
            let data = {}
            for(let i=0; i< drop.keys.length; i++){
                data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
            }

            if(app_window.currentSource.index && app_window.currentSource.index.valid) {
                Future.promise(app_window.sessionModel.handleDropFuture(drop.proposedAction, data, app_window.currentSource.index)).then(
                    function(quuids){
                        // if(viewport.playhead)
                        //     viewport.playhead.jumpToSource(quuids[0])
                        // session.selectedSource.selectionFilter.newSelection([quuids[0]])
                    }
                )
            } else {
                // no playlist etc.
                Future.promise(
                    app_window.sessionModel.handleDropFuture(drop.proposedAction, data)
                ).then(function(quuids){})
            }
        }
    }

    XsViewerContextMenu {
        id: viewerContextMenu
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
        attributesGroupNames: "viewport_zoom_and_pan_modes"

        onValueChanged: {
            if(zoom_and_pane_attrs.zoom_z) viewport.setOverrideCursor("://cursors/magnifier_cursor.svg", true)
            else if(zoom_and_pane_attrs.pan_x) viewport.setOverrideCursor(Qt.OpenHandCursor)
            else viewport.setOverrideCursor("", false);
        }

    }

    onMouseButtonsChanged: {
        if (mouseButtons == Qt.RightButton) {
            viewerContextMenu.x = mouse.x
            viewerContextMenu.y = mouse.y
            viewerContextMenu.visible = true
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

    Item {
        id: hud
        anchors.fill: parent

        XsModuleAttributesModel {
            id: viewport_overlays
            attributesGroupNames: "viewport_overlay_plugins"
        }

        XsModuleAttributesModel {
            id: hud_elements_bottom_left
            attributesGroupNames: "hud_elements_bottom_left"
        }

        XsModuleAttributesModel {
            id: hud_elements_bottom_center
            attributesGroupNames: "hud_elements_bottom_center"
        }

        XsModuleAttributesModel {
            id: hud_elements_bottom_right
            attributesGroupNames: "hud_elements_bottom_right"
        }

        XsModuleAttributesModel {
            id: hud_elements_top_left
            attributesGroupNames: "hud_elements_top_left"
        }

        XsModuleAttributesModel {
            id: hud_elements_top_center
            attributesGroupNames: "hud_elements_top_center"
        }

        XsModuleAttributesModel {
            id: hud_elements_top_right
            attributesGroupNames: "hud_elements_top_right"
        }

        XsModuleAttributes {
            id: hud_toggle
            attributesGroupNames: "hud_toggle"
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

}
