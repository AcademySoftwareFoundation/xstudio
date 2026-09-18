// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0
import xStudio 1.0

import "./delegates"

XsWindow {

    id: dialog
    minimumWidth: 580
    height: 300
    // minimumHeight: 250
    title: "Configure Hotkey Binding"
    property var hotkeyUUID

    property var liveKeys: []
    property var liveModifiers: []

    property var currSequence: hotkeyReference.key.concat(hotkeyReference.modifiers)

    property var liveSequence: liveKeys.concat(liveModifiers)    
    onLiveSequenceChanged: {
        hotkeysModel.testSequence = liveSequence
    }

    property var validSequence: liveKeys.length !== 0

    XsHotkeyReference {
        id: hotkeyReference
        uuid: dialog.hotkeyUUID ? dialog.hotkeyUUID : helpers.nullQUuid()
    }

    // clear the live key stroke suggestion when the user changes the hotkey they are configuring
    onHotkeyUUIDChanged: {
        liveModifiers = []
        liveKeys = []
    }

    // greab keyboard focus when the dialog is opened so the user can start entering a key stroke immediately
    onVisibleChanged: {
        if (visible) {
            requestActivate()
        }
    }

    XsPressedKeysMonitor {

        id: monitor
        property bool no_mod: false

        onPressedKeysChanged: {
            // we want the key stroke the user has entered to not update as
            // they lift keys. But when they have released all keys we
            // want them to start a new key stroke. 
            if (pressedKeys.length !== 0) {

                // we have to do this shennanegans or liveKeys becomes bound
                // to pressedKeys which is not what we want
                liveKeys = JSON.parse(JSON.stringify(pressedKeys))
                dialog.liveModifiers = JSON.parse(JSON.stringify(modifiers))
            }
        }

        onModifiersChanged: {
            if (no_mod || modifiers.length >= dialog.liveModifiers.length) {
                dialog.liveModifiers = JSON.parse(JSON.stringify(modifiers))
            }
            no_mod = modifiers.length === 0
        }
        
    }


    GridLayout {

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        columns: 2
        rowSpacing: 10
        columnSpacing: 20

        XsText {
            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            text: "Name"
        }

        XsText {
            Layout.alignment: Qt.AlignLeft|Qt.AlignVCenter
            text: hotkeyReference.hotkeyName
            font.bold: true
        }


        XsText {
            Layout.alignment: Qt.AlignRight|Qt.AlignTop
            text: "Description"
            verticalAlignment: Text.AlignTop
        }

        XsText {
            Layout.alignment: Qt.AlignLeft|Qt.AlignTop
            Layout.fillWidth: true
            text: hotkeyReference.description
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignTop
            font.italic: true
            wrapMode: Text.Wrap
        }

        XsText {
            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            text: "Current Key Sequence"
        }

        XsHotkeySequence {
            id: currentSeq
            Layout.alignment: Qt.AlignLeft|Qt.AlignVCenter
            regularKeys: hotkeyReference.key
            modifiers: hotkeyReference.modifiers
        }

        XsText {
            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            text: "Category"
        }

        XsText {
            Layout.alignment: Qt.AlignLeft|Qt.AlignVCenter
            text: hotkeyReference.context
            font.bold: true
        }

        Rectangle {
            height: 1
            Layout.fillWidth: true
            Layout.columnSpan: 2
            color: XsStyleSheet.menuBorderColor
        } 

        XsText {
            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            text: "New Key Sequence"
        }

        Item {

            Layout.fillWidth: true
            height: seq.height
            XsHotkeySequence {
                id: seq
                regularKeys: liveKeys
                modifiers: liveModifiers
                visible: liveKeys.length !== 0 || liveModifiers.length !== 0
            }

            XsText {
                anchors.verticalCenter: parent.verticalCenter
                text: "Press keys to suggest a new hotkey sequence"
                horizontalAlignment: Text.AlignLeft
                font.italic: true
                font.pixelSize: XsStyleSheet.fontSize*1.3
                visible: seq.visible === false
                opacity: 0.7
            }

        }

        Item {}

        RowLayout {
            Layout.alignment: Qt.AlignLeft|Qt.AlignVCenter
            spacing: 5

            XsSimpleButton {

                id: assignButton
                enabled: validSequence && !(JSON.stringify(currSequence) === JSON.stringify(liveSequence))
                text: qsTr("Assign")
                Layout.preferredWidth: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: {
                    hotkeysModel.assignTestSequence()
                    liveModifiers = []
                    liveKeys = []
                }
            }

            XsSimpleButton {
                text: qsTr("Reset to Default")
                onClicked: {
                    liveKeys = hotkeyReference.defaultKey
                    liveModifiers = hotkeyReference.defaultModifiers
                    hotkeysModel.assignTestSequence()
                    liveModifiers = []
                    liveKeys = []
                }
                enabled: !(JSON.stringify(hotkeyReference.defaultKey) === JSON.stringify(hotkeyReference.key) && JSON.stringify(hotkeyReference.defaultModifiers) === JSON.stringify(hotkeyReference.modifiers))
            }
            XsSimpleButton {
                text: qsTr("Unbind")
                onClicked: {
                    liveKeys = [""]
                    liveModifiers = []
                    hotkeysModel.assignTestSequence()
                    liveModifiers = []
                    liveKeys = []
                }
                enabled: !currentSeq.unbound
            }

        }


        XsIcon {
            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            source: hotkeysModel.testSequenceStatus === "OK" ? "qrc:/icons/check_circle.svg" : "qrc:/icons/warning.svg"
            imgOverlayColor: hotkeysModel.testSequenceStatus === "OK" ? "green" : "orange"
        }

        XsText {
            Layout.alignment: Qt.AlignLeft|Qt.AlignVCenter
            Layout.fillWidth: true
            text: hotkeysModel.testSequenceStatus
            font.weight: Font.Bold 
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.Wrap
        }

    }

    RowLayout {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 10
        spacing: 10
    
        XsSimpleButton {

            text: qsTr("Close")
            Layout.preferredWidth: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.close()
            }
        }
    }

    XsHotkeyArea {
        id: hotkeyArea
        anchors.fill: parent
        context: "configuration"        
    }

}