// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 1.4
import QtQuick 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml.Models 2.12
import Qt.labs.qmlmodels 1.0
import QtQuick.Dialogs 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

Rectangle {
    id: playlist
    color: XsStyle.mainBackground
    implicitHeight: playlist_control.implicitHeight
    property string text
    property var backend
	property string type
    property var parent_backend
    property ListView listView
    property int listViewIndex

    property bool highlighted: backend.selected

    function not_drag_drop_target() {
        is_drag_drop_target = false
    }
    function dragging_items_from_media_list(source_uuid, mousePos) {
        is_drag_drop_target = true
    }

    function labelButtonReleased(mouse) {
        if (mouse.button == Qt.LeftButton) {
            if(mouse.modifiers & Qt.ControlModifier) {
                backend.selected = !backend.selected
            } else if(mouse.modifiers == Qt.NoModifier) {
                parent_backend.setSelection([backend.cuuid])
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

    Connections {
      target: backend
        function onMediaAdded(uuid) {
          busy.stop()
          playlist_control.busy.running = true
          busy.start()
      }
    }
	XsSessionBarWidget {
		id: playlist_control
        property bool has_drag: false

	   	anchors.top: parent.top
	   	anchors.left: parent.left
	   	anchors.right: parent.right
        tint: backend.flag

		text: playlist.text
		type_icon_source: "qrc:///feather_icons/grid.svg"
		type_icon_color: XsStyle.highlightColor
		search_visible: false
	   	expand_visible: false
        expand_button_holder: true
        color: {
            has_drag ? XsStyle.highlightColor : (
                highlighted ?  XsStyle.menuBorderColor :  (hovered ? XsStyle.controlBackground : XsStyle.mainBackground)
            )
        }
        gradient: has_drag ? styleGradient.accent_gradient : null
		icon_border_color: XsStyle.menuBorderColor
		highlighted: playlist.highlighted
        child_count: playlist.backend.mediaList.length

		onLabelPressed: labelButtonReleased(mouse)
        onPlusPressed: {parent_backend.setSelection([backend.cuuid]); plusMenu.toggleShow()}
        onMorePressed: {parent_backend.setSelection([backend.cuuid]); moreMenu.toggleShow()}

        onLabelDoubleClicked: {
            playerWidget.switchSource(backend)
        }

        DropArea {
            anchors.fill: playlist_control.drop_area

            onEntered: {
                if(drag.hasUrls || drag.hasText) {
                    drag.acceptProposedAction()
                    playlist_control.has_drag = true
                }
            }
            onExited: {
                playlist_control.has_drag = false
            }
            onDropped: {
               playlist_control.has_drag = false

                // if session then ignore everying else..
                if(drop.hasUrls) {
                    for(var i=0; i < drop.urls.length; i++) {
                        if(drop.urls[i].toLowerCase().endsWith('.xst')) {
                            session.loadUrl(drop.urls[i])
                            app_window.sessionFunction.newRecentPath(drop.urls[i])
                            return;
                        }
                    }
                }

                // prepare drop data
                let data = {}
                for(let i=0; i< drop.keys.length; i++){
                    data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
                }
                Future.promise(backend.handleDropFuture(data)).then(function(quuids){})
            }
        }
	}

    property bool is_drag_drop_target: false
    Rectangle {
        id: drag_target_highlight
        anchors.fill: parent
        color: "transparent"
        border.color: "white"
        border.width: 2
        visible: playlist.is_drag_drop_target
        z: 100
    }

    XsStringRequestDialog {
        id: request_name
        okay_text: "Rename"
        text: "noname"
        onOkayed: backend.setName(text)
        x: XsUtils.centerXInParent(panel, parent, width)
        y: XsUtils.centerYInParent(panel, parent, height)
    }

    XsButtonDialog {
        id: removeContainer
        text: "Remove"
        width: 300
        buttonModel: ["Cancel", "ContactSheet And Media", "ContactSheet"]
        onSelected: {
            if(button_index == 2) {
                parent_backend.removeContainer(backend.cuuid)
            } else if(button_index == 1) {
                parent_backend.removeContainer(backend.cuuid, true)
            }
        }
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

        XsMenuSeparator {}

        XsMenuItem {
            mytext: qsTr("Rename...")
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

        XsMenu {
            title: "Convert To"
            XsMenuItem {
                mytext: qsTr("Subset")
                onTriggered: {
                    parent_backend.convertToSubset(backend.cuuid)
                }
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
    XsMenu {
		id: plusMenu
		x: playlist_control.plus_button.x
	    y: playlist_control.plus_button.y

		fakeDisabled: true

        XsMenuItem {
            mytext: qsTr("Add Media...")
            onTriggered: XsUtils.openDialogPlaylist("qrc:/dialogs/XsAddMediaDialog.qml").open()
        }
        XsMenuItem {
            mytext: qsTr("Add Media From Clipboard")
            onTriggered: app_window.session.add_media_from_clipboard(backend)
        }
	}
}
