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
    roleValue: "Stack"

    Component {
		Rectangle {
			id: control

			width: ListView.view.width
			height: ListView.view.height

			property real myWidth: ((duration.value ? duration.value : 0) * scaleX) //+ trackHeaderWidth// + 10
			property real parentWidth: Math.max(ListView.view.width, myWidth + trackHeaderWidth)

			color: timelineBackground

			// needs to dynamicy resize badsed on listview..
			// in the mean time hack..

			property real scaleX: ListView.view.scaleX
			property real scaleY: ListView.view.scaleY
			property real itemHeight: ListView.view.itemHeight
			property real timelineHeaderHeight: itemHeight
			property real trackHeaderWidth: ListView.view.trackHeaderWidth
            property var setTrackHeaderWidth: ListView.view.setTrackHeaderWidth

            property string itemFlag: flagColourRole != "" ? flagColourRole : ListView.view.itemFlag

			opacity: enabledRole ? 1.0 : 0.2

			property bool isSelected: false
			property bool isHovered: hoveredItem == control

			property var timelineSelection: ListView.view.timelineSelection
			property var timelineFocusSelection: ListView.view.timelineFocusSelection
	        property int playheadFrame: ListView.view.playheadFrame
            property var timelineItem: ListView.view.timelineItem
            property var hoveredItem: ListView.view.hoveredItem

            property var itemTypeRole: typeRole
            property alias list_view_video: list_view_video
            property alias list_view_audio: list_view_audio

			function modelIndex() {
				return control.DelegateModel.model.srcModel.index(
	    			index, 0, control.DelegateModel.model.rootIndex
	    		)
			}

			// function viewStartFrame() {
			// 	return trimmedStartRole + ((myWidth * hbar.position)/scaleX);
			// }

			// function viewEndFrame() {
			// 	return trimmedStartRole + ((myWidth * (hbar.position+hbar.size))/scaleX);
			// }

			function jumpToStart() {
				if(hbar.size<1.0)
					hbar.position = 0.0
			}

			function jumpToEnd() {
				if(hbar.size<1.0)
					hbar.position = 1.0 - hbar.size
			}


			// ListView.Center
			// ListView.Beginning
			// ListView.End
			// ListView.Visible
			// ListView.Contain
			// ListView.SnapPosition

			function jumpToFrame(frame, mode) {
				if(hbar.size<1.0) {
					let new_position = hbar.position
					let first = ((frame - trimmedStartRole) * scaleX) / myWidth

					if(mode == ListView.Center) {
						new_position = first - (hbar.size / 2)
					} else if(mode == ListView.Beginning) {
						new_position = first
					} else if(mode == ListView.End) {
						new_position = (first - hbar.size) - (2 * (1.0 / (trimmedDurationRole * scaleX)))
					} else if(mode == ListView.Visible) {
						// calculate frame as position.
						if(first < new_position) {
							new_position -= (hbar.size / 2)
						} else if(first > (new_position + hbar.size)) {
							// reposition
							new_position += (hbar.size / 2)
						}
					}

					return hbar.position = Math.max(0, Math.min(new_position, 1.0 - hbar.size))
				}
				return hbar.position
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

		    DelegateChooser {
		        id: chooser
		        role: "typeRole"

		        XsDelegateClip {}
		        XsDelegateGap {}
		        XsDelegateAudioTrack {}
		        XsDelegateVideoTrack {}
		    }


 		    XsSortFilterModel {
		        id: video_items
		        srcModel: app_window.sessionModel
		        rootIndex: helpers.makePersistent(control.DelegateModel.model.srcModel.index(
		    		index, 0, control.DelegateModel.model.rootIndex
		    	))
		        delegate: chooser

		        filterAcceptsItem: function(item) {
		        	return item.typeRole == "Video Track"
		        }

		        lessThan: function(left, right) {
		        	return left.index > right.index
		        }
		        // onUpdated: console.log("video_items updated")
		    }

 		    XsSortFilterModel {
		        id: audio_items
		        srcModel: app_window.sessionModel
		        rootIndex: helpers.makePersistent(control.DelegateModel.model.srcModel.index(
		    		index, 0, control.DelegateModel.model.rootIndex
		    	))
		        delegate: chooser

		        filterAcceptsItem: function(item) {
		        	return item.typeRole == "Audio Track"
		        }

		        lessThan: function(left, right) {
		        	return left.index < right.index
		        }
		        // onUpdated: console.log("audio_items updated")
		    }

			Connections {
			    target: app_window.sessionModel

			    function onRowsMoved(parent, first, count, target, first) {
			    	Qt.callLater(video_items.update)
			    	Qt.callLater(audio_items.update)
			    }
			}


		    // capture pointer to stack, so we can watch it's available size
	        XsModelProperty {
		        id: duration
        		role: "trimmedDurationRole"
        		index: control.DelegateModel.model.rootIndex
    		}

			XsTimelineCursor {
				z:10
				anchors.left: parent.left
				anchors.leftMargin: trackHeaderWidth
				anchors.right: parent.right
				anchors.top: parent.top
				height: control.height

				tickWidth: tickWidget.tickWidth
				secondOffset: tickWidget.secondOffset
				fractionOffset: tickWidget.fractionOffset
				start: tickWidget.start
				duration: tickWidget.duration
				fps: tickWidget.fps
				position: playheadFrame
			}

		    ScrollBar {
		        id: hbar
		        hoverEnabled: true
		        active: hovered || pressed
		        orientation: Qt.Horizontal

		        size: width / myWidth //(myWidth - trackHeaderWidth)

		        // onSizeChanged: {
		        // 	console.log("size", size, "position", position, )
		        // }

		        anchors.left: parent.left
		        anchors.leftMargin: trackHeaderWidth
		        anchors.right: parent.right
		        anchors.bottom: parent.bottom
	            policy: size < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
	            z:11
		    }

            ColumnLayout {
            	id: splitView
                anchors.fill: parent
                spacing: 0

	    		ColumnLayout {
	    			id: topView
                    Layout.minimumWidth: parent.width
                    Layout.minimumHeight: (itemHeight * control.scaleY) * 2
                    Layout.preferredHeight: parent.height*0.7
	    			spacing: 0

	    			RowLayout {
		    			spacing: 0
	    				Layout.preferredHeight: timelineHeaderHeight
	    				Layout.fillWidth: true

		    			Rectangle {
		    				color: trackBackground
		    				Layout.preferredHeight: timelineHeaderHeight
		    				Layout.preferredWidth: trackHeaderWidth
		    			}

				    	Rectangle {
				    		id: frameTrack
		    				Layout.preferredHeight: timelineHeaderHeight
		    				Layout.fillWidth: true

							// border.color: "black"
							// border.width: 1
							color: trackBackground

				    		property real offset: hbar.position * myWidth

							XsTickWidget {
								id: tickWidget
								anchors.left: parent.left
								anchors.right: parent.right
								anchors.top: parent.top
								height: parent.height-4
								tickWidth: control.scaleX
								secondOffset: (frameTrack.offset  / control.scaleX) % rateFPSRole
								fractionOffset: frameTrack.offset % control.scaleX
								start: trimmedStartRole + (frameTrack.offset  / control.scaleX)
								duration: Math.ceil(width / control.scaleX)
								fps: rateFPSRole

								onFramePressed: viewport.playhead.frame = frame
								onFrameDragging: viewport.playhead.frame = frame
							}
						}
					}

                    Rectangle {
				        color: trackEdge
	    				Layout.fillHeight: true
	    				Layout.fillWidth: true

					    ListView {
					        id: list_view_video
					        anchors.fill: parent


		    				spacing: 1

					        model: video_items
					        clip: true
					        interactive: false
					        // header: stack_header
					        // headerPositioning: ListView.OverlayHeader
					        verticalLayoutDirection: ListView.BottomToTop

					        property real scaleX: control.scaleX
					        property real scaleY: control.scaleY
					        property real itemHeight: control.itemHeight
			    			property var timelineSelection: control.timelineSelection
							property var timelineFocusSelection: control.timelineFocusSelection

			    			property real cX: hbar.position * myWidth
					        property real parentWidth: control.parentWidth
					        property int playheadFrame: control.playheadFrame
			                property var timelineItem: control.timelineItem
			                property var hoveredItem: control.hoveredItem
			                property real trackHeaderWidth: control.trackHeaderWidth
							property string itemFlag: control.itemFlag
				            property var setTrackHeaderWidth: control.setTrackHeaderWidth

		        			footerPositioning: ListView.InlineFooter
					        footer: Rectangle {
								color: timelineBackground
								width: parent.width
								height: Math.max(0,list_view_video.parent.height - ((((itemHeight*control.scaleY)+1) * list_view_video.count)))
					        }

					        displaced: Transition {
					            NumberAnimation {
					                properties: "x,y"
					                duration: 100
					            }
					        }

					        ScrollBar.vertical: ScrollBar {
					            policy: list_view_video.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
					        }
					    }
					}
				}

				XsBorder {
					id: sizer
					Layout.minimumWidth: parent.width
					Layout.preferredHeight: handleSize
					Layout.minimumHeight: handleSize
					Layout.maximumHeight: handleSize
					color: trackEdge
					leftBorder: false
					rightBorder: false
					property real handleSize: 8

					MouseArea {
						id: ma
						anchors.fill: parent
			            hoverEnabled: true
            			acceptedButtons: Qt.LeftButton

            			cursorShape: Qt.SizeVerCursor

            			onPositionChanged: {
            				if(pressed) {
            					let ppos = mapToItem(splitView, 0, mouse.y)
            					topView.Layout.preferredHeight = ppos.y - (sizer.handleSize/2)
            					bottomView.Layout.preferredHeight = splitView.height - (ppos.y - (sizer.handleSize/2)) - sizer.handleSize
            				}
            			}
					}
				}

			    Item {
			    	id: bottomView
                    Layout.minimumWidth: parent.width
                    Layout.minimumHeight: itemHeight*control.scaleY
                    Layout.preferredHeight: parent.height*0.3
                    Rectangle {
				        anchors.fill: parent
				        color: trackEdge
					    ListView {
					        id: list_view_audio
					        spacing: 1

					        anchors.fill: parent

					        model: audio_items
					        clip: true
					        interactive: false

					        property real scaleX: control.scaleX
					        property real scaleY: control.scaleY
					        property real itemHeight: control.itemHeight
			    			property var timelineSelection: control.timelineSelection
							property var timelineFocusSelection: control.timelineFocusSelection
			    			property real cX: hbar.position * myWidth
					        property real parentWidth: control.parentWidth
					        property int playheadFrame: control.playheadFrame
			                property var timelineItem: control.timelineItem
			                property var hoveredItem: control.hoveredItem
			                property real trackHeaderWidth: control.trackHeaderWidth
				            property var setTrackHeaderWidth: control.setTrackHeaderWidth
							property string itemFlag: control.itemFlag

					        displaced: Transition {
					            NumberAnimation {
					                properties: "x,y"
					                duration: 100
					            }
					        }

		        			footerPositioning: ListView.InlineFooter
					        footer: Rectangle {
								color: timelineBackground
								width: parent.width
								height: Math.max(0,bottomView.height - ((((itemHeight*control.scaleY)+1) * list_view_audio.count)))
					        }

  					        ScrollBar.vertical: ScrollBar {
					            policy: list_view_audio.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
					        }
					    }
                    }
			    }
    		}
		}
	}
}