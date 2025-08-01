// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1
import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0

DelegateChoice {
    roleValue: "Gap"

    Component {
    	XsGapItem {
			id: control

    		property var config: ListView.view || control.parent

			width: durationFrame * config.scaleX
			height: config.scaleY * config.itemHeight

			isHovered: hoveredItem == control

			start: startFrame
			duration: durationFrame
			fps: rateFPSRole
			name: nameRole
			parentStart: parentStartRole
			isEnabled: enabledRole

			property int adjustDuration: 0
			property bool isAdjustingDuration: false
			property int adjustStart: 0
			property bool isAdjustingStart: false
			property int durationFrame: isAdjustingDuration ? trimmedDurationRole + adjustDuration : trimmedDurationRole
			property int startFrame: isAdjustingStart ? trimmedStartRole + adjustStart : trimmedStartRole
            property var itemTypeRole: typeRole

			property var timelineSelection: config.timelineSelection
			property var timelineFocusSelection: config.timelineFocusSelection
            property var timelineItem: config.timelineItem
            property var parentLV: config
            property var hoveredItem: config.hoveredItem

			function adjust(offset) {
				adjustDuration = offset
			}

			// we only ever adjust duration
			function checkAdjust(offset) {
				let tmp = Math.max(0, trimmedDurationRole + offset)

				if(trimmedDurationRole != tmp-offset) {
					// console.log("duration limited", trimmedDurationRole, tmp-doffset)
					return checkAdjust(tmp-trimmedDurationRole)
				}

				return offset
			}

			function modelIndex() {
				return control.DelegateModel.model.srcModel.index(
	    			index, 0, control.DelegateModel.model.rootIndex
	    		)
			}

			Connections {
				target: timelineSelection
				function onSelectionChanged(selected, deselected) {
					if(isSelected && helpers.itemSelectionContains(deselected, modelIndex()))
						isSelected = false
					else if(!isSelected && helpers.itemSelectionContains(selected, modelIndex()))
						isSelected = true
				}
			}
		}
	}
}
