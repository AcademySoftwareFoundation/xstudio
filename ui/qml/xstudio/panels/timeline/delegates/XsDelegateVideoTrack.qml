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
import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0

DelegateChoice {
    roleValue: "Video Track"

    Component {
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

			width: ListView.view.width
			height: itemHeight * scaleY

			opacity: enabledRole ? 1.0 : 0.2

			property bool isHovered: hoveredItem == control
			property bool isSelected: false
			property bool isFocused: false
			property var timelineSelection: ListView.view.timelineSelection
			property var timelineFocusSelection: ListView.view.timelineFocusSelection
            property var hoveredItem: ListView.view.hoveredItem
            property var itemTypeRole: typeRole

            property alias list_view: list_view

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

    		Connections {
				target: timelineFocusSelection
				function onSelectionChanged(selected, deselected) {
					if(isFocused && helpers.itemSelectionContains(deselected, modelIndex()))
						isFocused = false
					else if(!isFocused && helpers.itemSelectionContains(selected, modelIndex()))
						isFocused = true
				}
			}

		    DelegateChooser {
		        id: chooser
		        role: "typeRole"

		        XsDelegateClip {}
		        XsDelegateGap {}
		    }

		    DelegateModel {
		        id: track_items
		        property var srcModel: app_window.sessionModel
		        model: srcModel
		        rootIndex: helpers.makePersistent(control.DelegateModel.model.srcModel.index(
		    		index, 0, control.DelegateModel.model.rootIndex
		    	))
		        delegate: chooser
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
				isEnabled: enabledRole
				isFocused: control.isFocused
				onFocusClicked: timelineFocusSelection.select(modelIndex(), ItemSelectionModel.Toggle)
				onEnabledClicked: enabledRole = !enabledRole
	    	}

	    	Flickable {
				anchors.top: parent.top
				anchors.bottom: parent.bottom
				anchors.left: track_header.right
				anchors.right: parent.right

				interactive: false

				contentWidth: Math.ceil(trimmedDurationRole * control.scaleX)
				contentHeight: Math.ceil(control.itemHeight * control.scaleY)
				contentX: control.cX

		    	Row {
		    	    id:list_view

			        property real scaleX: control.scaleX
			        property real scaleY: control.scaleY
			        property real itemHeight: control.itemHeight
	    			property var timelineSelection: control.timelineSelection
	    			property var timelineFocusSelection: control.timelineFocusSelection
	                property var timelineItem: control.timelineItem
	                property var hoveredItem: control.hoveredItem
	                property real trackHeaderWidth: control.trackHeaderWidth
					property string itemFlag: control.itemFlag

					property var itemAtIndex: item_repeater.itemAt

			    	Repeater {
			    		id: item_repeater
						model: track_items
		    		}
		    	}
	    	}
		}
	}
}
