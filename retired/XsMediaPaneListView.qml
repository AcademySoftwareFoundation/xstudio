// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

Flickable {

    id: media_list
    contentHeight: mediaList.length*item_height
    flickableDirection: Flickable.VerticalFlick
    //clip: true

    property var source: session.selectedSource
    // property var source: session.selectedSource ? session.selectedSource : session.onScreenSource
    property var mediaList: source ? source.mediaList ? source.mediaList : [] : []
    property var mediaModel: source ? source.mediaModel ? source.mediaModel : [] : []
    property var header
    property var dragDropAnimationDuration: 100
    property var item_height: header.item_height
    property var selection_uuids: selectionFilter ? selectionFilter.selectedMediaUuids : []
    property var previousCompareMode: -1
    property var playhead: sessionWidget.viewport.playhead
    property var selectionFilter: source ? source.selectionFilter : undefined
    property var firstMediaItemInSelection

    property var currentOnScreenMediaUuid: playhead.media ? playhead.media.uuid : undefined
    property alias mediaMenu: media_menu

    onItem_heightChanged: {
        for (var idx = 0; idx < mediaList.length; idx++) {
            if(repeater.itemAt(idx))
                repeater.itemAt(idx).y = item_height * idx
        }
        autoscroll(true, currentOnScreenMediaUuid)
    }

    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AlwaysOn
    }

    Rectangle {
        color: XsStyle.mainBackground
        width: media_list.width
        height: media_list.height
        x: 0
        y: contentY
    }

    // function jumpToPreviousSource() {
    //     console.log("jumpToPreviousSource")
    //     if (!playhead.jumpToPreviousSource()) {
    //         selectionFilter.moveSelectionByIndex(-1)
    //     }
    // }

    // function jumpToNextSource() {
    //     console.log("jumpToNextSource")

    //     if (!playhead.jumpToNextSource()) {
    //         selectionFilter.moveSelectionByIndex(1)
    //     }
    // }

    function selectSingleItem(item_idx) {
        if (selection_uuids.length == 1 && mediaList[item_idx].uuid == selection_uuids[0] ) {
            // The user has clicked on a single, already selected item. We do not allow
            // nothing to be selected so pass
            return
        }

        var the_item = repeater.itemAt(item_idx)
        if (the_item.selected) {
            playhead.jumpToSource(mediaList[item_idx].uuid)
            return
        }

        // user has clicked outside of any existing selection
        selectionFilter.newSelection([mediaList[item_idx].uuid])
    }

    function selectAll() {
        var uuids = []
        for (var idx = 0; idx < mediaList.length; idx++) {
            uuids.push(mediaList[idx].uuid)
        }
        selectionFilter.newSelection(uuids)
    }

    function deselectAll() {
        selectionFilter.newSelection([currentOnScreenMediaUuid])
    }

    function sortAlphabetically() {
        if (source) source.sortAlphabetically()
        if (playhead) {
            // this refreshes the whole list, or it can get in a muddle due
            // to previous drag-drop ordering not matching following the
            // sortAlphabetically
            var all_uuids = []
            for (var i = 0; i < mediaList.length; ++i) {
                all_uuids.push(mediaList[i].uuid)
            }
            source.dragDropReorder(
                all_uuids,
                ""
            )
            mouse_area.dragging_group = []
            mouse_area.not_dragging_group = []
            autoscroll(true, currentOnScreenMediaUuid)
        }
    }

    onSelection_uuidsChanged: {

        if (selection_uuids.length) {
            //playhead.jumpToSource(selection_uuids[selection_uuids.length-1])
        }
        set_first_in_selection()

    }

    function delete_selected() {
        selectionFilter.deleteSelected()
    }

    function gather_media_for_selected() {
        selectionFilter.gatherSourcesForSelected()
    }

    function evict_selected() {
        selectionFilter.evictSelected()
    }

    function appendSingleItem(item_idx) {

        var the_item = repeater.itemAt(item_idx)
        // add to the selection like this so we get a signal
        // that selection_uuids has changed. This signal is
        // essential so that the selection order indicator on
        // each item updates. Direct push does not trigger the
        // signal
        var uuids = selection_uuids
        uuids.push(mediaList[item_idx].uuid)
        selectionFilter.newSelection(uuids)

    }

    function indexFromUuid(uuid) {
        var item_index = -1
        for (var idx = 0; idx < mediaList.length; idx++) {
            if (repeater.itemAt(idx) != undefined && repeater.itemAt(idx).uuid == uuid) {
                item_index = idx
                break
            }
        }
        return item_index
    }

    function multiSelect(item_idx) {

        if (selection_uuids.length == 0) {
            selectSingleItem(item_idx)
            return
        }
        var uuids = selection_uuids
        var last_item_index = indexFromUuid(uuids[uuids.length-1])

        if (last_item_index > item_idx) {
            for (var idx = last_item_index-1; idx >= item_idx; idx--) {
                if (!repeater.itemAt(idx).selected) {
                    uuids.push(mediaList[idx].uuid)
                }
            }
        } else if (last_item_index != -1) {
            for (var idx = last_item_index+1; idx <= item_idx; idx++) {
                if (!repeater.itemAt(idx).selected) {
                    uuids.push(mediaList[idx].uuid)
                }
            }
        }
        selectionFilter.newSelection(uuids)

    }

    function deSelectItem(item_idx) {

        var the_item = repeater.itemAt(item_idx)
        var uuid = mediaList[item_idx].uuid
        var uuids = selection_uuids.filter(function(ele){
            return ele != uuid;
        });
        selectionFilter.newSelection(uuids)
    }

    Item {

        id: clayout
        anchors.fill: parent

        Repeater {

            id: repeater
            model: mediaModel

            XsMediaViewItemContainer {
                reapeaterGroup: repeater
                y: index*item_height
                currentIndex: index
                x: 0
                width: parent.width
                uuid: media_ui_object.uuid

                // switching visibility according to whether they are in the visible
                // portion of the scroll box makes the UI much faster when you have
                // a very long playlist
                visible: (y+height) > media_list.contentY && y < (media_list.contentY+media_list.height)

            }

        }
    }

    PropertyAnimation {
        id: scroll_animator
        target: media_list
        property: "contentY"
        running: false
        duration: XsStyle.mediaListMaxScrollSpeed

        function autoScroll(scroll_to) {
            running = false
            to = Math.min(Math.max(0, scroll_to), mediaList.length*item_height-Math.min(contentHeight,height))
            current_target = to
            running = true
        }

        property var current_target: undefined

        function mouseWheel(delta) {
            if (running) {
                running = false
                to = Math.min(Math.max(0, current_target + delta*item_height/120), mediaList.length*item_height-height)
                current_target = to
                running = true
            } else {
                current_target = Math.min(Math.max(0, contentY + delta*item_height/120), mediaList.length*item_height-height)
                to = current_target
                running = true
            }
        }

    }

    function autoscroll(force, target_uuid) {
        if (mediaList == undefined) return
        var matched = false
        for (var i = 0; i < mediaList.length; ++i) {
            var item = repeater.itemAt(i)
            if (item && item.uuid == target_uuid) {
                var curr_onscreen_y = i*item_height - contentY
                if (force || curr_onscreen_y < item_height*0.25 || curr_onscreen_y > (height-item_height*0.75)) {
                    scroll_animator.autoScroll((i+0.5)*item_height - height/2)
                }
                matched = true
                break
            }
        }
        if (!matched) scroll_animator.autoScroll(0)
    }

    onMediaListChanged: {

        for (var i = 0; i < mediaList.length; ++i) {
            var item = repeater.itemAt(i)
            if(item) {
                item.y = i*item_height
            }
        }
        set_first_in_selection()
        //autoscroll(true, currentOnScreenMediaUuid)
    }

    function set_first_in_selection() {

        var result = undefined
        if (selection_uuids.length) {
            var first_selected = indexFromUuid(selection_uuids[0])
            if (first_selected != -1) {
                result = mediaList[first_selected]
            } else if (mediaList.length) {
                result = mediaList[0]
            }
        } else if (mediaList.length) {
            result = mediaList[0]
        }
        firstMediaItemInSelection = result;

    }

    function switchSelectedToNamedStream(stream_name) {
        for (var i = 0; i < mediaList.length; ++i) {
            var item = repeater.itemAt(i)
            if (item.selected) {
                mediaList[i].mediaSource.switchToStream(stream_name)
            }
        }
    }

    onCurrentOnScreenMediaUuidChanged: {
        autoscroll(false, currentOnScreenMediaUuid)
    }

    onSourceChanged: {
        contentY = 0
        autoscroll(true, currentOnScreenMediaUuid)
    }

    XsMediaMenu {
        id: media_menu
    }

    // media dropped from desktop
    DropArea {
        anchors.fill: parent

        onEntered: {
            if(drag.hasUrls || drag.hasText) {
                drag.acceptProposedAction()
            }
        }
        onDropped: {
            var count = mediaList.length

           if(drop.hasUrls) {
                for(var i=0; i < drop.urls.length; i++) {
                    if(drop.urls[i].toLowerCase().endsWith('.xst')) {
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

            if(session.selectedSource) {
                Future.promise(source.handleDropFuture(data)).then(
                    function(quuids){
                        if(mediaList.length >= count+1) {
                            selectSingleItem(count)
                        }
                    }
                )
            } else {
                Future.promise(session.handleDropFuture(data)).then(function(quuids){})
            }
        }
    }

    // This mouse event handler implements the drag/drop re-ordering
    // in the media list and also selection behaviour
    MouseArea {

        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property var under_mouse
        property var last_selected_idx: undefined

        property bool doing_drag: false
        property var y_drag_start
        property var x_drag_start

        property var dragging_group: []
        property var not_dragging_group: []

        property var dragged_item_intial_pos_x
        property var dragged_item_intial_pos_y
        property var under_mouse_idx: 0

        property var drag_drop_uuids: []
        property var drag_drop_before_uuid
        property var freshly_selected: undefined
        property bool dragged_outside: doing_drag ? (mouseX < 0.0 || mouseY < 0.0 || mouseX > width || mouseY > height) : 0
        property var drag_outside_items: []

        onPressed: {

            if (mouse.button != Qt.LeftButton) return;

            y_drag_start = mouse.y
            x_drag_start = mouse.x
            doing_drag = false
            freshly_selected = undefined

            if (under_mouse) {

                // if item is already selected, deselect it if CTRL is held or do nothing
                if (under_mouse.selected && mouse.modifiers == Qt.ControlModifier) {
                    deSelectItem(under_mouse.currentIndex)
                } else {
                    if (mouse.modifiers == Qt.ControlModifier) {
                        appendSingleItem(under_mouse.currentIndex)
                    } else if (mouse.modifiers == Qt.ShiftModifier) {
                        multiSelect(under_mouse.currentIndex)
                    } else {
                        // if the user is clicking for a second time on the only selected item, don't
                        // do anytjhing incase they are doing a drag-drop playlist reorder.
                        if (!(selection_uuids.length == 1 && under_mouse.uuid == selection_uuids[0]))
                        {
                            selectSingleItem(under_mouse.currentIndex)
                            freshly_selected = under_mouse
                        }

                    }
                }

            }

        }

        Timer {

            id: dragDropFinishTimer
            interval: dragDropAnimationDuration
            running: false
            repeat: false

            onTriggered: {

                source.dragDropReorder(
                    mouse_area.drag_drop_uuids,
                    mouse_area.drag_drop_before_uuid
                )

            }
        }

        onReleased: {

            if (mouse.button == Qt.RightButton) {
                // mouse is relative to control, if we're inside a scrollable area
                // we need to translate..
                media_menu.x = mouse.x
                media_menu.y = mouse.y-contentY
                media_menu.visible = true
                return;
            }

            if (doing_drag && !dragged_outside) {

                // execute the drag/drop action to update the playlist ordering

                drag_drop_uuids = []

                var top_of_first_drag_item = dragged_item_intial_pos_y - under_mouse_idx*item_height + (mouseY - y_drag_start)
                var target_idx = Math.min(Math.max(Math.round(top_of_first_drag_item/item_height),0), not_dragging_group.length)

                for (var i = 0; i < dragging_group.length; ++i) {
                    // This starts animation to make the items slide into the gap
                    // in the list
                    dragging_group[i].drag_animation(0.0, (i + target_idx)*item_height, 100.0)
                    drag_drop_uuids.push(mediaList[dragging_group[i].currentIndex].uuid)
                }
                doing_drag = false

                if (target_idx < not_dragging_group.length) {
                    drag_drop_before_uuid = "" + mediaList[not_dragging_group[target_idx].currentIndex].uuid
                } else {
                    drag_drop_before_uuid = "" // no 'before' uuid ensures the items are put at the end of the list
                }

                // use a timer to execute the actual re-ordering when the
                // animation has completed
                dragDropFinishTimer.start()

            } else if (doing_drag && dragged_outside) {

                // force a rebuild of the items in the media list view
                var all_uuids = []
                for (var i = 0; i < mediaList.length; ++i) {
                    all_uuids.push(mediaList[i].uuid)
                }

                source.dragDropReorder(
                    all_uuids,
                    ""
                )

                sessionWidget.dropping_items_from_media_list(
                    source.uuid,
                    source.parent_playlist ? source.parent_playlist : source,
                    dragging_group,
                    mapToGlobal(mouseX, mouseY)
                    )

                dragging_group = []
                not_dragging_group = []

            } else if (under_mouse && under_mouse != freshly_selected) {

                // if the user clicks (and releases without starting a drag and drop) on a selected
                // item, the selection is reset to include only the item clicked on, i.e. deselects
                // any other items.
                if (selection_uuids.length == 1 && under_mouse.uuid == selection_uuids[0])
                {
                    selectSingleItem(under_mouse.currentIndex)
                }

            }
        }

        onContainsMouseChanged: {
            if (!containsMouse && !pressed && under_mouse) {
                under_mouse.underMouse = false
                under_mouse = undefined
            }
        }

        onWheel: {
            // TODO: also use wheel.pixelDelta for trackpad/wacom ?
            scroll_animator.mouseWheel(-wheel.angleDelta.y)

        }

        onDoubleClicked: {
            if (mouse.button == Qt.RightButton) return;
            if (under_mouse && source != session.onScreenSource) {
                session.switchOnScreenSource(source.uuid)
                selectSingleItem(under_mouse.currentIndex)
            }
            // show some viewer if none present.
            if((!app_window.popout_window || !app_window.popout_window.visible) && !playerWidget.visible) {
                app_window.togglePopoutViewer()
            }
        }

        onMouseXChanged: {
            if (!pressed && under_mouse) {
                if(mouseX > 0 && mouseX < under_mouse.media_item.thumb_width) {
                    var new_frame = Math.round(mouseX / under_mouse.media_item.thumb_width * 20.0) / 20.0
                    if(new_frame != under_mouse.preview_frame)
                        under_mouse.preview_frame = new_frame
                } else {
                    if(under_mouse.preview_frame !=  -1){
                        under_mouse.preview_frame = -1
                    }
                }
            }
        }

        onDragged_outsideChanged: {

            if (doing_drag && dragged_outside) {
                reverseDragDropAnimation()
                var drag_items = []
                for (var idx in dragging_group) {
                    drag_items.push(dragging_group[idx].media_item.media_item)
                }
                drag_outside_items = drag_items
            } else if (doing_drag && !dragged_outside) {

                var dx = (mouseX - x_drag_start)
                var dy = (mouseY - y_drag_start)
                updateDragDropAnimation(dx, dy, true)
                drag_outside_items = []
                sessionWidget.dragging_items_from_media_list(
                    source.uuid,
                    source.parent_playlist ? source.parent_playlist.uuid : null,
                    drag_outside_items,
                    mapToGlobal(0.0, 0.0)
                    )

            }
        }

        onMouseYChanged: {

            if (!pressed) {

                var idx = Math.floor(mouseY/item_height)
                if (idx < mediaList.length && idx >=0) {
                    if (under_mouse != repeater.itemAt(idx)) {
                        if (under_mouse) {
                            under_mouse.underMouse = false
                        }
                        under_mouse = repeater.itemAt(idx)
                        under_mouse.underMouse = true
                    }
                }

            } else if (under_mouse && mouse.buttons == Qt.LeftButton && mouse.modifiers == Qt.NoModifier) {

                // user clicked on a media item and has
                // started dragging... work out how far
                // they have dragged
                var dx = (mouseX - x_drag_start)
                var dy = (mouseY - y_drag_start)
                var delta = Math.sqrt(dx*dx + dy*dy)

                if ((delta > 30.0 || doing_drag) && !dragged_outside) {

                    if (!doing_drag) {

                        makeDragDropGroups()

                        dragged_item_intial_pos_x = under_mouse.x;
                        dragged_item_intial_pos_y = under_mouse.y;

                        updateDragDropAnimation(dx, dy, true)

                        doing_drag = true

                    } else {
                        updateDragDropAnimation(dx, dy, false)
                    }
                } else if (doing_drag && dragged_outside) {

                    sessionWidget.dragging_items_from_media_list(
                        source.uuid,
                        source.parent_playlist ? source.parent_playlist.uuid : null,
                        drag_outside_items,
                        mapToGlobal(mouseX, mouseY)
                        )
                }

            } else if (under_mouse && mouse.buttons == Qt.LeftButton && mouse.modifiers == Qt.AltModifier) {

                // user clicked on a media item and has
                // started dragging... work out how far
                // they have dragged
                var dx = (mouseX - x_drag_start)
                var dy = (mouseY - y_drag_start)
                var delta = Math.sqrt(dx*dx + dy*dy)

                if (delta > 30.0 || doing_drag) {
                    if (!doing_drag) {

                        makeDragDropGroups()

                        dragged_item_intial_pos_x = under_mouse.x;
                        dragged_item_intial_pos_y = under_mouse.y;

                        startDragOutside(dx, dy)

                        doing_drag = true

                    } else {

                        updateDragOutside(dx, dy)

                    }
                }
            }
        }

        // make ordered lists of media items that are being
        // drag/dropped and those that aren't being drag/dropped
        function makeDragDropGroups() {
            var new_dragging_group = []
            not_dragging_group = []
            for (var i = 0; i < mediaList.length; ++i) {
                var item = repeater.itemAt(i)
                if (item.selected) {
                    if (item == under_mouse) {
                        under_mouse_idx = new_dragging_group.length
                    }
                    new_dragging_group.push(item)
                } else {
                    not_dragging_group.push(item)
                }
                item.orig_x = item.x
                item.orig_y = item.y
            }
            dragging_group = new_dragging_group
        }

        function startDragOutside(dx, dy) {
            for (var i = 0; i < dragging_group.length; ++i) {
                dragging_group[i].being_dragged_outside = true;
            }
        }

        function updateDragOutside(dx, dy) {

        }

        function reverseDragDropAnimation() {

            for (var i = 0; i < not_dragging_group.length; ++i) {
                not_dragging_group[i].reverse_animation()
            }
            for (var i = 0; i < dragging_group.length; ++i) {
                dragging_group[i].reverse_animation()
            }

        }

        // As the user starts the drag drop, the selected items slide
        // together to form a continuous group stuck to the item that
        // is being dragged (the 'under_mouse' item). The items *not*
        // being dragged have to re-arrange themselves to make a gap
        // into which the drag-drop selection will be dropped.
        function updateDragDropAnimation(dx, dy, start_animation) {

            // this is the index in the list where the drag/drop set
            // would be dropped
            var top_of_first_drag_item = dragged_item_intial_pos_y - under_mouse_idx*item_height + dy
            var target_idx = Math.min(
                Math.max(
                    Math.round(top_of_first_drag_item/item_height),
                    0),
                not_dragging_group.length
                )

            // The size of the gap needed for them to fall into
            var gap_size = dragging_group.length*item_height

            // arrange the items not being dragged so they are continuous
            // up to the drag-drop index, and then a gap, and then continuous
            // again
            for (var i = 0; i < not_dragging_group.length; ++i) {
                if (i < target_idx) {
                    not_dragging_group[i].drag_animation(0, i*item_height, 0.0)
                } else {
                    not_dragging_group[i].drag_animation(0, i*item_height + gap_size, 0.0)
                }
            }

            if(not_dragging_group[target_idx]){
                autoscroll(false, not_dragging_group[target_idx].uuid)
            }

            // The items being dragged track the position of the 'under_mouse' item that
            // it the anchor for the drag. When the drag drop animation starts, they slide
            // into their position, otherwise they immediately track the position of the
            // 'under_mouse' key item
            if (start_animation) {
                for (var i = 0; i < dragging_group.length; ++i) {

                    dragging_group[i].drag_animation(
                        dragged_item_intial_pos_x + dx,
                        dragged_item_intial_pos_y + (i - under_mouse_idx)*item_height + dy,
                        100.0
                        )

                }
            } else {
                for (var i = 0; i < dragging_group.length; ++i) {

                    dragging_group[i].immediate_drag(
                        dragged_item_intial_pos_x + dx,
                        dragged_item_intial_pos_y + (i - under_mouse_idx)*item_height + dy
                        )

                }
            }
        }
    }
}
