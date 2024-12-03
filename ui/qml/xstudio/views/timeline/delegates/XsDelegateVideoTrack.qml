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

Rectangle {
	id: control

	color: timelineBackground
	property real scaleX: ListView.view.scaleX
	property real scaleY: ListView.view.scaleY
	property real itemHeight: ListView.view.itemHeight
	property real trackHeaderWidth: ListView.view.trackHeaderWidth
	property real cX: ListView.view.cX
    property real parentWidth: ListView.view.parentWidth
    property var timelineItem: ListView.view.timelineItem
    property string itemFlag: flagColourRole != "" ? flagColourRole : ListView.view.itemFlag
    property var parentLV: ListView.view
    readonly property bool extraDetail: height > 60
    property var setTrackHeaderWidth: ListView.view.setTrackHeaderWidth

	property bool isSizerHovered: ListView.view.isSizerHovered
	property bool isSizerDragging: ListView.view.isSizerDragging
    property var setSizerHovered: ListView.view.setSizerHovered
    property var setSizerDragging: ListView.view.setSizerDragging

    property var draggingStarted: ListView.view.draggingStarted
    property var dragging: ListView.view.dragging
    property var draggingStopped: ListView.view.draggingStopped
    property var doubleTapped: ListView.view.doubleTapped
    property var tapped: ListView.view.tapped

	width: ListView.view.width
	height: itemHeight * scaleY

	property bool isHovered: hoveredItem == control
	property bool isSelected: false
	property bool isConformSource:  DelegateModel.model.modelIndex(index) == conformSourceIndex
	property var timelineSelection: ListView.view.timelineSelection
    property var hoveredItem: ListView.view.hoveredItem
    property var itemTypeRole: typeRole
    property alias list_view: list_view

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

	XsTrackHeader {
    	id: track_header
		z: 2
		anchors.top: parent.top
		anchors.left: parent.left

		width: trackHeaderWidth
		height: Math.ceil(control.itemHeight * control.scaleY)

		isHovered: control.isHovered
		itemFlag: control.itemFlag
		trackIndex: trackIndexRole
		setTrackHeaderWidth: control.setTrackHeaderWidth
		text: nameRole
		title: "Video Track"
		isEnabled: enabledRole
		isLocked: lockedRole
		isSelected: control.isSelected
		isConformSource: control.isConformSource
		isSizerHovered: control.isSizerHovered
		isSizerDragging: control.isSizerDragging
		notificationModel: notificationRole

		onSizerHovered: setSizerHovered(hovered)
		onSizerDragging: setSizerDragging(dragging)

		onEnabledClicked: enabledRole = !enabledRole
		onLockedClicked: lockedRole = !lockedRole
		onConformSourceClicked: conformSourceIndex = helpers.makePersistent(control.DelegateModel.model.modelIndex(index))
		onFlagSet: flagItems([control.DelegateModel.model.modelIndex(index)], flag == "#00000000" ? "": flag)
	}

	Flickable {
		id: flicker
		property bool forceEval: false
		anchors.top: parent.top
		anchors.bottom: parent.bottom
		anchors.left: track_header.right
		anchors.right: parent.right

		interactive: false

		contentWidth: contentItem.childrenRect.width
		contentHeight: contentItem.childrenRect.height
		contentX: (forceEval && !forceEval) || control.cX

		onContentWidthChanged: {
			if(contentX != control.cX) {
				forceEval = !forceEval
			}
		}
		clip: false

    	Row {
    	    id:list_view
			clip: false
			// opacity: isHovered ? 1.0 : enabledRole ? (lockedRole ? 0.6 : 1.0) : 0.3

	        property real scaleX: control.scaleX
	        property real scaleY: control.scaleY
	        property real itemHeight: control.itemHeight
			property var timelineSelection: control.timelineSelection
            property var timelineItem: control.timelineItem
            property var hoveredItem: control.hoveredItem
            property real trackHeaderWidth: control.trackHeaderWidth
			property string itemFlag: control.itemFlag
			// property bool isParentLocked: lockedRole
			property var itemAtIndex: item_repeater.itemAt
            property var parentLV: control.parentLV

            property var draggingStarted: control.draggingStarted
            property var dragging: control.dragging
            property var draggingStopped: control.draggingStopped
		    property var doubleTapped: control.doubleTapped
		    property var tapped: control.tapped

			property bool isParentLocked: lockedRole
	        property bool isParentEnabled: enabledRole

	    	Repeater {
	    		id: item_repeater
				model: DelegateModel {
					id: track_items
			        property var srcModel: theSessionData
			        model: srcModel
			        rootIndex: helpers.makePersistent(control.DelegateModel.model.modelIndex(index))

			        delegate: DelegateChooser {
				        role: "typeRole"

						DelegateChoice {
						    roleValue: "Clip"
					        XsDelegateClip {}
					    }

						DelegateChoice {
						    roleValue: "Gap"
					        XsDelegateGap {}
					    }
				    }
			    }
    		}
    	}
	}
}
