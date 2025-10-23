import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "./widgets"
import "./hud"

// This Item extends the pure 'Viewport' QQuickItem from the cpp side

Viewport {

    id: view

    property bool has_context_menu: true

    /*XsPressedKeysMonitor {
        id: monitor
    }

    property var fonk

    XsText {
        id: keyIndicator
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 40
        font.pixelSize: 50
        Behavior on opacity { NumberAnimation { easing.type: Easing.InQuart; duration: 1000; from: 1; to: 0; onRunningChanged: if(!running) keyIndicator.opacity = 0 } }

        property var fonk: monitor.pressedKeys

        onFonkChanged: {
            opacity = 1
            opacity = 0
            if (fonk != "") text = "Hotkey Pressed : " + fonk
        }

    }*/

    /**************************************************************

    HOTKEYS

    ****************************************************************/
    XsHotkey {
        id: hide_ui_hotkey
        sequence: "Ctrl+H"
        name: "Hide UI"
        description: "Hides/reveals the toolbars and info bars in the viewer"
        context: view.name
        onActivated: {
            elementsVisible = !elementsVisible
        }
    }

    XsHotkey {
        id: set_media_rate_hotkey
        sequence: "Alt+F"
        name: "Set On-Screen Media FPS"
        description: "Shows a pop-up that allows you to enter a new FPS rate for the on-screen media."
        context: view.name
        onActivated: {
            show_fps_widget()
        }
    }


    XsHotkey {
        id: reload_selected_media_hotkey
        sequence: "U"
        name: "Reload Selected Media"
        description: "Reload selected media items."
        context: view.name
        onActivated: {
            let mediaUuid = viewportPlayhead.mediaUuid
            let mindex = theSessionData.searchRecursive(mediaUuid, "actorUuidRole", theSessionData.index(0, 0))
            if (mindex.valid) {
                theSessionData.rescanMedia([mindex])
            }
        }
        componentName: "Media List"
    }

    property alias hide_ui_hotkey: hide_ui_hotkey

    onPointerEntered: {
        //appWindow.setFocusViewport(view)
    }

    onVisibleChanged: {
        if (visible) {
            //appWindow.setFocusViewport(view)
        }
    }

    XsViewportOverlays {}

    // the property imageBoundariesInViewport updates every time the viewport
    // geometry changes (pan/zoom or image size). We don't want the repeater 
    // below to rebuild the XsViewportHUD items on every update to 
    // imageBoundariesInViewport, so by maintaining a ListModel the Repeater
    // will not full rebuild but just update the model data values that
    // effect the XsViewportHUD instances (the image boundaries per image on
    // screen)
    ListModel{
        id: image_boundaries
    }

    onImageBoundariesInViewportChanged: {
        var n = imageBoundariesInViewport.length
        for (var i = 0; i < n; ++i) {
            if (i < image_boundaries.count) {
                image_boundaries.get(i).imageBoundary = imageBoundariesInViewport[i];
                image_boundaries.get(i).multiHUD = n > 1;
            } else {
                image_boundaries.append({"imageBoundary": imageBoundariesInViewport[i], "multiHUD": n > 1})
            }
        }
        if (image_boundaries.count > n) {
            image_boundaries.remove(n, image_boundaries.count - n)
        }
    }

    // this property is only set in offscreen viewports, but it is referenced
    // in the ViewportHUD qml code so we create a dummy property here 
    property var hud_plugins_display_data

    // this one lays out the HUD graphics coming from HUD plugins and
    // also general overlay graphics like Mask
    Repeater {
        model: image_boundaries
        XsViewportHUD {
            imageIndex: index
        }
    }
    
    Loader {
        id: menu_loader
    }

    // This menu is built from a menu model that is maintained by xSTUDIO's
    // backend. We access the menu model by an id string 'menuModelName' that
    // will be set by the derived type
    Component {
        id: menuComponent
        XsViewerContextMenu {
            viewport: view
        }
    }

    onMousePressScreenPixels: (position, buttons) => {
        if (buttons == Qt.RightButton) {
            showContextMenu(position.x, position.y)
        }
    }

    function showContextMenu(x, y) {
        
        if (!has_context_menu) return;

        if (menu_loader.item == undefined) {
            menu_loader.sourceComponent = menuComponent
        }
        menu_loader.item.showMenu(
            view,
            x,
            y);
    }

    // Cursor switching for pan/zoom modes

    XsModuleData {
        id: viewport_attrs
        modelDataName: view.name + "_attrs"
    }

    XsAttributeValue {
        id: __frame_rate_expr
        attributeTitle: "Frame Rate"
        model: viewport_attrs
    }
    property alias frame_rate_expr: __frame_rate_expr.value

    XsAttributeValue {
        id: __custom_cursor
        attributeTitle: "Custom Cursor Name"
        model: viewport_attrs
    }
    property alias custom_cursor: __custom_cursor.value

    property var cursor_name_dict: {
        "Qt.ArrowCursor" : Qt.ArrowCursor,
        "Qt.UpArrowCursor" : Qt.UpArrowCursor,
        "Qt.CrossCursor" : Qt.CrossCursor,
        "Qt.WaitCursor" : Qt.WaitCursor,
        "Qt.IBeamCursor" : Qt.IBeamCursor,
        "Qt.SizeVerCursor" : Qt.SizeVerCursor,
        "Qt.SizeHorCursor" : Qt.SizeHorCursor,
        "Qt.SizeBDiagCursor" : Qt.SizeBDiagCursor,
        "Qt.SizeFDiagCursor" : Qt.SizeFDiagCursor,
        "Qt.SizeAllCursor" : Qt.SizeAllCursor,
        "Qt.BlankCursor" : Qt.BlankCursor,
        "Qt.SplitVCursor" : Qt.SplitVCursor,
        "Qt.SplitHCursor" : Qt.SplitHCursor,
        "Qt.PointingHandCursor" : Qt.PointingHandCursor,
        "Qt.ForbiddenCursor" : Qt.ForbiddenCursor,
        "Qt.OpenHandCursor" : Qt.OpenHandCursor,
        "Qt.ClosedHandCursor" : Qt.ClosedHandCursor,
        "Qt.WhatsThisCursor" : Qt.WhatsThisCursor,
        "Qt.BusyCursor" : Qt.BusyCursor,
        "Qt.DragMoveCursor" : Qt.DragMoveCursor,
        "Qt.DragCopyCursor" : Qt.DragCopyCursor,
        "Qt.DragLinkCursor" : Qt.DragLinkCursor
    }

    onCustom_cursorChanged: {
        if (typeof custom_cursor == "object") {
            if (custom_cursor.name in cursor_name_dict) {
                setOverrideCursor(cursor_name_dict[custom_cursor.name])
            } else {
                setOverrideCursor(custom_cursor.name, custom_cursor.size, custom_cursor.x_offset, custom_cursor.y_offset)
            }
        }
    }

    property var snapshotDialog
    function doSnapshot() {
        studio.setupSnapshotViewport(view.playheadActorAddress())
        if (snapshotDialog == undefined) {
            snapshotDialog = Qt.createQmlObject('import xStudio 1.0; XsSnapshotDialog { viewerWidth:view.width; viewerHeight: view.height}', appWindow)
        }
        snapshotDialog.visible = true
    }

    // Note: svg rendering not working for this image. Works in a browser so
    // suspect that QML SVG rendering has issues
    /*Image {

        id: inspect_icon
        source: "qrc:/images/xstudio_backdrop.svg"
        visible: !view.hasPlayhead
        anchors.fill: parent
        sourceSize.height: 960
        sourceSize.width: 540    
    }*/

    // This highlights the 'hero' image momentarily for  certain compare modes
    Rectangle {
        visible: opacity > 0
        x: imageBox.x
        y: imageBox.y
        width: imageBox.width
        height: imageBox.height
        color: "transparent"
        border.color: palette.highlight
        border.width: 2
        // Yuk! TODO: add a property for key image boundary in viewport
        property var ib: view.imageBoundariesInViewport.length > viewportPlayhead.keySubplayheadIndex ? view.imageBoundariesInViewport[viewportPlayhead.keySubplayheadIndex] : undefined
        property var imageBox: ib ? ib : Qt.rect(0,0,0,0)
        property var idx: viewportPlayhead.keySubplayheadIndex
        onIdxChanged: {
            changeAnimation.stop()
            changeAnimation.start()
        }
        NumberAnimation on opacity {
            id: changeAnimation
            from: 1
            to: 0
            duration: 2000
        }
    }

    function show_fps_widget() {
        loader.sourceComponent = fps_popup
        loader.item.x = view.width/2-loader.item.width/2
        loader.item.y = view.height/2-loader.item.height/2
        loader.item.visible = true
    }

    Loader {
        id: loader
    }

    Component {
        id: fps_popup
        XsViewerSetMediaFPSPopup {}
    }

}
