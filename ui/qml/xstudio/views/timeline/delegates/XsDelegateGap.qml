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
	isEnabled: enabledRole

	property int adjustDuration: "adjust_duration" in userDataRole ? userDataRole.adjust_duration : 0
	property bool isAdjustingDuration: "is_adjusting_duration" in userDataRole ? userDataRole.is_adjusting_duration : false
	property int adjustStart: "adjust_start" in userDataRole ? userDataRole.adjust_start : 0
	property bool isAdjustingStart: "is_adjusting_start" in userDataRole ? userDataRole.is_adjusting_start : false
	property int durationFrame: isAdjustingDuration ? trimmedDurationRole + adjustDuration : trimmedDurationRole
	property int startFrame: isAdjustingStart ? trimmedStartRole + adjustStart : trimmedStartRole
    property var itemTypeRole: typeRole

	property var timelineSelection: config.timelineSelection
	property var timelineFocusSelection: config.timelineFocusSelection
    property var timelineItem: config.timelineItem
    property var parentLV: config
    property var hoveredItem: config.hoveredItem

	function modelIndex() {
		return helpers.makePersistent(DelegateModel.model.modelIndex(index))
	}

	Connections {
		target: timelineSelection
		function onSelectionChanged(selected, deselected) {
			if(isSelected && helpers.itemSelectionContains(deselected, control.DelegateModel.model.modelIndex(index)))
				isSelected = false
			else if(!isSelected && helpers.itemSelectionContains(selected, control.DelegateModel.model.modelIndex(index)))
				isSelected = true
		}
	}
}
