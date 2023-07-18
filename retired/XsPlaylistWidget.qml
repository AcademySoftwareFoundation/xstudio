// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 1.4
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml.Models 2.12
import Qt.labs.qmlmodels 1.0
import QtQuick.Dialogs 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.uuid 1.0

Rectangle {
    id: playlist
    color: XsStyle.mainBackground
    height: playlist_control.implicitHeight + (playlist_control.expanded ? layout.contentItem.childrenRect.height + 5 : 0)
    property string text
    property var backend
	property string type
    property var parent_backend

	property bool highlighted: backend.selected
    property ListView listView
    property int listViewIndex

	function labelButtonReleased(mouse) {
        if (mouse.button == Qt.LeftButton) {
            if(mouse.modifiers & Qt.ControlModifier) {
                backend.selected = !backend.selected
            } else if(mouse.modifiers == Qt.NoModifier) {
                parent_backend.setSelection([backend.cuuid])
            }
        }
    }

    Connections {
        target: backend
        function onSelectedSubitemChanged() {
            if(!backend.selectedSubitem) {
                // select playlist as subgroup no longer valid.
                parent_backend.setSelection([backend.cuuid])

                // does current backend exist..
                if(!session.onScreenSource || !session.onScreenSource.hasBackend)  {
                    // selects a valid backend
                    session.switchOnScreenSource(null_uuid.asQuuid)
                }
            }
        }
   }
    // Timer {
    //     id: busy
    //     interval: 500;
    //     running: false;
    //     repeat: false
    //     onTriggered: playlist_control.busy.running = false
    // }

    property var internal_drag_drop_cursor: sessionWidget.internal_drag_drop_cursor_position
    property var internal_drag_drop_source_playlist_uuid: sessionWidget.internal_drag_drop_source_uuid
    property bool is_drag_drop_target: false

 	XsSessionBarWidget {
        busy.running: playlist_control.isLoadingMedia || playlist_control.isBusy
		id: playlist_control
        property bool has_drag: false
		text: playlist.text
		type_icon_source: "qrc:///feather_icons/list.svg"
		type_icon_color: XsStyle.highlightColor
        color: {
            has_drag ? XsStyle.highlightColor : (
                highlighted ?  XsStyle.menuBorderColor :  (hovered ? XsStyle.controlBackground : XsStyle.mainBackground)
            )
        }
        gradient: has_drag ? styleGradient.accent_gradient : null

        progress: app_window.events[backend.uuid.toString()] ? app_window.events[backend.uuid.toString()].progressPercentage : 0
        property bool is_drag_drop_target: false
        Rectangle {
            id: drag_target_highlight
            anchors.fill: parent
            color: "transparent"
            border.color: "white"
            border.width: 2
            visible: playlist_control.is_drag_drop_target
            x: 10000
        }
        decoratorModel: (
            app_window.sessionModel.tags[backend.uuid] ?
                (
                    app_window.sessionModel.tags[backend.uuid]["Decorator"] ? app_window.sessionModel.tags[backend.uuid]["Decorator"].tags : null
                )
                : null
            )
        // decoratorModel: {backend ? backend.decorators : null}
	   	anchors.top: parent.top
	   	anchors.left: parent.left
	   	anchors.right: parent.right
        tint: backend.flag
        child_count: playlist.backend.mediaModel.length
        issue_count: playlist.backend.offlineMediaCount

        property var isLoadingMedia: backend.loadingMedia
        property var isBusy: backend.isBusy

		icon_border_color: XsStyle.menuBorderColor
		onLabelPressed: labelButtonReleased(mouse)
		onPlusPressed: {parent_backend.setSelection([backend.cuuid]); plusMenu.toggleShow()}
		onMorePressed: {parent_backend.setSelection([backend.cuuid],false);moreMenu.toggleShow()}
		highlighted: playlist.highlighted
        expand_button_holder: true
		search_visible: false
        expanded: backend.expanded
        expand_visible: backend.itemModel.rowCount()
        onExpandClicked: backend.expanded = !backend.expanded

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
                            Future.promise(studio.loadSessionRequestFuture(drop.urls[i])).then(function(result){})
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

    property var internal_drag_drop_target

    function not_drag_drop_target() {
        if (internal_drag_drop_target) {
            internal_drag_drop_target.not_drag_drop_target()
            internal_drag_drop_target = null
        } else if (playlist_control.is_drag_drop_target) {
            playlist_control.is_drag_drop_target = false
        }
    }

    XsButtonDialog {
        id: move_or_copy
        text: "Add Media"
        width: 200
        buttonModel: ["Cancel", "Move", "Copy"]
        property var dropped_media_uuids: null
        property var dest_uuid: null
        property var src_uuid: null
        onSelected: {
            if(button_index == 2) {
                session.copyMedia(dest_uuid, dropped_media_uuids);
            } else if(button_index == 1) {
                session.moveMedia(dest_uuid, src_uuid, dropped_media_uuids);
            }
        }
    }

    function dropping_items_from_media_list(source_uuid, source_playlist, drag_drop_items, mousePos) {

        // clear drag-drop highlight status
        if (internal_drag_drop_target) {
            internal_drag_drop_target.not_drag_drop_target()
        }
        playlist_control.is_drag_drop_target = false

        var dropped_media_uuids = []
        for (var i in drag_drop_items) {
            dropped_media_uuids.push(drag_drop_items[i].uuid)
        }

        var local_pos = playlist.mapFromGlobal(mousePos)

        // drop target is this playlist
        if (local_pos.y > 0.0 && local_pos.y < playlist_control.height) {

            // n.b. source_uuid can come from a subset (of this playlist) - can't drag drop
            // from a subset of this playlist into this playlist
            if (source_uuid != backend.uuid && source_playlist.uuid != backend.uuid) {
                move_or_copy.dropped_media_uuids = dropped_media_uuids
                move_or_copy.dest_uuid = backend.uuid
                move_or_copy.src_uuid = source_uuid
                move_or_copy.open()
            }

        } else {

            // drop target is a child of this playlist
            var item = playlistMainContent.the_view.layout.itemAt(local_pos.x, local_pos.y-playlistMainContent.y)
            if (item && item.content) {
                if (item.content.backend.uuid != source_uuid) {
                    // check if media is from parent..
                    if(backend.uuid != source_uuid) {
                        move_or_copy.dropped_media_uuids = dropped_media_uuids
                        move_or_copy.dest_uuid = item.content.backend.uuid
                        move_or_copy.src_uuid = source_uuid
                        move_or_copy.open()
                    } else {
                        session.copyMedia(item.content.backend.uuid, dropped_media_uuids);
                    }
                }
            }
        }

    }

    function dragging_items_from_media_list(source_uuid, parent_playlist_uuid, mousePos) {

        var local_pos = playlist.mapFromGlobal(mousePos)

        if (local_pos.y > 0.0 && local_pos.y < playlist_control.height) {
            if (internal_drag_drop_target) {
                internal_drag_drop_target.not_drag_drop_target()
                internal_drag_drop_target = null
            }
            if (source_uuid != backend.uuid && backend.uuid != parent_playlist_uuid) {
                playlist_control.is_drag_drop_target = true
            }
            return
        } else if (playlist_control.is_drag_drop_target) {
            playlist_control.is_drag_drop_target = false
        }

        var item = playlistMainContent.the_view.layout.itemAt(local_pos.x, local_pos.y-playlistMainContent.y)
        if (item && item.content) {
            if (internal_drag_drop_target != item.content) {
                if (internal_drag_drop_target) {
                    internal_drag_drop_target.not_drag_drop_target()
                }
                if (item.content.backend.uuid != source_uuid)
                {
                    internal_drag_drop_target = item.content
                    internal_drag_drop_target.dragging_items_from_media_list(source_uuid, mousePos)
                } else {
                    internal_drag_drop_target = null
                }
            }
        } else if (internal_drag_drop_target) {
            internal_drag_drop_target.not_drag_drop_target()
            internal_drag_drop_target = null
        }
    }


    property alias playlistMainContent: playlistMainContent

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
            spacing: 0
            id: cl
            property alias the_view: the_view

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                id: the_view
                property alias layout: layout

                ListView {
                    id: layout
                    // implicitHeight: contentItem.childrenRect.height
                    // anchors.top: playlist_control.bottom
                    // anchors.left: parent.left
                    // anchors.right: parent.right
                    // anchors.bottom: parent.bottom
                    anchors.leftMargin: (playlist_control.icon_width  + 5)
                    visible: playlist_control.expanded
                    model: backend.itemModel
                    delegate: XsSessionChooser {}
                    focus: true
                    clip: true
                }
            }
        }
    }

    // XsBusyIndicator {
    //     Layout.fillHeight: true
    //     Layout.preferredWidth: height
    //     running: playlist_control.isLoadingMedia || playlist_control.isBusy
    // }

    // BusyIndicator {
    //     anchors.right: parent.right
    //     anchors.top: parent.top
    //     anchors.bottom: parent.bottom
    //     anchors.margins: 2
    //     running: playlist_control.isLoadingMedia || playlist_control.isBusy
    // }

	// ListView {
	// 	id: layout
	// 	implicitHeight: contentItem.childrenRect.height
	//    	anchors.top: playlist_control.bottom
	//    	anchors.left: parent.left
	//    	anchors.right: parent.right
	//    	anchors.bottom: parent.bottom
	//    	anchors.leftMargin: (playlist_control.icon_width  + 5)*2
 //        clip: true
	//    	visible: playlist_control.expanded
	//    	model: backend.itemModel
	//    	delegate: XsSessionChooser {}
	// 	focus: true
	// }


	XsStringRequestDialog {
		id: request_name
		okay_text: "Rename"
		text: "noname"
		onOkayed: backend.setName(text)
        centerOn: playlist_panel
        y: playlist_panel.mapToGlobal(0, 25).y
	}

    function onlySubTypes(obj) {
        if(obj.type == "Subset" || obj.type == "Timeline"  || obj.type == "ContactSheet" || obj.type == "ContainerDivider" ) {
            return true
        }
        return false
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
            onTriggered: {
                app_window.session.mergePlaylists(XsUtils.getSelectedCuuids(app_window.session))
            }
        }
        XsMenuItem {
            mytext: qsTr("Export As Session...")
            onTriggered: app_window.sessionFunction.saveSelectedSessionDialog()
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

        // XsMenuItem {
        //     mytext: qsTr("Copy Selected To")
        //     enabled: false
        //     onTriggered: {
        //         var selected_cuuids = XsUtils.getSelectedCuuids(app_window.session, true, onlySubTypes)
        //         parent_backend.copyContainers(selected_cuuids, backend.cuuid)
        //     }
        // }
        // XsMenuItem {
        //     mytext: qsTr("Move Selected To")
        //     enabled: false
        //     onTriggered: {
        //         var selected_cuuids = XsUtils.getSelectedCuuids(app_window.session, true, onlySubTypes)
        //         parent_backend.moveContainers(selected_cuuids, backend.cuuid, false)
        //     }
        // }
        // XsMenuItem {
        //     mytext: qsTr("Move (With Media) Selected To")
        //     enabled: false
        //     onTriggered: {
        //         var selected_cuuids = XsUtils.getSelectedCuuids(app_window.session, true, onlySubTypes)
        //         parent_backend.moveContainers(selected_cuuids, backend.cuuid, true)
        //     }
        // }

        XsMenuSeparator {
            visible: app_window.sessionModel.tags[backend.uuid] ? (app_window.sessionModel.tags[backend.uuid]["Menu"] ? app_window.sessionModel.tags[backend.uuid]["Menu"].tags.length : 0 ) : 0
        }

        // decorators
        Repeater {
            id: additionalMenus
            model: (
            app_window.sessionModel.tags[backend.uuid] ?
                (
                    app_window.sessionModel.tags[backend.uuid]["Menu"] ? app_window.sessionModel.tags[backend.uuid]["Menu"].tags : null
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

	QMLUuid {
		id: null_uuid
	}

    XsStringRequestDialog {
        id: request_new_group
        property var uuid
        okay_text: "New Group"
        text: "Untitled Group"
        onOkayed: {backend.createGroup(text, uuid); backend.expanded = true;}
        centerOn: playlist_panel
        y: playlist_panel.mapToGlobal(0, 25).y
    }

    XsNewPlaylistDividerDialog {
        id: request_new_divider
        playlist: backend
    }

    XsNewTimelineDialog {
        id: request_new_timeline
        playlist: backend
    }
    XsNewSubsetDialog {
        id: request_new_subset
        playlist: backend
    }
    XsNewContactSheetDialog {
        id: request_new_contact_sheet
        playlist: backend
    }

	XsMenu {
		id: plusMenu
		x: playlist_control.plus_button.x
	    y: playlist_control.plus_button.y

		fakeDisabled: true

        XsMenuItem {
        	mytext: qsTr("New Subset")
        	onTriggered: request_new_subset.open()
        }

        XsMenuItem {
            enabled: false
        	mytext: qsTr("New Contact Sheet")
        	onTriggered: request_new_contact_sheet.open()
        }

        XsMenuItem {
        	mytext: qsTr("New Timeline")
        	onTriggered: request_new_timeline.open()
            enabled: false
        }

        XsMenuItem {
        	mytext: qsTr("New Divider")
        	onTriggered: request_new_divider.open()
        }

        XsMenuSeparator {}

        XsMenuItem {
            mytext: qsTr("Add Media...")
            onTriggered: XsUtils.openDialogPlaylist("qrc:/dialogs/XsAddMediaDialog.qml").open()
        }
        XsMenuItem {
            mytext: qsTr("Add Media From Clipboard")
            onTriggered: app_window.session.add_media_from_clipboard(backend)
        }

        // XsMenuItem {
        // 	mytext: qsTr("New Group")
        // 	onTriggered: {
        //         backend.expanded = true
        // 		request_new_group.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : null_uuid.asQuuid
        //         request_new_group.open()
        // 	}
        // }
	}
}
