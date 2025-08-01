// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudioReskin 1.0
import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0

DelegateChoice {
    roleValue: "Clip"

    Component {
    	RowLayout {
    		id: control
    		spacing: 0

    		property var config: ListView.view || control.parent

			width: (durationFrame + adjustPreceedingGap + adjustAnteceedingGap) * config.scaleX
			height: config.scaleY * config.itemHeight

    		property bool isAdjustPreceeding: false
    		property bool isAdjustAnteceeding: false

    		property int adjustPreceedingGap: 0
    		property int adjustAnteceedingGap: 0

    		property bool isBothHovered: false

	        property bool isAdjustingStart: false
	        property int adjustStart: 0
			property int startFrame: isAdjustingStart ? trimmedStartRole + adjustStart : trimmedStartRole

	        property bool isAdjustingDuration: false
	        property int adjustDuration: 0
			property int durationFrame: isAdjustingDuration ? trimmedDurationRole + adjustDuration : trimmedDurationRole
			property int currentStartRole: trimmedStartRole
			property real fps: rateFPSRole

			property var timelineFocusSelection: config.timelineFocusSelection
			property var timelineSelection: config.timelineSelection
            property var timelineItem: config.timelineItem
            property var itemTypeRole: typeRole
            property var hoveredItem: config.hoveredItem
            property var scaleX: config.scaleX
            property var parentLV: config
            property string itemFlag: flagColourRole != "" ? flagColourRole : config.itemFlag

            property bool hasMedia: mediaIndex.valid
            property var mediaIndex: control.DelegateModel.model.srcModel.index(-1,-1, control.DelegateModel.model.rootIndex)

			onHoveredItemChanged: isBothHovered = false

			function modelIndex() {
				return control.DelegateModel.model.srcModel.index(
	    			index, 0, control.DelegateModel.model.rootIndex
	    		)
			}

			function adjust(offset) {
				let doffset = offset
				if(isAdjustingStart) {
					adjustStart = offset
					doffset = -doffset
				}
				if(isAdjustingDuration) {
					adjustDuration = doffset
				}
			}

			function checkAdjust(offset, lock_duration=false, lock_end=false) {
				let doffset = offset

				if(isAdjustingStart) {
					let tmp = Math.min(
						availableStartRole+availableDurationRole-1,
						Math.max(trimmedStartRole + offset, availableStartRole)
					)

					if(lock_end && tmp > trimmedStartRole+trimmedDurationRole) {
						tmp = trimmedStartRole+trimmedDurationRole-1
					}

					if(trimmedStartRole != tmp-offset) {
						return checkAdjust(tmp-trimmedStartRole)
					}

					// if adjusting duration as well
					doffset = -doffset
				}

				if(isAdjustingDuration && lock_duration) {
					let tmp = Math.max(
						1,
						Math.min(trimmedDurationRole + doffset, availableDurationRole - (startFrame-availableStartRole) )
					)

					if(trimmedDurationRole != tmp-doffset) {
						if(isAdjustingStart)
							return checkAdjust(-(tmp-trimmedDurationRole))
						else
							return checkAdjust(tmp-trimmedDurationRole)
					}
				}

				return offset
			}


	        function updateStart(startX, x) {
	        	let tmp = - (startX - x) * ((availableDurationRole - activeDurationRole) / width)
        		adjustStart = Math.floor(Math.min(
        			Math.max(trimmedStartRole + tmp, availableStartRole),
        			availableStartRole + availableDurationRole - trimmedDurationRole
        		) - trimmedStartRole)
	        }



    		XsGapItem {
    			visible: adjustPreceedingGap != 0
    			Layout.preferredWidth: adjustPreceedingGap * scaleX
    			Layout.fillHeight: true
    			start: 0
    			duration: adjustPreceedingGap
    		}

	    	XsClipItem {
				id: clip

				Layout.preferredWidth: durationFrame * scaleX
				Layout.fillHeight: true

				isHovered: hoveredItem == control || isAdjustingStart || isAdjustingDuration || isBothHovered
				start: startFrame
				duration: durationFrame
				isEnabled: enabledRole && hasMedia
				fps: control.fps
				name: nameRole
				parentStart: parentStartRole
				availableStart: availableStartRole
				availableDuration: availableDurationRole
				primaryColor: itemFlag != "" ?  itemFlag : defaultClip
	            mediaFlagColour: mediaFlag.value == undefined || mediaFlag.value == "" ? "transparent" : mediaFlag.value


			    XsModelProperty {
			        id: mediaFlag
			        role: "flagColourRole"
			        index: mediaIndex
			    }

			    Component.onCompleted: {
			    	checkMedia()
			    }

			    function checkMedia() {
			    	let model = control.DelegateModel.model.srcModel
			    	let tindex = model.getTimelineIndex(control.DelegateModel.model.rootIndex)
			    	let mlist = model.index(0, 0, tindex)
			    	mediaIndex = model.search(clipMediaUuidRole, "actorUuidRole", mlist)
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

				Connections {
					target: control.timelineFocusSelection
					function onSelectionChanged(selected, deselected) {
						if(clip.isFocused && helpers.itemSelectionContains(deselected, modelIndex()))
							clip.isFocused = false
						else if(!clip.isFocused && helpers.itemSelectionContains(selected, modelIndex()))
							clip.isFocused = true
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
	}
}
