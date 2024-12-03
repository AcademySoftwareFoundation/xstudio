// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

Item {
	id: control

	property bool isHovered: false
	property bool isSelected: false
	property string itemFlag: ""
	property string text: ""
	property int trackIndex: 0
	property var setTrackHeaderWidth: function(val) {}
	property string title: "Video Track"

	property bool isEnabled: false
	property bool isLocked: false
	property bool isConformSource: false
	property bool dragTarget: false

	property bool isSizerHovered: false
	property bool isSizerDragging: false
	property var notificationModel: []

	signal sizerHovered(bool hovered)
	signal sizerDragging(bool dragging)

	signal enabledClicked()
	signal lockedClicked()
	signal conformSourceClicked()
	signal flagSet(string flag, string flag_text)

	XsGradientRectangle {
		id: control_background

		anchors.fill: parent
		anchors.rightMargin: 4

		border.width: 1
        border.color: (isHovered || dragTarget) ? palette.highlight : "transparent"

		flatColor: topColor

        topColor: isSelected ? Qt.darker(palette.highlight, 2) : XsStyleSheet.panelBgGradTopColor
        bottomColor: isSelected ? Qt.darker(palette.highlight, 2) :  XsStyleSheet.panelBgGradBottomColor

		RowLayout {
			spacing: 10
			anchors.fill: parent
			anchors.leftMargin: 10
			anchors.rightMargin: 10
			anchors.topMargin: 5
			anchors.bottomMargin: 5

			Rectangle {
				Layout.fillHeight: true
				Layout.preferredWidth: height / 2
				color: itemFlag != "" ? helpers.saturate(itemFlag, 0.4) : control_background.color
				border.width: 2
				border.color: Qt.lighter(color, 1.2)

				MouseArea {
					anchors.fill: parent
					onPressed: showFlagMenu(mouse.x, mouse.y, this, flagSet)
					cursorShape: Qt.PointingHandCursor
				}
			}

		    Label {
		    	Layout.fillHeight: true
		    	Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

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
	        	font.pixelSize: XsStyleSheet.fontSize *1.1

		    }

		    Repeater {
		        Layout.fillHeight: true
		        Layout.alignment: Qt.AlignRight
		        model: notificationModel

		        // notify widget
		        XsNotification {
		            Layout.fillHeight: true
		            Layout.preferredWidth: height
			        Layout.alignment: Qt.AlignRight
		            text: modelData.text
		            type: modelData.type
		            percentage: modelData.progress_percent || 0.0
		        }
		    }

		    GridLayout {
		    	Layout.fillHeight: true
		    	Layout.alignment: Qt.AlignRight

				Rectangle {
			    	Layout.fillHeight: true
					Layout.preferredWidth: height
					visible: isConformSource
					color: "transparent"
					border.width: 2
					border.color: XsStyleSheet.widgetBgNormalColor

					XsLabel {
						anchors.fill: parent
	    	            text: "C"
					}
				}

				XsPrimaryButton{
			    	Layout.fillHeight: true
					Layout.preferredWidth: height
					imageDiv.height: height-2
					imageDiv.width: height-2
	                imgSrc: isEnabled ? "qrc:/icons/visibility.svg" : "qrc:/icons/visibility_off.svg"
					isActiveViaIndicator: false
    	            isActive: !isEnabled
					onClicked: control.enabledClicked()
				}


				XsPrimaryButton{
			    	Layout.fillHeight: true
					Layout.preferredWidth: height
					imageDiv.height: height-2
					imageDiv.width: height-2
	                imgSrc: isLocked ? "qrc:/icons/lock.svg" : "qrc:/icons/unlock.svg"
					isActiveViaIndicator: false
    	            isActive: isLocked
					onClicked: control.lockedClicked()
				}
		    }
		}
	}

	Rectangle {
		width: 4
		height: parent.height

		anchors.right: parent.right
		anchors.top: parent.top

        color: isSizerDragging ? palette.highlight : isSizerHovered  ? XsStyleSheet.secondaryTextColor : timelineBackground

		MouseArea {
			id: ma
			anchors.fill: parent
            hoverEnabled: true
			acceptedButtons: Qt.LeftButton
	        preventStealing: true
			cursorShape: Qt.SizeHorCursor

			onContainsMouseChanged: sizerHovered(containsMouse)

			onPressedChanged: sizerDragging(pressed)

			onPositionChanged: {
				if(pressed) {
					let ppos = mapToItem(control, mouse.x, 0)
					setTrackHeaderWidth(ppos.x + 4)
				}
			}
		}
	}
}

