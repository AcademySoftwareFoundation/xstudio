// SPDX-License-Identifier: Apache-2.0
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.clipboard 1.0

DelegateChoice {
    roleValue: "Subset"

	Rectangle {
	    id: control
	    color: XsStyle.mainBackground
	    implicitHeight: playlist_control.implicitHeight
		width: parent ? parent.width : 0
	    property bool highlighted: false
		property bool insertionFlag: false
		property bool dropFlag: false

		height: (insertionFlag ? 20 : 0) + playlist_control.implicitHeight

		function type() {
			return typeRole
		}

	    Connections {
	    	target: sessionSelectionModel
	    	function onSelectionChanged(selected, deselected) {
	    		control.highlighted = sessionSelectionModel.isSelected(modelIndex())
	    	}
	    }

	    Component.onCompleted: {
	    	// grab children
        	control.DelegateModel.model.srcModel.fetchMore(modelIndex())
	    }

	    function modelIndex() {
	    	return control.DelegateModel.model.srcModel.index(index, 0, control.DelegateModel.model.rootIndex)
	    }

	    Connections {
	    	target: control.DelegateModel.model.srcModel
	    	function onMediaAdded(index) {
	    		if(index == modelIndex()) {
	    			busy.stop()
		            playlist_control.busy.running = true
			        busy.start()
	    		}
	    	}
	    }

	    function labelButtonReleased(mouse) {
	        if (mouse.button == Qt.LeftButton) {
	            if(mouse.modifiers & Qt.ControlModifier) {
	   				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.Toggle)
	            } else if(mouse.modifiers == Qt.NoModifier) {
					sessionSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
					sessionSelectionModel.setCurrentIndex(modelIndex(),ItemSelectionModel.setCurrentIndex)
	            }
	        }
	    }

	    Timer {
	        id: busy
	        interval: 500;
	        running: false;
	        repeat: false
	        onTriggered: playlist_control.busy.running = false
	    }

		XsSessionBarWidget {
			id: playlist_control
		    text: nameRole != undefined ? nameRole : ""

			anchors.topMargin: (insertionFlag ? 20 : 0)
		   	anchors.top: parent.top
		   	anchors.left: parent.left
		   	anchors.right: parent.right
	        tint: flagColourRole != undefined ? flagColourRole : ""

			type_icon_source: "qrc:///feather_icons/trello.svg"
			type_icon_color: XsStyle.highlightColor
			search_visible: false
		   	expand_visible: false
	        expand_button_holder: true
	        color: {
	            dropFlag? XsStyle.highlightColor : (
	                highlighted ?  XsStyle.menuBorderColor :  (hovered ? XsStyle.controlBackground : XsStyle.mainBackground)
	            )
	        }
	        gradient: dropFlag ? styleGradient.accent_gradient : null
			icon_border_color: XsStyle.menuBorderColor
			highlighted: control.highlighted

	        child_count: mediaCountRole != undefined ? mediaCountRole : 0

			onLabelPressed: labelButtonReleased(mouse)
	        onPlusPressed: {
				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
	        	plusMenu.toggleShow()
	        }
	        onMorePressed: {
				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
	        	moreMenu.toggleShow()
	       	}

	        onLabelDoubleClicked: app_window.sessionFunction.setScreenSource(modelIndex())
		}

	    XsStringRequestDialog {
	        id: request_name
	        okay_text: "Rename"
	        text: "noname"
	        onOkayed: nameRole = text
	        x: XsUtils.centerXInParent(panel, parent, width)
	        y: XsUtils.centerYInParent(panel, parent, height)
	    }

	    XsButtonDialog {
	        id: removeContainer
	        text: "Remove"
	        width: 300
	        buttonModel: ["Cancel", "Subset And Media", "Subset"]
	        onSelected: {
	            if(button_index == 2) {
	            	control.DelegateModel.model.model.removeRows(index, 1, false, control.DelegateModel.model.rootIndex)
	            } else if(button_index == 1) {
	            	control.DelegateModel.model.model.removeRows(index, 1, true, control.DelegateModel.model.rootIndex)
	            }
	        }
	    }

	    XsMenu {
	        id: moreMenu
	        x: playlist_control.more_button.x
	        y: playlist_control.more_button.y

	        fakeDisabled: true

	        XsFlagMenu {
	            flag:  flagColourRole != undefined ? flagColourRole : ""
	            onFlagHexChanged: flagColourRole = flagHex
	        }

	        XsMenuSeparator {}

	        XsMenuItem {
	            mytext: qsTr("Rename...")
	            onTriggered: {
	                request_name.text = nameRole
	                request_name.open()
	            }
	        }
	        XsMenuItem {
	            mytext: qsTr("Duplicate")
	        	onTriggered: control.DelegateModel.model.model.duplicateRows(index, 1, control.DelegateModel.model.rootIndex)
	        }

	        XsMenu {
	            title: "Convert To"
	            XsMenuItem {
	                enabled: false
	                mytext: qsTr("Contact Sheet")
	            }
	            XsMenuItem {
	                mytext: qsTr("Timeline")
	                enabled: false
	            }
	        }

	        XsMenuSeparator {}

	        XsMenuItem {
	            mytext: qsTr("Remove")
	            onTriggered: {
	                removeContainer.open()
	            }
	        }
	    }

	    Clipboard {
	      id: clipboard
	    }

		XsMenu {
			id: plusMenu
			x: playlist_control.plus_button.x
		    y: playlist_control.plus_button.y

			fakeDisabled: true

	        XsMenuItem {
	            mytext: qsTr("Add Media...")
	            onTriggered: sessionFunction.addMedia(modelIndex())
	        }

	        XsMenuItem {
	            mytext: qsTr("Add Media From Clipboard")
	            onTriggered: sessionFunction.addMediaFromClipboard(modelIndex())
	        }
		}
	}
}