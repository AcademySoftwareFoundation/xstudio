// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.15
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0

Rectangle {
	id: control
   	property string text
   	property bool search_visible: true
   	property bool plus_visible: true
   	property bool more_visible: true
   	property bool divider_visible: false
   	property bool expand_visible: true
   	property bool expand_button_holder: false
	property bool more_button_holder: true
   	property bool autohide_buttons: true
   	property int icon_width: label.height
   	property bool expanded: false
   	property alias plus_button: plus_button
   	property alias more_button: more_button
   	property alias search_button: search_button
   	property alias drop_area: control_mouse
   	property alias busy: busy
   	property alias font: label.font
   	property var decoratorModel: null
   	property var progress: 0

   	property color tint: "#00000000"
   	property int child_count: -1
	property int issue_count: 0
   	property bool highlighted: false
   	property bool hovered: false

	signal plusPressed(variant mouse)
	signal plusClicked(variant mouse)
	signal plusReleased(variant mouse)
	signal searchClicked(variant mouse)
	signal searchReleased(variant mouse)
	signal morePressed(variant mouse)
	signal moreClicked(variant mouse)
	signal moreReleased(variant mouse)
	signal expandPressed(variant mouse)
	signal expandClicked(variant mouse)
	signal expandReleased(variant mouse)
	signal labelDoubleClicked(variant mouse)
	signal labelPressed(variant mouse)
	signal labelReleased(variant mouse)
	signal labelClicked(variant mouse)

	onExpandClicked: expanded = !expanded
   	color: highlighted ? XsStyle.menuBorderColor : XsStyle.controlBackground

   	implicitWidth: row.implicitWidth
   	implicitHeight: row.implicitHeight + 4
   	Layout.fillWidth: true

	property string type_icon_source
	property string type_icon_color: XsStyle.controlColor
	property string icon_border_color: XsStyle.menuBorderColor

    Rectangle {
        id: progress_bar
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; }
        // visible: opacity
        opacity: progress && progress !== 100 ? 0.8 : 0.0
        property int widthDest: (parent.width / 100.0) * progress
        width: progress_bar.widthDest
        // Behavior on width { SmoothedAnimation { velocity: 200 } }
        Behavior on width { SmoothedAnimation { velocity: 500 } }
        Behavior on opacity {NumberAnimation { duration: !progress ? 0 : 1000 } }

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { id: gradient1; color: "transparent"; position: 0.0 }
            GradientStop { id: gradient2; color: XsStyle.highlightColor; position: 1.0 }
        }
    }


    Rectangle {
    	id: flag_bar
    	width: 4
    	color: tint

        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
	    	bottomMargin: 1
	    	topMargin: 1
        }
	}

	Rectangle
	{
		id: base_line_move
	    color: XsStyle.controlBackground
	    height: 1
	    visible: !divider_visible
	    anchors
	    {
	        left: parent.left
	        right: control_mouse.left
	        bottom: parent.bottom
	    }
	}

    MouseArea {
    	id: control_mouse
        anchors {
            left: flag_bar.right
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
    	hoverEnabled: true
    	onHoveredChanged: control.hovered = containsMouse
    	onPressed: {control.labelPressed(mouse)} //mouse.accepted = false must be true, or clicked and released will not work..
    	onClicked: {control.labelClicked(mouse); mouse.accepted = false}
    	onDoubleClicked: { control.labelDoubleClicked(mouse); mouse.accepted = false }
    	onReleased: { control.labelReleased(mouse); mouse.accepted = false }
	    propagateComposedEvents: true

	    RowLayout{
	    	id: row
	    	anchors.left: parent.left
	    	anchors.right: parent.right
			anchors.verticalCenter: parent.verticalCenter

		    Image {
		    	id: expand_button
		    	Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
		    	opacity: expand_visible ? 1 : 0
		    	visible: expand_visible || expand_button_holder
		        width: icon_width
		        height: width+4
				source: "qrc:///feather_icons/chevron-right.svg"
		        sourceSize.height: height
		        sourceSize.width:  width
		        smooth: true
			    transformOrigin: Item.Center
	    		rotation: expanded ? 90 : 0
			    MouseArea {
			        anchors.fill: parent
		        	onClicked: control.expandClicked(mouse)
		        	onReleased: control.expandReleased(mouse)
			    }

			    layer {
			        enabled: true
			        effect: ColorOverlay {
			            color: XsStyle.controlColor
			        }
			    }
		    }

		    Image {
		    	Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
		        id: type_icon
		        width: icon_width+4
		        height: width
		        source: type_icon_source
		        sourceSize.height: height
		        sourceSize.width:  width
		        smooth: true
		        visible: type_icon_source
			    layer {
			        enabled: true
			        effect: ColorOverlay {
			            color: type_icon_color
			        }
			    }
		    }

		    Rectangle {
		    	visible: divider_visible
		        implicitHeight: 1
	   	        Layout.fillWidth: true
		        color: highlighted ? XsStyle.mainBackground : XsStyle.menuBorderColor
		    }

		    // decorators
	        Repeater {
	        	id: decorator
	            model: decoratorModel
	            XsDecoratorWidget {
			    	Layout.topMargin: 2
			    	Layout.bottomMargin: 2
					Layout.leftMargin: 3
	                Layout.preferredWidth: icon_width
	                Layout.preferredHeight: icon_width
			    	Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
	                widgetString: modelData.data
	            }
	        }

		    Label {
		    	Layout.topMargin: 4
		    	Layout.bottomMargin: 4
		    	Layout.leftMargin: 3
		    	id: label
		    	elide: Text.ElideMiddle
		        text: control.text
		        Layout.fillWidth: divider_visible ? false : true
		        color: XsStyle.controlColor
		        font.pixelSize: XsStyle.sessionBarFontSize
		        font.family: XsStyle.controlTitleFontFamily
		        font.hintingPreference: Font.PreferNoHinting
		        font.weight: Font.DemiBold
		    }

		    Rectangle {
		    	visible: divider_visible
		        implicitHeight: 1
	   	        Layout.fillWidth: true
		        color: highlighted ? XsStyle.mainBackground : XsStyle.menuBorderColor
		    }

			XsBusyIndicator {
				id: busy
		        Layout.fillHeight: true
		        Layout.preferredWidth: height
			    running: false
			}

		    Rectangle {
		    	id: plus_button
		    	opacity: control.hovered || !control.autohide_buttons ? 1.0: 0.0
		    	visible: plus_visible && !(!control.hovered && busy.running)
		    	Layout.topMargin: 0
		    	Layout.bottomMargin: 0
		    	Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
		        gradient: plus_ma.containsMouse ? styleGradient.accent_gradient : null
		        color: plus_ma.containsMouse ? XsStyle.highlightColor : XsStyle.mainBackground
		        width: icon_width+4
		        height: width
				border.color : icon_border_color
				border.width : 1
			    Image {
			        id: add
			        anchors.fill: parent
			        anchors.margins: 1
					source: "qrc:///feather_icons/plus.svg"
			        sourceSize.height: height
			        sourceSize.width:  width
			        smooth: true
				    layer {
				        enabled: true
				        effect: ColorOverlay {
				            color: XsStyle.controlColor
				        }
				    }
			    }

	    	    // Behavior on opacity {
	  	      //       NumberAnimation {duration: control_mouse.containsMouse ? 200 : 0}
	         //    }

	  			MouseArea {
			    	id: plus_ma
			        cursorShape: Qt.PointingHandCursor
		           	hoverEnabled: true
			        anchors.fill: parent
				    onClicked: control.plusClicked(mouse)
				    onPressed: control.plusPressed(mouse)
				    onReleased: control.plusReleased(mouse)
			    }
			}
		    Rectangle {
		    	id: search_button
		    	opacity: control.hovered  || !control.autohide_buttons ? 1.0: 0.0
		    	visible: search_visible && !(!control.hovered && busy.running)
		    	Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
		    	Layout.topMargin: 0
		    	Layout.bottomMargin: 0
		        gradient: search_ma.containsMouse ? styleGradient.accent_gradient : null
		        color: search_ma.containsMouse ? XsStyle.highlightColor : XsStyle.mainBackground
		        width: icon_width+4
		        height: width
				border.color : icon_border_color
				border.width : 1
			    Image {
			        id: search
			        anchors.fill: parent
			        anchors.margins: 1
					source: "qrc:///feather_icons/search.svg"
			        sourceSize.height: height
			        sourceSize.width:  width
			        smooth: true
				    layer {
				        enabled: true
				        effect: ColorOverlay {
				            color: XsStyle.controlColor
				        }
				    }
			    }
			    MouseArea {
			    	id: search_ma
			        cursorShape: Qt.PointingHandCursor
		           	hoverEnabled: true
			        anchors.fill: parent
		        	onClicked: control.searchClicked(mouse)
		        	onReleased: control.searchReleased(mouse)
					onPressed: control.searchPressed(mouse)

			    }
	    	    // Behavior on opacity {
	  	      //       NumberAnimation {duration: control_mouse.containsMouse ? 200 : 0}
	         //    }
			}
		    Rectangle {
		    	id: more_button
		    	opacity: control.hovered || !control.autohide_buttons ? 1.0: 0.0
		    	visible: (more_button_holder && !(!control.hovered && busy.running))|| (more_visible && (control.hovered || !control.autohide_buttons) && !(!control.hovered && busy.running))
		    	Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
		    	Layout.topMargin: 0
		    	Layout.bottomMargin: 0
		    	Layout.rightMargin: child_count !== -1 ? 0 : 12
		        gradient: more_ma.containsMouse ? styleGradient.accent_gradient : null
		        color: more_ma.containsMouse ? XsStyle.highlightColor : XsStyle.mainBackground

		        width: icon_width+4
		        height: width
				border.color : icon_border_color
				border.width : 1
			    Image {
			        id: more
			        anchors.margins: 1
			        anchors.fill: parent
					source: "qrc:///feather_icons/more-horizontal.svg"
			        sourceSize.height: height
			        sourceSize.width:  width
			        smooth: true
				    layer {
				        enabled: true
				        effect: ColorOverlay {
				            color: XsStyle.controlColor
				        }
				    }
				}
			    MouseArea {
			    	id: more_ma
			        cursorShape: Qt.PointingHandCursor
		           	hoverEnabled: true
			        anchors.fill: parent
		        	onClicked: control.moreClicked(mouse)
		        	onReleased: control.moreReleased(mouse)
					onPressed: control.morePressed(mouse)
			    }
	    	    // Behavior on opacity {
	  	      //       NumberAnimation {duration: control_mouse.containsMouse ? 200 : 0}
	         //    }
		    }

		    Label {
				Layout.leftMargin: 2
		    	Layout.topMargin: 2
		    	Layout.bottomMargin: 2
		    	Layout.rightMargin: 12

		    	id: child_label
		        text: issue_count ? child_count + " ("+issue_count+" issue" + (issue_count > 1 ? "s)" : ")") : child_count
		        Layout.fillWidth: false
		        color: XsStyle.controlColor
		        font.pixelSize: XsStyle.sessionBarFontSize
		        font.family: XsStyle.controlTitleFontFamily
		        font.hintingPreference: Font.PreferNoHinting
		        font.weight: Font.DemiBold
		        visible: child_count !== -1
		    }
		}

		Rectangle
		{
			id: base_line
		    color: XsStyle.controlBackground
		    height: 1
		    visible: !divider_visible
		    anchors
		    {
		        left: parent.left
		        right: parent.right
		        bottom: parent.bottom
		    }
		}
	}
}