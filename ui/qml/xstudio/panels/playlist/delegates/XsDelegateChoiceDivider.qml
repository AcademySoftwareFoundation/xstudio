// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0

DelegateChoice {
    roleValue: "ContainerDivider"

    Rectangle {
    	id: control
	    color: XsStyle.mainBackground
		width: parent ? parent.width : 0
		height: (insertionFlag ? 20 : 0) + divider.implicitHeight
		property bool insertionFlag: false
		property bool dropFlag: false

		visible: !placeHolderRole

		function modelIndex() {
			return control.DelegateModel.model.srcModel.index(index, 0, control.DelegateModel.model.rootIndex)
		}

		function type() {
			return typeRole
		}

		XsSessionBarDivider {
			id: divider
		    text: nameRole == undefined ? "" : nameRole

			anchors.topMargin: (insertionFlag ? 20 : 0)
		   	anchors.top: parent.top
		   	anchors.left: parent.left
		   	anchors.right: parent.right

		    color: highlighted || dropFlag ? XsStyle.menuBorderColor : (hovered ? XsStyle.controlBackground : XsStyle.mainBackground)
		    tint: flagColourRole == undefined ? "" : flagColourRole

		    expand_button_holder: true

		    property bool highlighted: false


		    Connections {
		    	target: sessionSelectionModel
		    	function onSelectionChanged(selected, deselected) {
		    		divider.highlighted = sessionSelectionModel.isSelected(control.modelIndex())
		    	}
		    }

		    function labelButtonReleased(mouse) {
		        if (mouse.button == Qt.LeftButton) {
		            if(mouse.modifiers & Qt.ControlModifier) {
		   				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.Toggle)
		            } else if(mouse.modifiers == Qt.NoModifier) {
						sessionSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
		            }
		        }
		    }

		    onLabelPressed: labelButtonReleased(mouse)

		    onMorePressed: {
				sessionSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
		    	moreMenu.toggleShow()
		    }

			XsStringRequestDialog {
				id: request_name
				okay_text: "Rename"
				text: nameRole == undefined ? "" : nameRole
				onOkayed: nameRole = text
		        x: XsUtils.centerXInParent(panel, parent, width)
		        y: XsUtils.centerYInParent(panel, parent, height)
			}

			XsMenu {
				id: moreMenu
				x: divider.more_button.x
			    y: divider.more_button.y

				fakeDisabled: true

		        XsFlagMenu {
		            flag: flagColourRole == undefined ? "" : flagColourRole
		            onFlagHexChanged: flagColourRole = flagHex
		        }

		        XsMenuItem {
		        	mytext: qsTr("Rename")
		        	onTriggered: request_name.open()
		        }

		        XsMenuItem {
		        	mytext: qsTr("Duplicate")
		        	onTriggered: control.DelegateModel.model.model.duplicateRows(index, 1, control.DelegateModel.model.rootIndex)
		        }

		        XsMenuItem {
		        	mytext: qsTr("Remove")
		        	onTriggered: control.DelegateModel.model.model.removeRows(index, 1, control.DelegateModel.model.rootIndex)
		        }
			}
		}
    }
}
