// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.helpers 1.0

RowLayout {
	id: control
	spacing: 0

	property var config: ListView.view || control.parent

	width: (isFloating ? trimmedDurationRole : durationFrame + adjustPreceedingGap + adjustAnteceedingGap) * config.scaleX
	height: config.scaleY * config.itemHeight

	property bool showDragLeft: "show_drag_left" in userDataRole ? userDataRole.show_drag_left : false
	property bool showDragRight: "show_drag_right" in userDataRole ? userDataRole.show_drag_right : false
	property bool showDragMiddle: "show_drag_middle" in userDataRole ? userDataRole.show_drag_middle : false
	property bool showDragLeftLeft: "show_drag_left_left" in userDataRole ? userDataRole.show_drag_left_left : false
	property bool showDragRightRight: "show_drag_right_right" in userDataRole ? userDataRole.show_drag_right_right : false
	property bool showRolling: "show_rolling" in userDataRole ? userDataRole.show_rolling : false

	property bool isAdjustPreceeding: "is_adjusting_preceeding" in userDataRole ? userDataRole.is_adjusting_preceeding : false
	property bool isAdjustAnteceeding: "is_adjusting_anteceeding" in userDataRole ? userDataRole.is_adjusting_anteceeding : false

	property int adjustPreceedingGap: "adjust_preceeding_gap" in userDataRole ? userDataRole.adjust_preceeding_gap : 0
	property int adjustAnteceedingGap: "adjust_anteceeding_gap" in userDataRole ? userDataRole.adjust_anteceeding_gap : 0

	property int moveX: "move_x" in userDataRole ? userDataRole.move_x : 0
	property int moveY: "move_y" in userDataRole ? userDataRole.move_y : 0

	property bool isFloating: "is_floating" in userDataRole ? userDataRole.is_floating : false

	property int dragValue: "drag_value" in userDataRole ? userDataRole.drag_value : 0

	property int adjustDuration: "adjust_duration" in userDataRole ? userDataRole.adjust_duration : 0
	property bool isAdjustingDuration: "is_adjusting_duration" in userDataRole ? userDataRole.is_adjusting_duration : false
	property int adjustStart: "adjust_start" in userDataRole ? userDataRole.adjust_start : 0
	property bool isAdjustingStart: "is_adjusting_start" in userDataRole && userDataRole.is_adjusting_start != null ? userDataRole.is_adjusting_start : false

	property int startFrame: isAdjustingStart ? trimmedStartRole + adjustStart : trimmedStartRole
	property int durationFrame: isAdjustingDuration ? trimmedDurationRole + adjustDuration : trimmedDurationRole

	property int currentDurationFrame: trimmedDurationRole
	property real fps: rateFPSRole

	property var timelineSelection: config.timelineSelection
    property var timelineItem: config.timelineItem
    property var itemTypeRole: typeRole
    property var hoveredItem: config.hoveredItem
    property var scaleX: config.scaleX
    property var parentLV: config

    property bool isParentLocked: config.isParentLocked
    property bool isParentEnabled: config.isParentEnabled

    property var draggingStarted: config.draggingStarted
    property var dragging: config.dragging
    property var draggingStopped: config.draggingStopped
    property var doubleTapped: config.doubleTapped
    property var tapped: config.tapped

    property string itemFlag: flagColourRole != "" ? flagColourRole : config.itemFlag

    property bool hasMedia: !trimmedDurationRole || clipMediaUuidRole == "{00000000-0000-0000-0000-000000000000}" || mediaIndex.valid
    property alias mediaIndex: mediaStatus.index

    property alias isSelected: clip.isSelected
    property var isLocked: lockedRole

	property var mediaUuid: clipMediaUuidRole

	onMediaUuidChanged: updateMediaIndex()

	XsModelProperty {
		id: mediaStatus
		role: "mediaStatusRole"
		onIndexChanged: {
			if(!index.valid && clipMediaUuidRole && clipMediaUuidRole != "{00000000-0000-0000-0000-000000000000}")
				updateMediaIndex()
		}
	}

	function modelIndex() {
		return helpers.makePersistent(DelegateModel.model.modelIndex(index))
	}

    Timer {
        id: updateTimer
        interval: 500
        running: false
        repeat: false
        onTriggered: {
        	updateMediaIndex()
        }
    }

    function updateMediaIndex(retry=true) {
    	let m = DelegateModel.model.srcModel
    	let tindex = m.getTimelineIndex(DelegateModel.model.modelIndex(index))
    	let mlist = m.index(0, 0, tindex)
    	let result = m.search(clipMediaUuidRole, "actorUuidRole", mlist)

    	if(retry && !result.valid && clipMediaUuidRole && clipMediaUuidRole != "{00000000-0000-0000-0000-000000000000}" && !updateTimer.running) {
    		updateTimer.start()
    	} else {
    		result = helpers.makePersistent(result)
    		if(mediaStatus.index != result)
    			mediaStatus.index = result
    		if(mediaFlag.index != result)
    			mediaFlag.index = result
    	}
    }

	XsGapItem {
		visible: adjustPreceedingGap != 0
		Layout.preferredWidth: adjustPreceedingGap * scaleX
		Layout.fillHeight: true
		start: 0
		duration: adjustPreceedingGap
	}

	onIsFloatingChanged: {
		if(isFloating) {
	    	// let new_parent = control.parent.parent.parent.parent
	    	let new_parent = control.parent.parent.parent.parent.parent.parent
			let orig = clip.mapFromItem(new_parent, clip.x, clip.y)
			clip.parent = new_parent
			clip.mappedX = -orig.x
			clip.mappedY = -orig.y
		} else {
			clip.parent = control
			clip.mappedX = 0
			clip.mappedY = 0
		}
	}

	XsClipItem {
		id: clip

		x: mappedX + (moveX * scaleX)
		y: mappedY + (moveY * (config.itemHeight+1) * scaleY)

		property real mappedX: 0
		property real mappedY: 0

		Layout.minimumWidth: durationFrame * scaleX
		Layout.maximumWidth: durationFrame * scaleX
		width: durationFrame * scaleX
		Layout.fillHeight: true

		isHovered: hoveredItem == control || isAdjustingStart || isAdjustingDuration
		start: startFrame
		duration: durationFrame
		isLocked: (isParentLocked || lockedRole)
		isEnabled: isParentEnabled && enabledRole
		isMissingMedia: !hasMedia || (mediaStatus.value != undefined && mediaStatus.value != "Online")
		isInvalidRange: !activeRangeValidRole

		showRolling: isSelected && isHovered && control.showRolling && !isParentLocked && !lockedRole
		showDragLeft: (isSelected || isHovered) && control.showDragLeft && !isParentLocked && !lockedRole
		showDragRight: (isSelected || isHovered) && control.showDragRight && !isParentLocked && !lockedRole
		showDragMiddle: (isSelected || isHovered) && control.showDragMiddle && !isParentLocked && !lockedRole
		showDragLeftLeft: (isSelected || isHovered) && control.showDragLeftLeft && !isParentLocked && !lockedRole
		showDragRightRight: (isSelected || isHovered) && control.showDragRightRight && !isParentLocked && !lockedRole

		name: nameRole
		dragValue: control.dragValue

		availableStart: availableStartRole
		availableDuration: availableDurationRole
		primaryColor: itemFlag != "" ?  itemFlag : defaultClip
        mediaFlagColour: mediaFlag.value == undefined || mediaFlag.value == "" ? "transparent" : mediaFlag.value

	    XsModelProperty {
	        id: mediaFlag
	        role: "flagColourRole"
			onIndexChanged: {
				if(!index.valid && clipMediaUuidRole && clipMediaUuidRole != "{00000000-0000-0000-0000-000000000000}")
					updateMediaIndex()
			}
	    }

	    Component.onCompleted: {
	    	updateMediaIndex()
	    }

	    onDraggingStarted: {
	    	control.draggingStarted(modelIndex(), control, mode)
	    	isDragging = true

	  //   	if(mode == "middle" && !rippleMode) {
		 //    	let new_parent = control.parent.parent.parent.parent
			// 	let orig = mapFromItem(new_parent, x, y)
			// 	clip.parent = new_parent
			// 	mappedX = -orig.x
			// 	mappedY = -orig.y
			// }
	    }
		onDragging: control.dragging(modelIndex(), control, mode, x / scaleX, y / scaleY / config.itemHeight)
		onDoubleTapped: control.doubleTapped(control, mode)
		onTapped: control.tapped(button, x, y, modifiers, control)
		onDraggingStopped: {
			control.draggingStopped(modelIndex(), control, mode)
	    	isDragging = false
			// if(mode == "middle" && !rippleMode) {
			// 	clip.parent = control
			// 	mappedX = 0
			// 	mappedY = 0
			// }
		}

	    Connections {
	    	target: dragContainer.dragged_items
	    	function onSelectionChanged() {
	    		if(dragContainer.dragged_items.selectedIndexes.length) {
	    			if(dragContainer.dragged_items.isSelected(modelIndex())) {
	    				if(dragContainer.Drag.supportedActions == Qt.CopyAction)
	    					clip.isCopying = true
	    				else
	    					clip.isMoving = true
	    			}
	    		} else {
	    			clip.isMoving = false
	    			clip.isCopying = false
	    		}
	    	}
	    }

		Connections {
			target: control.timelineSelection
			function onSelectionChanged(selected, deselected) {
				if(clip.isSelected && helpers.itemSelectionContains(deselected, modelIndex()))
					clip.isSelected = false
				else if(!clip.isSelected && helpers.itemSelectionContains(selected, modelIndex()))
					clip.isSelected = true
			}
		}
	}

	XsGapItem {
		visible: adjustAnteceedingGap != 0
		Layout.preferredWidth: adjustAnteceedingGap * scaleX
		Layout.fillHeight: true
		start: 0
		duration: adjustAnteceedingGap
	}
}
