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
        color: XsStyle.controlColor
        font.pixelSize: XsStyle.sessionBarFontSize
        font.family: XsStyle.controlTitleFontFamily
        font.hintingPreference: Font.PreferNoHinting
        font.weight: Font.DemiBold
        verticalAlignment: Qt.AlignVCenter
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

    XsMenu {

        id: plusMenu

        fakeDisabled: true

        XsMenuItem {
            mytext: qsTr("New Playlist")
            onTriggered: sessionFunction.newPlaylist(
                app_window.sessionModel.index(0, 0), null
            )
        }

        XsMenuItem {
            mytext: qsTr("New Divider")
            onTriggered: sessionFunction.newDivider(
                app_window.sessionModel.index(0, 0), null, playlist_panel
            )
        }

        XsMenuItem {
            mytext: qsTr("New Dated Divider")
            onTriggered: sessionFunction.newDivider(
                app_window.sessionModel.index(0, 0), XsUtils.yyyymmdd("-"), playlist_panel
            )
        }
    }

    XsMenu {
        id: moreMenu

        fakeDisabled: true

        XsMenuItem {
            mytext: qsTr("New Session")

            onTriggered: {
                app_window.sessionFunction.saveBeforeNewSession()
            }
        }
        XsMenuItem {
            mytext: qsTr("Open Session...")
            onTriggered: {
                app_window.sessionFunction.saveBeforeOpen()
            }
        }
        XsMenuItem {
            mytext: qsTr("Save Session")
            onTriggered: app_window.sessionFunction.saveSession()
        }
        XsMenuItem {
            mytext: qsTr("Save Session As...")
            onTriggered: app_window.sessionFunction.saveSessionAs()
        }
        XsMenuItem {
            mytext: qsTr("Import Session...")
            onTriggered: app_window.sessionFunction.importSession()
        }
        XsMenuItem {
            mytext: qsTr("Copy Session Link")
            onTriggered: app_window.sessionFunction.copySessionLink()
        }
    }
}
