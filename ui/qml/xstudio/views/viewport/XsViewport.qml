import QtQuick 2.12
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "./widgets"
import "./hud"

// This Item extends the pure 'Viewport' QQuickItem from the cpp side

Viewport {

    id: view

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

    property alias hide_ui_hotkey: hide_ui_hotkey

    onPointerEntered: {
        //appWindow.setFocusViewport(view)
    }

    onVisibleChanged: {
        if (visible) {
            //appWindow.setFocusViewport(view)
        }
    }

    // this one lays out the HUD graphics coming from HUD plugins and
    // also general overlay graphics like Mask
    XsViewportHUD {}

    XsViewportOverlays {}

    XsViewportFrameErrorIndicator {}

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

    onMousePress: {
        if (buttons == Qt.RightButton) {
            showContextMenu(mousePosition.x, mousePosition.y)
        }
    }

    function showContextMenu(x, y) {
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

    XsAttributeValue {
        id: __frame_error
        attributeTitle: "frame_error"
        model: viewport_attrs
    }
    property alias frame_error_message: __frame_error.value

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
        if (custom_cursor in cursor_name_dict) {
            setOverrideCursor(cursor_name_dict[custom_cursor])
        } else {
            setOverrideCursor(custom_cursor, true)
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
        property var ib: view.imageBoundariesInViewport[viewportPlayhead.keySubplayheadIndex]
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

}
