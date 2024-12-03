// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.0
import QuickFuture 1.0
import QuickPromise 1.0
import QtQml 2.14

import xStudio 1.0
import xstudio.qml.helpers 1.0

Rectangle {
	id: control

	width: ListView.view.width
	height: ListView.view.height

	// width / 2 so we can extend beyond the end of the timeline

	property real myWidth: ((duration.value ? duration.value : 0) * scaleX) + (width/2)
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

    property var draggingStarted: ListView.view.draggingStarted
    property var dragging: ListView.view.dragging
    property var draggingStopped: ListView.view.draggingStopped
    property var doubleTapped: ListView.view.doubleTapped
    property var tapped: ListView.view.tapped

    property string itemFlag: flagColourRole != "" ? flagColourRole : ListView.view.itemFlag

	opacity: enabledRole ? 1.0 : 0.2

	property bool isSelected: false
	property bool isHovered: hoveredItem == control

	property var timelineSelection: ListView.view.timelineSelection
	property var timelineFocusSelection: ListView.view.timelineFocusSelection
    property int playheadFrame: ListView.view.playheadFrame
    property var timelineItem: ListView.view.timelineItem
    property var hoveredItem: ListView.view.hoveredItem

    property alias markerModel: marker_model
    property alias cursor: cursor

    property var itemTypeRole: typeRole
    property alias list_view_video: list_view_video
    property alias list_view_audio: list_view_audio
    property alias list_view_middle: sizer

    property alias scrollbar: hbar


    property bool isSizerHovered: false
    property bool isSizerDragging: false

    function setSizerHovered(value) {
    	isSizerHovered = value
    }

    function setSizerDragging(value) {
    	isSizerDragging = value
    }

	function modelIndex() {
		return helpers.makePersistent(DelegateModel.model.modelIndex(index))
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

	function jumpToPosition(value) {
		if(hbar.size<1.0)
			hbar.position = Math.max(0, Math.min(value, 1.0 - hbar.size))

		return hbar.position
	}

	function currentPosition() {
		return hbar.position
	}

	// ListView.Center
	// ListView.Beginning
	// ListView.End
	// ListView.Visible
	// ListView.Contain
	// ListView.SnapPosition

	function jumpToFrame(frame, mode) {
		let new_position = 0.0
		let moved = false
		let first = ((frame - trimmedStartRole) * scaleX) / myWidth

		if(mode == ListView.Center) {
			new_position = first - (hbar.size / 2)
			moved = true
		} else if(mode == ListView.Beginning) {
			new_position = first
			moved = true
		} else if(mode == ListView.End) {
			new_position = (first - hbar.size) - (2 * (1.0 / (trimmedDurationRole * scaleX)))
			moved = true
		} else if(mode == ListView.Contain) {
			// calculate frame as position.
			if(first < hbar.position) {
				new_position = first - (hbar.size * 0.05) //(hbar.size / 2)
				moved = true
			} else if(first > (hbar.position + hbar.size)) {
				// reposition
				new_position = first - (hbar.size*0.95)//(hbar.size / 2)
				moved = true
			} else {
				// migrate to center of view..
				let cen = first - (hbar.size / 2)
				let inc = (hbar.size * 0.05)

				if(hbar.position > cen + inc) {
					new_position = hbar.position - inc
	 				moved = true
				} else if(hbar.position < cen - inc) {
					new_position = hbar.position + inc
	 				moved = true
				} else {
					new_position = cen
					moved = true
				}
			}
		} else if(mode == ListView.Visible) {
			// calculate frame as position.
			if(first < hbar.position) {
				new_position = first - (hbar.size * 0.99) //(hbar.size / 2)
				moved = true
			} else if(first > (hbar.position + hbar.size)) {
				// reposition
				new_position = first - (hbar.size*0.01)//(hbar.size / 2)
				moved = true
			}
		}

		if(moved) {
			new_position = Math.max(0, Math.min(new_position, 1.0 - hbar.size))
			if(hbar.position != new_position) {
				hbar.position = new_position
				return true
			}
		}

		return false
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

    XsSortFilterModel {
        id: video_items
        srcModel: theSessionData
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

			DelegateChoice {
			    roleValue: "Video Track"
		        XsDelegateVideoTrack {}
		    }
	    }

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
        srcModel: theSessionData
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

			DelegateChoice {
			    roleValue: "Audio Track"
		        XsDelegateAudioTrack {}
		    }
	    }

        filterAcceptsItem: function(item) {
        	return item.typeRole == "Audio Track"
        }

        lessThan: function(left, right) {
        	return left.index < right.index
        }
        // onUpdated: console.log("audio_items updated")
    }

	Connections {
	    target: theSessionData

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
		id: cursor
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
		visible: cursorX >= 0 && cursorX < frameTrack.width
	}

	// snapline
	XsTimelineCursor {
		z:10
		anchors.left: parent.left
		anchors.leftMargin: trackHeaderWidth
		anchors.right: parent.right
		anchors.top: parent.top
		height: control.height

		color: "black"
		showBobble: false

		tickWidth: tickWidget.tickWidth
		secondOffset: tickWidget.secondOffset
		fractionOffset: tickWidget.fractionOffset
		start: tickWidget.start
		duration: tickWidget.duration
		fps: tickWidget.fps
		position: snapLine
		visible: snapLine != -1
	}

	// cutline
	XsTimelineCursor {
		z:10
		anchors.left: parent.left
		anchors.leftMargin: trackHeaderWidth
		anchors.right: parent.right
		anchors.top: parent.top
		height: control.height

		color: "red"
		showBobble: false

		tickWidth: tickWidget.tickWidth
		secondOffset: tickWidget.secondOffset
		fractionOffset: tickWidget.fractionOffset
		start: tickWidget.start
		duration: tickWidget.duration
		fps: tickWidget.fps
		position: cutLine
		visible: editMode == "Cut" && cutLine != -1
	}

    XsScrollBar {
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

		            HoverHandler {
		                cursorShape: Qt.PointingHandCursor
		            }

		            TapHandler {
		                acceptedButtons: Qt.LeftButton
		                onTapped: {
		                	timeMode = timeMode == "timecode" ? "frames" : "timecode"
		                }
		            }

		            XsText {
		                XsModelPropertyMap {
		                    id: timelineDetail
		                    index: theTimeline.timelineModel.rootIndex
		                    property real fps: index.valid ? values.rateFPSRole : 24.0
		                    property int start: index.valid ? values.availableStartRole : 0
		                }

		                XsTimeCode {
		                    id: ttc
		                    dropFrame: false
		                    frameRate: timelineDetail.fps
		                    totalFrames: playheadActive ? currentFrame : lastFrame

		                    property int currentFrame: timelinePlayhead.logicalFrame + timelineDetail.start
		                    property int lastFrame: 0
	                        property bool playheadActive: timelinePlayhead.pinnedSourceMode ? currentPlayhead.uuid == timelinePlayhead.uuid : false

	                        onPlayheadActiveChanged: {
						        if (!playheadActive) lastFrame = currentFrame
    						}

		                }

		                id: timestampDiv
		                // Layout.preferredWidth: btnWidth*3
		                // Layout.preferredHeight: parent.height
		                // text: timelinePlayhead.currentSourceTimecode ? timelinePlayhead.currentSourceTimecode : "00:00:00:00"

		                anchors.fill: parent
		                text: timeMode == "timecode" ? (ttc.timeCode ? ttc.timeCode : "00:00:00:00") : String(ttc.totalFrames+1).padStart(6, '0')
		                font.pixelSize: XsStyleSheet.fontSize + 6
		                font.weight: Font.Bold
		                font.family: XsStyleSheet.fixedWidthFontFamily
		                horizontalAlignment: Text.AlignHCenter
		            }
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

						onFramePressed: {
							timelinePlayhead.logicalFrame = frame
						}
						onFrameDragging:{
							timelinePlayhead.logicalFrame = frame
						}
					}

					XsMarkerModel {
						id: marker_model
						markerData: markersRole
						onMarkerDataChanged: markersRole = markerData
					}

					XsMarkers {
						id: markersWidget
						visible: !hideMarkers
						anchors.left: parent.left
						anchors.right: parent.right
						anchors.bottom: parent.bottom
						height: 8
						z:1
						tickWidth: control.scaleX
						fractionOffset: frameTrack.offset % control.scaleX
						start: trimmedStartRole + (frameTrack.offset  / control.scaleX)
						duration: Math.ceil(width / control.scaleX)

						model: marker_model
					}

		    		Rectangle {
		    			color: XsStyleSheet.accentColor
		    			opacity: 0.3
		    			visible: timelinePlayhead && timelinePlayhead.enableLoopRange
		    			anchors.fill: parent
		    			property int start: (timelinePlayhead.loopStartFrame - (frameTrack.offset  / control.scaleX)) * control.scaleX
		    			property int end: (timelinePlayhead.loopEndFrame - (frameTrack.offset  / control.scaleX)) * control.scaleX
						anchors.leftMargin: Math.max(0, start)
						anchors.rightMargin: parent.width - (end-start) - start
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
			        // or we trash stuff outside
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

	    			property real cY: vbar.position * ((((itemHeight*control.scaleY)+1) * list_view_video.count))
	    			property real cX: hbar.position * myWidth
			        property real parentWidth: control.parentWidth
			        property int playheadFrame: control.playheadFrame
	                property var timelineItem: control.timelineItem
	                property var hoveredItem: control.hoveredItem
	                property real trackHeaderWidth: control.trackHeaderWidth
					property string itemFlag: control.itemFlag
		            property var setTrackHeaderWidth: control.setTrackHeaderWidth
		            property real footerHeight: Math.max(itemHeight,list_view_video.parent.height - ((((itemHeight*control.scaleY)+1) * list_view_video.count)))

				    property bool isSizerHovered: control.isSizerHovered
				    property bool isSizerDragging: control.isSizerDragging
				    property var setSizerHovered: control.setSizerHovered
				    property var setSizerDragging: control.setSizerDragging

                    property var draggingStarted: control.draggingStarted
                    property var dragging: control.dragging
		            property var draggingStopped: control.draggingStopped
				    property var doubleTapped: control.doubleTapped
				    property var tapped: control.tapped

        			footerPositioning: ListView.InlineFooter
			        footer: Rectangle {
        	            XsPrimaryButton{ id: addVideoBtn
        	            	imgSrc: "qrc:/icons/add.svg"
        	            	text: "Add Video Track"
        	            	showBoth: false
            				width: XsStyleSheet.primaryButtonStdWidth
            				height: XsStyleSheet.primaryButtonStdHeight
        	            	anchors.left: parent.left
							anchors.leftMargin: 1
        	            	anchors.bottom: parent.bottom
							anchors.bottomMargin: 2
        	            	onClicked: {
	        	            	isHovered = false
        	            		theTimeline.addTrack("Video Track", false)
        	            	}
        	            }

						color: timelineBackground
						width: parent.width
						height: list_view_video.footerHeight
			        }

			        displaced: Transition {
			            NumberAnimation {
			                properties: "x,y"
			                duration: 100
			            }
			        }

			        ScrollBar.vertical: XsScrollBar {
			        	id: vbar
			            policy: list_view_video.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
			        }
			    }
			}
		}

		Rectangle {
			id: sizer

	        color: ma.pressed ? palette.highlight : ma.containsMouse  ? XsStyleSheet.secondaryTextColor : "transparent"

			border.color: trackEdge
			Layout.minimumWidth: parent.width
			Layout.preferredHeight: handleSize
			Layout.minimumHeight: handleSize
			Layout.maximumHeight: handleSize
			property real handleSize: 8

			MouseArea {
				id: ma
				anchors.fill: parent
	            hoverEnabled: true
    			acceptedButtons: Qt.LeftButton
		        preventStealing: true

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
	    			property real cY: abar.position * ((((itemHeight*control.scaleY)+1) * list_view_audio.count))
			        property real parentWidth: control.parentWidth
			        property int playheadFrame: control.playheadFrame
	                property var timelineItem: control.timelineItem
	                property var hoveredItem: control.hoveredItem
	                property real trackHeaderWidth: control.trackHeaderWidth
		            property var setTrackHeaderWidth: control.setTrackHeaderWidth
					property string itemFlag: control.itemFlag

				    property bool isSizerHovered: control.isSizerHovered
				    property bool isSizerDragging: control.isSizerDragging
				    property var setSizerHovered: control.setSizerHovered
				    property var setSizerDragging: control.setSizerDragging

                    property var draggingStarted: control.draggingStarted
                    property var dragging: control.dragging
		            property var draggingStopped: control.draggingStopped
				    property var doubleTapped: control.doubleTapped
				    property var tapped: control.tapped

			        displaced: Transition {
			            NumberAnimation {
			                properties: "x,y"
			                duration: 100
			            }
			        }

        			footerPositioning: ListView.InlineFooter
			        footer: Rectangle {
        	            XsPrimaryButton{ id: addAudioBtn
        	            	imgSrc: "qrc:/icons/add.svg"
        	            	text: "Add Audio Track"
        	            	showBoth: false
            				width: XsStyleSheet.primaryButtonStdWidth
            				height: XsStyleSheet.primaryButtonStdHeight
        	            	anchors.left: parent.left
							anchors.leftMargin: 1
        	            	anchors.top: parent.top
        	            	anchors.topMargin: 2
        	            	onClicked: {
								isHovered = false
								theTimeline.addTrack("Audio Track", false)
        	            	}
        	            }

						color: timelineBackground
						width: parent.width
						height: Math.max(itemHeight,bottomView.height - ((((itemHeight*control.scaleY)+1) * list_view_audio.count)))
			        }

				    ScrollBar.vertical: XsScrollBar {
				      	id: abar
			            policy: list_view_audio.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
			        }
			    }
            }
	    }
	}
}
