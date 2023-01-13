// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5

import xStudio 1.0

Rectangle {

    id: tabButton

    anchors.top: parent ? parent.top : undefined
    anchors.bottom:  parent ? parent.bottom : undefined
    color: {
        if (isCurrentIndex) {
            return XsStyle.mainBackground
        } else {
            return Qt.darker(XsStyle.mainBackground,1.2)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "white"
        opacity: 0.2
        visible: mouseArea.containsMouse || mouseCloseArea.containsMouse
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: XsStyle.tabSeparatorColor
        width: XsStyle.tabSeparatorWidth
        visible: player_tabs.get(0).tab_id != tab_id
    }

    property bool isCurrentIndex: tab_id == playerWidget.currentTabId
    property bool canClose: player_tabs.count > 1
    property var tabBar

    Timer {
        // remove the item after the width animation.
        id: destroyTimer
        running: false
        repeat: false
        interval: 200
        onTriggered: {
            player_tabs.removeTab(tab_id)
        }
    }

    Label {
        id: tabButtonText
        anchors.fill: parent
        text: source ? source.fullName : ""
        font.pixelSize: XsStyle.sessionBarFontSize
        font.family: XsStyle.controlTitleFontFamily
        font.hintingPreference: Font.PreferNoHinting
        font.weight: Font.DemiBold
        color: isCurrentIndex?XsStyle.hoverColor:XsStyle.mainColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    XsMenu {

        title: { "Playlist"}
        // place this menu so it shows just next to the '+'
        // button

        id: playlistsMenu
        Instantiator {
            model: session.playlistNames
            delegate: XsMenuItem {
                mytext: modelData["text"]
                onTriggered: {
                    session.switchOnScreenSource(modelData["uuid"])
                    session.setSelection([modelData["uuid"]])
                }
            }
            onObjectAdded: playlistsMenu.insertItem(index, object)
            onObjectRemoved: playlistsMenu.removeItem(object)
        }
    }


    MouseArea {
        id: mouseArea
        cursorShape: Qt.PointingHandCursor
        anchors.fill: tabButtonText
        hoverEnabled: true

        onHoveredChanged: {
            if (mouseArea.containsMouse) {
                if (isCurrentIndex) {
                    status_bar.normalMessage("Current tab " + tabButtonText.text)
                } else {
                    status_bar.normalMessage("Switch to tab " + tabButtonText.text)
                }
            } else {
                status_bar.clearMessage()
            }
        }
        onClicked: {
            if (playerWidget.currentTabId == tab_id) {
                playlistsMenu.x = mouse.x
                playlistsMenu.y = mouse.y
                playlistsMenu.toggleShow()
            } else {
                playerWidget.currentTabId = tab_id
            }
        }
    }

    Label {
        id: closeX
        text: 'X'
        width: tabButton.height * .75
        height: width
        font.pixelSize: 8
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        color: mouseCloseArea.containsMouse?XsStyle.hoverColor:isCurrentIndex?XsStyle.hoverColor:XsStyle.mainColor
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        visible: canClose && (mouseArea.containsMouse || mouseCloseArea.containsMouse)
        background: Rectangle {
            anchors.fill: parent
            radius: closeX.height
            color: mouseCloseArea.containsMouse ? "#933" : "transparent"
        }
    }

    MouseArea {
        id: mouseCloseArea
        cursorShape: Qt.PointingHandCursor
        anchors.fill: closeX
        hoverEnabled: canClose
        onClicked: {
            if (canClose) {
                tabButton.width = 0
                playerWidget.switchToAlternativeTab(tab_id)
                destroyTimer.start()
            }
        }
        onHoveredChanged: {
            if (mouseCloseArea.containsMouse) {
                status_bar.normalMessage("Close tab " + tabButton.text)
            } else {
                status_bar.clearMessage()
            }
        }
    }
}