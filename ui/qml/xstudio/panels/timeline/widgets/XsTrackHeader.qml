// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.1

Item {
	id: control

	property bool isHovered: false
	property string itemFlag: ""
	property string text: ""
	property int trackIndex: 0
	property var setTrackHeaderWidth: function(val) {}
	property string title: "Video Track"

	property bool isEnabled: false
	signal enabledClicked()

	property bool isFocused: false
	signal focusClicked()

	Rectangle {
		id: control_background

		color: Qt.darker(  trackBackground, isSelected ? 0.6 : 1.0)

		anchors.fill: parent

		RowLayout {
			clip: true
			spacing: 10
			anchors.fill: parent
			anchors.leftMargin: 10
			anchors.rightMargin: 10
			anchors.topMargin: 5
			anchors.bottomMargin: 5

			Rectangle {
				Layout.preferredHeight: parent.height/3
				Layout.preferredWidth: Layout.preferredHeight
				color: itemFlag != "" ? helpers.saturate(itemFlag, 0.4) : control_background.color
				border.width: 2
				border.color: Qt.lighter(color, 1.2)

				MouseArea {

					anchors.fill: parent
					onPressed: trackFlag.popup()
					cursorShape: Qt.PointingHandCursor

					XsFlagMenu {
						id:trackFlag
						onFlagSet: flagColourRole = (hex == "#00000000" ? "" : hex)
					}
				}
			}

		    Label {
		    	// Layout.preferredWidth: 20
		    	Layout.fillHeight: true
		    	horizontalAlignment: Text.AlignLeft
		    	verticalAlignment: Text.AlignVCenter
				text: control.title[0] + trackIndex
		    }

		    XsElideLabel {
		    	Layout.fillHeight: true
		    	Layout.fillWidth: true
		    	Layout.minimumWidth: 30
		    	Layout.alignment: Qt.AlignLeft
				elide: Qt.ElideRight
		    	horizontalAlignment: Text.AlignLeft
		    	verticalAlignment: Text.AlignVCenter
				text: control.text == "" ? control.title : control.text
		    }

		    GridLayout {
		    	Layout.fillHeight: true
		    	Layout.alignment: Qt.AlignRight

				Rectangle {
					Layout.preferredHeight: Math.min(Math.min(control.height - 20, control.width/3/4), 40)
					Layout.preferredWidth: Layout.preferredHeight

					color: control.isEnabled ? trackEdge : Qt.darker(trackEdge, 1.4)

					Label {
				    	horizontalAlignment: Text.AlignHCenter
				    	verticalAlignment: Text.AlignVCenter
						anchors.fill: parent
						text: "E"
					}
			    	MouseArea {
						anchors.fill: parent
						cursorShape: Qt.PointingHandCursor

						onClicked: {
							control.enabledClicked()
						}
				    }
		    	}

				Rectangle {
					Layout.preferredHeight: Math.min(Math.min(control.height - 20, control.width/3/4), 40)
					Layout.preferredWidth: Layout.preferredHeight

					color: control.isFocused ? trackEdge : Qt.darker(trackEdge, 1.4)

					Label {
				    	horizontalAlignment: Text.AlignHCenter
				    	verticalAlignment: Text.AlignVCenter
						anchors.fill: parent
						text: "F"
					}
			    	MouseArea {
						anchors.fill: parent
						cursorShape: Qt.PointingHandCursor

						onClicked: {
							control.focusClicked()
						}
				    }
		    	}
		    }


		// Label {
		// 	anchors.top: parent.top
		// 	anchors.left: parent.left
		// 	anchors.leftMargin: 10
		// 	anchors.topMargin: 5
		// 	text: trimmedStartRole
		// 	visible: extraDetail
		// 	z:4
		// }
		// Label {
		// 	anchors.top: parent.top
		// 	anchors.left: parent.left
		// 	anchors.leftMargin: 40
		// 	anchors.topMargin: 5
		// 	text: trimmedDurationRole
		// 	visible: extraDetail
		// 	z:4
		// }
		// Label {
		// 	anchors.top: parent.top
		// 	anchors.left: parent.left
		// 	anchors.leftMargin: 70
		// 	anchors.topMargin: 5
		// 	text: trimmedDurationRole ? trimmedStartRole + trimmedDurationRole - 1 : 0
		// 	visible: extraDetail
		// 	z:4
		// }
		}
	}

	Rectangle {
		width: 4
		height: parent.height

		anchors.right: parent.right
		anchors.top: parent.top
		color: timelineBackground

		MouseArea {
			id: ma
			anchors.fill: parent
            hoverEnabled: true
			acceptedButtons: Qt.LeftButton

			cursorShape: Qt.SizeHorCursor

			onPositionChanged: {
				if(pressed) {
					let ppos = mapToItem(control, mouse.x, 0)
					setTrackHeaderWidth(ppos.x + 4)
				}
			}
		}
	}
}

