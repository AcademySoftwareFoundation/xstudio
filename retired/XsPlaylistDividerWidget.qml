// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 1.4
import QtQuick 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml.Models 2.12
import Qt.labs.qmlmodels 1.0

import xStudio 1.0

XsSessionBarDivider {
	id: control
    property var backend
	property string type
    property var parent_backend
    property ListView listView
    property int listViewIndex

    color: highlighted ? XsStyle.menuBorderColor : (hovered ? XsStyle.controlBackground : XsStyle.mainBackground)
    tint: backend.flag

    expand_button_holder: true

    property bool highlighted: backend.selected

    function labelButtonReleased(mouse) {
        if (mouse.button == Qt.LeftButton) {
            if(mouse.modifiers & Qt.ControlModifier) {
                backend.selected = !backend.selected
            } else if(mouse.modifiers == Qt.NoModifier) {
                parent_backend.setSelection([backend.cuuid])
            }
        }
    }

    onLabelPressed: labelButtonReleased(mouse)
    onMorePressed: {parent_backend.setSelection([backend.cuuid]); moreMenu.toggleShow()}

	XsStringRequestDialog {
		id: request_name
		okay_text: "Rename"
		text: "noname"
		onOkayed: parent_backend.renameContainer(text, backend.cuuid)
        x: XsUtils.centerXInParent(panel, parent, width)
        y: XsUtils.centerYInParent(panel, parent, height)
	}

	XsMenu {
		id: moreMenu
		x: more_button.x
	    y: more_button.y

		fakeDisabled: true

        XsFlagMenu {
            flag: backend.flag
            onFlagHexChanged: {
                if(backend.flag !== flagHex)
                    parent_backend.reflagContainer(flagHex, backend.cuuid)
            }
        }

        XsMenuItem {
        	mytext: qsTr("Rename")
        	onTriggered: {
				request_name.text = backend.name
				request_name.open()
        	}
        }
        XsMenuItem {
        	mytext: qsTr("Duplicate")
        	onTriggered: {
                var nextItem = listView.itemAtIndex(listViewIndex+1)
                if(nextItem)
                    parent_backend.duplicateContainer(backend.cuuid, nextItem.contentItem.backend.cuuid)
                else
                    parent_backend.duplicateContainer(backend.cuuid)
        	}
        }
        XsMenuItem {
        	mytext: qsTr("Remove")
        	onTriggered: parent_backend.removeContainer(backend.cuuid)
        }
	}
}