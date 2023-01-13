// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12

import xstudio.qml.uuid 1.0
import xstudio.qml.clipboard 1.0

import xStudio 1.0

Rectangle {

    id: playlist_control
    color: "transparent"
    anchors.fill: parent
    property bool expanded: true
    property alias null_uuid: null_uuid

    QMLUuid {
        id: null_uuid
    }

    Label {

        anchors.left: parent.left
        anchors.leftMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        text: "Playlists"
        // text: session.name + (session.modified ? " *" : "")
        color: XsStyle.controlColor
        font.pixelSize: XsStyle.sessionBarFontSize
        font.family: XsStyle.controlTitleFontFamily
        font.hintingPreference: Font.PreferNoHinting
        font.weight: Font.DemiBold
        verticalAlignment: Qt.AlignVCenter

        Rectangle {

            anchors.fill: parent
            visible: mouseArea.containsMouse
            color: "white"
            opacity: 0.1
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                request_name.text = session.name
                request_name.open()
            }
        }

        XsStringRequestDialog {
            id: request_name
            okay_text: "Rename"
            text: "noname"
            onOkayed: session.name = text
            x: XsUtils.centerXInParent(playlist_control, parent, width)
            y: XsUtils.centerYInParent(playlist_control, parent, height)
        }
    }

    ListModel {

        id: buttons

        ListElement {
            button_source: "qrc:///feather_icons/plus.svg"
            action_name: "plus"
        }

        // ListElement {
        //     button_source: "qrc:///feather_icons/search.svg"
        //     action_name: "search"
        // }

        ListElement {
            button_source: "qrc:///feather_icons/more-horizontal.svg"
            action_name: "more"
        }

    }

    RowLayout {

        id: buttons_layout
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 6
        //width: buttons_layout.length*

        Repeater {

            id: repeater
            model: buttons

            Rectangle {

                id: plus_button

                gradient: plus_ma.containsMouse ? styleGradient.accent_gradient : null
                color: plus_ma.containsMouse ? XsStyle.highlightColor : XsStyle.mainBackground
                width: 17
                height: width
                border.color : XsStyle.menuBorderColor
                border.width : 1

                Image {
                    id: add
                    anchors.fill: parent
                    anchors.margins: 1
                    source: button_source
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
                    id: plus_ma
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: playlist_control.buttonClicked(action_name, mapToItem(playlist_control, width/2, height/2))
                    onPressed: playlist_control.buttonClicked(action_name, mapToItem(playlist_control, width/2, height/2))
                    onReleased: playlist_control.buttonClicked(action_name, mapToItem(playlist_control, width/2, height/2))
                }
            }

        }
    }

    function buttonClicked(action_name, pos) {
        if (action_name == "plus") {
            plusMenu.x = pos.x
            plusMenu.y = pos.y
            plusMenu.open()
        } else if (action_name == "more") {
            moreMenu.x = pos.x
            moreMenu.y = pos.y
            moreMenu.open()
        }
    }

    XsNewPlaylistDialog {
        id: request_new_playlist
    }

    XsStringRequestDialog {
        id: request_new_group
        property var uuid
        okay_text: "New Group"
        text: "Untitled Group"
        onOkayed: {session.createGroup(text, uuid);session.expanded = true;}
    }

    XsMenu {

        id: plusMenu

        fakeDisabled: true

        XsMenuItem {
            mytext: qsTr("New Playlist")
            onTriggered: {
                // request_new_playlist.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : null_uuid.asQuuid
                request_new_playlist.open()
            }
        }

        // XsMenuItem {
        //     mytext: qsTr("New Group")
        //     onTriggered:{
        //         request_new_group.uuid = layout.currentItem ? layout.currentItem.backend.cuuid : null_uuid.asQuuid
        //         request_new_group.open()
        //     }
        // }

        XsMenuItem {
            mytext: qsTr("New Divider")
            onTriggered: XsUtils.openDialog("qrc:/dialogs/XsNewSessionDividerDialog.qml",playlist_panel).open()
        }
        XsMenuItem {
            mytext: qsTr("New Dated Divider")
            onTriggered: {
                let d = XsUtils.openDialog("qrc:/dialogs/XsNewSessionDividerDialog.qml",playlist_panel)
                d.dated = true
                d.open()
            }
        }
    }

    XsMenu {
        id: moreMenu

        fakeDisabled: true

        XsMenuItem {
            mytext: qsTr("New Session")

            onTriggered: {
                app_window.session.save_before_new_session()
            }
        }
        XsMenuItem {
            mytext: qsTr("Open Session...")
            onTriggered: {
                app_window.session.save_before_open()
            }
        }
        XsMenuItem {
            mytext: qsTr("Save Session")
            onTriggered: app_window.session.save_session()
        }
        XsMenuItem {
            mytext: qsTr("Save Session As...")
            onTriggered: app_window.session.save_session_as()
        }
        XsMenuItem {
            mytext: qsTr("Import Session...")
            onTriggered: app_window.session.import_session()
        }
        XsMenuItem {
            mytext: qsTr("Copy Session Link")
            onTriggered: app_window.session.copy_session_link()
        }
    }
}
