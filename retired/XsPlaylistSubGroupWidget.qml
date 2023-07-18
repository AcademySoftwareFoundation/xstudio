// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 1.4
import QtQuick 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml.Models 2.12
import Qt.labs.qmlmodels 1.0

import xStudio 1.0

Rectangle {
    id: playlist_group
    color: XsStyle.mainBackground
    implicitHeight: playlist_control.implicitHeight + (playlist_control.expanded ? layout.implicitHeight : 0)
    property string text
    property var backend
	property string type
    property var parent_backend
    property ListView listView
    property int listViewIndex

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

    DelegateChooser {
        id: chooser
        role: "type"
        DelegateChoice {
            roleValue: "ContainerDivider";
            XsPlaylistDividerWidget {
                anchors.left: parent.left
                anchors.right: parent.right
                text: object.name
                type: object.type
                parent_backend: playlist_group.parent_backend
                backend: object
            }
        }
        DelegateChoice {
            roleValue: "Subset";
            XsPlaylistSubsetWidget {
                anchors.left: parent.left
                anchors.right: parent.right
                text: object.name
                type: object.type
                backend: object
            }
        }
        DelegateChoice {
            roleValue: "Timeline";
            XsPlaylistTimelineWidget {
                anchors.left: parent.left
                anchors.right: parent.right
                text: object.name
                type: object.type
                backend: object
            }
        }
        DelegateChoice {
            roleValue: "ContactSheet";
            XsPlaylistContactSheetWidget {
                anchors.left: parent.left
                anchors.right: parent.right
                text: object.name
                type: object.type
                backend: object
            }
        }    }

	XsSessionBarWidget {
		id: playlist_control
		text: playlist_group.text
		type_icon_source: "qrc:///feather_icons/server.svg"
		type_icon_color: XsStyle.highlightColor
		search_visible: false
        color: highlighted ? Qt.tint(XsStyle.menuBorderColor, Qt.rgba(tint.r,tint.g,tint.b, (tint.a ? 0.1 : 0.0))) : Qt.tint(XsStyle.mainBackground,Qt.rgba(tint.r,tint.g,tint.b, (tint.a ? 0.1 : 0.0)))
        tint: backend.flag

	   	anchors.top: parent.top
	   	anchors.left: parent.left
	   	anchors.right: parent.right
		icon_border_color: XsStyle.menuBorderColor
        onPlusPressed: plusMenu.toggleShow()
        onMorePressed: moreMenu.toggleShow()
		onLabelPressed: labelButtonReleased(mouse)
		highlighted: playlist_group.highlighted
        expanded: backend.expanded
        onExpandClicked: backend.expanded = !backend.expanded
        expand_visible: backend.itemModel.rowCount()
	}

	ListView {
		id: layout
		implicitHeight: contentItem.childrenRect.height
	   	anchors.top: playlist_control.bottom
	   	anchors.left: parent.left
	   	anchors.right: parent.right
	   	anchors.bottom: parent.bottom
	   	anchors.leftMargin: playlist_control.icon_width * 2
        clip: true
	   	visible: playlist_control.expanded
	   	model: backend.itemModel
	   	delegate: chooser
		focus: true
	}

    XsStringRequestDialog {
        id: request_name
        okay_text: "Rename"
        text: "noname"
        onOkayed: parent_backend.renameContainer(text, backend.cuuid)
        y: playlist_control.more_button.y
    }

    function rename() {
        request_name.text = backend.name
        request_name.open()
    }

    XsMenu {
        id: moreMenu
        x: playlist_control.more_button.x
        y: playlist_control.more_button.y

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
                rename()
            }
        }
        XsMenuItem {
            mytext: qsTr("Duplicate")
            onTriggered: {
                var nextItem =  ListView.view.itemAtIndex(index+1)
                if(nextItem)
                    parent_backend.duplicateContainer(backend.cuuid, nextItem.backend.cuuid)
                else
                    parent_backend.duplicateContainer(backend.cuuid)
            }
        }
        XsMenuItem {
            mytext: qsTr("Remove")
            onTriggered: {
                parent_backend.removeContainer(backend.cuuid)
            }
        }
    }

    XsStringRequestDialog {
        id: request_new_divider
        property var uuid
        property bool into
        okay_text: "New Divider"
        text: "Untitled Divider"
        onOkayed: {parent_backend.createDivider(text, uuid, into); backend.expanded = true;}
        y: playlist_control.plus_button.y
//        x: playlist_control.plus_button.x
    }

    XsStringRequestDialog {
        id: request_new_timeline
        property var uuid
        property bool into
        okay_text: "New Timeline"
        text: "Untitled Timeline"
        onOkayed: {parent_backend.createTimeline(text, uuid, into); backend.expanded = true;}
        y: playlist_control.plus_button.y
//        x: playlist_control.plus_button.x
    }

    XsStringRequestDialog {
        id: request_new_subset
        property var uuid
        property bool into
        okay_text: "New Subset"
        text: "Untitled Subset"
        onOkayed: {parent_backend.createSubset(text, uuid, into); backend.expanded = true;}
        y: playlist_control.plus_button.y
//        x: playlist_control.plus_button.x
    }

    XsStringRequestDialog {
        id: request_new_contact_sheet
        property var uuid
        property bool into
        okay_text: "New Contact Sheet"
        text: "Untitled Contact Sheet"
        onOkayed: {parent_backend.createContactSheet(text, uuid, into); backend.expanded = true;}
        y: playlist_control.plus_button.y
//        x: playlist_control.plus_button.x
    }

	XsMenu {
		id: plusMenu
		x: playlist_control.plus_button.x
	    y: playlist_control.plus_button.y

		fakeDisabled: true
        XsMenuItem {
            mytext: qsTr("New Divider")
            onTriggered: {
                backend.expanded = true
                request_new_divider.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : backend.cuuid
                request_new_divider.into = layout.currentItem ? false : true
                request_new_divider.open()
            }
        }

        XsMenuItem {
            mytext: qsTr("New Timeline")
            // enabled: layout.currentItem && layout.currentItem.type === "Playlist"
            onTriggered: {
                backend.expanded = true
                request_new_timeline.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : backend.cuuid
                request_new_timeline.into = layout.currentItem ? false : true
                request_new_timeline.open()
            }
        }

        XsMenuItem {
            mytext: qsTr("New Subset")
            // enabled: layout.currentItem && layout.currentItem.type === "Playlist"
            onTriggered: {
                backend.expanded = true
                request_new_subset.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : backend.cuuid
                request_new_subset.into = layout.currentItem ? false : true
                request_new_subset.open()
            }
        }

        XsMenuItem {
            mytext: qsTr("New Contact Sheet")
            enabled: false

            // enabled: layout.currentItem && layout.currentItem.type === "Playlist"
            onTriggered: {
                backend.expanded = true
                request_new_contact_sheet.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : backend.cuuid
                request_new_contact_sheet.into = layout.currentItem ? false : true
                request_new_contact_sheet.open()
            }
        }
	}
}



