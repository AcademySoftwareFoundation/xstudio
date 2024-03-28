// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import Qt.labs.qmlmodels 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.uuid 1.0

DelegateChoice {
    roleValue: "Playlist"

	Rectangle {
	    id: control
	    color: XsStyle.mainBackground
	    height: (insertionFlag ? 20 : 0) + playlist_control.implicitHeight + (playlist_control.expanded ? layout.contentItem.childrenRect.height + 5 : 0)
		width: parent ? parent.width : 0

		// children  == media in sub container..
	    property int child_count: 0
		property bool highlighted: false
		property int offline_count: 0
		property bool insertionFlag: false
		property bool dropFlag: false
		property bool dropEndFlag: false

		property bool expanded: app_window.sessionExpandedModel.isSelected(modelIndex())
		property alias list_view: layout

	    Connections {
	    	target: sessionSelectionModel
	    	function onSelectionChanged(selected, deselected) {
	    		// console.log("onSelectionChanged", selected, deselected)
	    		control.highlighted = sessionSelectionModel.isSelected(modelIndex())
	    	}
	    }

	    Connections {
	    	target: sessionExpandedModel
	    	function onSelectionChanged(selected, deselected) {
	    		control.expanded = sessionExpandedModel.isSelected(modelIndex())
	    		playlist_control.expanded = control.expanded
	    	}
	    }

		function type() {
			return typeRole
		}

	    XsTimer {
	    	id: callback_delay_timer
	    }

	    function getOfflineCount() {
	    	let count = 0;
			let ind = modelIndex()
			let retry = false;

	    	if(ind.valid) {
	    		let mediaind = ind.model.index(0, 0, ind)
	    		if(mediaind.valid) {
	    			for(let i=0;i<mediaind.model.rowCount(mediaind); ++i) {
	    				let status = mediaind.model.get(mediaind.model.index(i, 0, mediaind), "mediaStatusRole");
	    				if(status == undefined) {
	    					retry = true;
	    				} else if(status != "Online"){
	    					count += 1
	    				}
	    			}
	    		} else {
					retry = true;
	    		}
	    	} else {
				retry = true;
	    	}

	    	if(retry) {
                callback_delay_timer.setTimeout(function() {offline_count = getOfflineCount()}, 1000);
	    	}

	    	return count;
	    }

		function modelIndex() {
			return control.DelegateModel.model.srcModel.index(index, 0, control.DelegateModel.model.rootIndex)
		}

	    Component.onCompleted: {
	    	let ind = modelIndex()

        	ind.model.fetchMore(ind)
			updateCounts()
			control.highlighted = sessionSelectionModel.isSelected(modelIndex())
	    }

	    Connections {
	    	target: control.DelegateModel.model.srcModel
	    	function onRowsRemoved(parent, first, last) {
	    		if(parent.parent == modelIndex())
	    	    	updateCounts()
	    	}

	    	function onRowsInserted(parent, first, last) {
	    		if(parent.parent == modelIndex())
	    	    	updateCounts()
	    	}
	    	function onMediaStatusChanged(parent) {
	    		if(parent == modelIndex()) {
	    			offline_count = getOfflineCount()
	    		}
	    	}
	    }

	    function updateCounts() {
	    	let ind = modelIndex()
		    child_count = ind.model.rowCount(ind.model.index(2, 0, ind))
		    offline_count = getOfflineCount()
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

		XsStringRequestDialog {
			id: request_name
			okay_text: "Rename"
			text: nameRole != undefined ?  nameRole : ""
			onOkayed: nameRole = text
	        centerOn: playlist_panel
	        y: playlist_panel.mapToGlobal(0, 25).y
		}

	 	XsSessionBarWidget {
			id: playlist_control

			text: nameRole != undefined ?  nameRole : ""
			anchors.topMargin: (insertionFlag ? 20 : 0)
		   	anchors.top: parent.top
		   	anchors.left: parent.left
		   	anchors.right: parent.right
	        tint: flagColourRole != undefined ? flagColourRole : ""

	        busy.running: busyRole != undefined ? busyRole : false

			type_icon_source: "qrc:///feather_icons/list.svg"
			type_icon_color: XsStyle.highlightColor
	        color: {
	            dropFlag? XsStyle.highlightColor : (
	                highlighted ?  XsStyle.menuBorderColor :  (hovered ? XsStyle.controlBackground : XsStyle.mainBackground)
	            )
	        }
	        gradient: dropFlag ? styleGradient.accent_gradient : null

	        progress: app_window.events[actorUuidRole != undefined ? actorUuidRole.toString() : ""] ? app_window.events[actorUuidRole != undefined ? actorUuidRole.toString() : ""].progressPercentage : 0

	        decoratorModel: (
	            app_window.sessionModel.tags[actorUuidRole] ?
	                (
	                    app_window.sessionModel.tags[actorUuidRole]["Decorator"] ? app_window.sessionModel.tags[actorUuidRole]["Decorator"].tags : null
	                )
	                : null
	            )

	        child_count: mediaCountRole == undefined ? 0 : mediaCountRole

	        issue_count: offline_count

			icon_border_color: XsStyle.menuBorderColor
			onLabelPressed: labelButtonReleased(mouse)
			onPlusPressed: {
				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
				plusMenu.toggleShow()
			}
			onMorePressed: {
				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.Select)
				moreMenu.toggleShow()
			}
			highlighted: control.highlighted
	        expand_button_holder: true
			search_visible: false
	        expanded: control.expanded
	        onExpandedChanged: {
	        	if(expanded)
		        	app_window.sessionExpandedModel.select(modelIndex(), ItemSelectionModel.Select)
				else
					app_window.sessionExpandedModel.select(modelIndex(), ItemSelectionModel.Deselect)
	        }
	        expand_visible: control.child_count

	        onLabelDoubleClicked: app_window.sessionFunction.setScreenSource(modelIndex())
		}

	    Item {
	        id: playlistMainContent
	        property alias the_view: cl.the_view

	        anchors {
	            left: parent.left
	            right: parent.right
	            top: playlist_control.bottom
	            bottom: parent.bottom
	        }

	        height: the_view.layout.contentItem.childrenRect.height


	        ColumnLayout {
	            anchors.fill: parent
	            anchors.leftMargin: playlist_control.icon_width  + 5
	            spacing: 0
	            id: cl
	            property alias the_view: the_view

	            ScrollView {
	                Layout.fillWidth: true
	                Layout.fillHeight: true
	                id: the_view
	                property alias layout: layout
	                horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff

	                ListView {
	                    id: layout

	                    property alias dropEndFlag: control.dropEndFlag
	                    // anchors.leftMargin: playlist_control.icon_width  + 5
	                    visible: playlist_control.expanded
	                    model: DelegateModel {
			                id: sess
			                property var srcModel: control.DelegateModel.model.srcModel
			                model: srcModel
			                rootIndex: srcModel.index(2,0,srcModel.index(index, 0, control.DelegateModel.model.rootIndex))
			                delegate: DelegateChooser {
			                    id: chooser
			                    role: "typeRole"

			                    XsDelegateChoiceDivider {}
			                    XsDelegateChoiceTimeline {}
			                    XsDelegateChoiceSubset {}
			                }
			            }

						Connections {
						    target: control.DelegateModel.model.srcModel
						    function onDataChanged(indexa,indexb,role) {
						    	if(modelIndex() == indexa && (!role.length || role.includes(sess.model.roleId("childrenRole")))) {
						        	control.DelegateModel.model.srcModel.fetchMore(modelIndex())
						        	control.DelegateModel.model.srcModel.fetchMore(sessionModel.index(0, 0, modelIndex()))
						        	control.DelegateModel.model.srcModel.fetchMore(sessionModel.index(2, 0, modelIndex()))
									updateCounts()
									sess.rootIndex = control.DelegateModel.model.srcModel.index(2,0,control.DelegateModel.model.srcModel.index(index, 0, control.DelegateModel.model.rootIndex))
						    	}
						    }

						    function onJsonChanged() {
								sess.rootIndex = control.DelegateModel.model.srcModel.index(2,0,control.DelegateModel.model.srcModel.index(index, 0, control.DelegateModel.model.rootIndex))
						    }
						}

	                    focus: true
	                    clip: true

				        // so we can place at bottom of list.
				        footer: Rectangle {
				            color: "transparent"
				            width: parent.width
				            height: dropEndFlag ? 20 : 0
				        }
	                }
	            }
	        }
	    }

		XsMenu {
			id: moreMenu
			x: playlist_control.more_button.x
		    y: playlist_control.more_button.y

			fakeDisabled: true

	        XsFlagMenu {
	            showChecked: false
	            onFlagSet: app_window.sessionFunction.flagSelected(hex)
	         }

	        XsMenuItem {
	            mytext: qsTr("Combine Selected")
	            onTriggered: app_window.sessionFunction.mergeSelected()
	        }
	        XsMenuItem {
	            mytext: qsTr("Export As Session...")
	            onTriggered: app_window.sessionFunction.saveSelectedSessionDialog()
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

	        XsMenuSeparator {
	            visible: app_window.sessionModel.tags[actorUuidRole] ? (app_window.sessionModel.tags[actorUuidRole]["Menu"] ? app_window.sessionModel.tags[actorUuidRole]["Menu"].tags.length : 0 ) : 0
	        }

	        Repeater {
	            id: additionalMenus
	            model: (
	            app_window.sessionModel.tags[actorUuidRole] ?
	                (
	                    app_window.sessionModel.tags[actorUuidRole]["Menu"] ? app_window.sessionModel.tags[actorUuidRole]["Menu"].tags : null
	                )
	                : null
	            )
	            XsDecoratorWidget {
	                widgetString: modelData.data
	            }
	            onItemAdded: moreMenu.insertMenu(moreMenu.count-2,item.widget_ui)
	            onItemRemoved: moreMenu.removeMenu(item.widget_ui)
	        }

	        XsMenuSeparator {}

	        XsMenuItem {
	            mytext: qsTr("Remove Selected")
	            onTriggered: app_window.sessionFunction.removeSelected()
	        }
		}

		XsMenu {
			id: plusMenu
			x: playlist_control.plus_button.x
		    y: playlist_control.plus_button.y

			fakeDisabled: true

	        XsMenuItem {
	        	mytext: qsTr("New Subset")
	        	onTriggered: sessionFunction.newSubset(
                    modelIndex().model.index(2,0, modelIndex()), null, playlist_panel
	            )
	        }

	        XsMenuItem {
	        	mytext: qsTr("New Contact Sheet")
	            enabled: false
	        }

	        XsMenuItem {
	        	mytext: qsTr("New Timeline")
	        	onTriggered: sessionFunction.newTimeline(
                    modelIndex().model.index(2,0, modelIndex()), null, playlist_panel
	            )
	        }

	        XsMenuItem {
	        	mytext: qsTr("New Divider")
	        	onTriggered: sessionFunction.newDivider(
	        		control.DelegateModel.model.model.index(2, 0, modelIndex()), null, playlist_panel
	        	)
	        }

	        XsMenuSeparator {}

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