// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.0
import QtQuick.Controls 2.5

import xStudio 1.0

Rectangle {
    id: statusRect
    color: XsStyle.statusBarBackground
//    property alias title: myTitle.text
//    property alias text: myMessage.text
    property bool textVisible: true
    property color myTitleBG: "transparent"
    property string myTitleFG: ""

    property string lastTitle: ""
    property string lastMessage: ""

    height: XsStyle.statusBarHeight*opacity

    visible: opacity != 0

    Behavior on opacity {
        NumberAnimation { duration: XsStyle.presentationModeAnimLength }
    }

    Behavior on color {
        ColorAnimation {duration:XsStyle.colorDuration}
    }
    Rectangle {
        height: 1
        color: XsStyle.controlBackground
        anchors.left: parent.left
        anchors.right: parent.right
    }
    function restoreColors() {
        myTitleBG = 'transparent'
        myTitleFG = ''
        myMessage.color = XsStyle.hoverColor
        if (lastTitle) {
            myTitle.text = lastTitle
            myTitle.visible = true
        } else {
            myTitle.visible = false
        }
        if (lastMessage) {
            myMessage.text = lastMessage
            myMessage.visible = true
        } else {
            myMessage.visible = false
        }
    }
    Timer {
        id: restoreColorsTimer
        running: false
        repeat: false
        interval: 3500
        onTriggered: restoreColors()
    }
    Timer {
        id: clearMessageTimer
        interval: 2000
        onTriggered: clearMessage()
    }

    function _statusMessage(title, message, ttlbgcolor) {
        myDev.opacity = 0
        restoreColorsTimer.stop()
        lastTitle = myTitle.text
        lastMessage = myMessage.text
        myTitle.text = title
        myTitle.visible = true
        myTitleFG = ttlbgcolor //'#fff'
//        myTitleBG = ttlbgcolor

        myMessage.text = message
        myMessage.visible = true
        myMessage.color = '#fff'
        restoreColorsTimer.restart()
        clearMessageTimer.restart()
    }
    function normalMessage(message, title, isdev) {
        if(restoreColorsTimer.running) {
            lastTitle = title
            lastMessage = message
        } else {
            if(title) {
                myTitle.text = title
                myTitle.visible = true
            } else {
                myTitle.visible = false
            }
            myMessage.visible = true
            myMessage.text = message
            if (typeof(isdev) === "undefined") {
                isdev = false
            }
            if (isdev) {
                myDev.opacity = 1
            } else {
                myDev.opacity = 0
            }
        }
    }
    function clearMessage() {
        myDev.opacity = 0
        if(restoreColorsTimer.running) {
            lastTitle = ""
            lastMessage = ""
        } else {
            textVisible = false
            myTitle.text = ""
            myMessage.text = ""
        }
    }
    function infoMessage(message) {
        _statusMessage('INFO', message, XsStyle.highlightColor)
    }
    function warningMessage(message) {
        _statusMessage('WARNING', message, "#c80")
    }
    function errorMessage(message) {
        _statusMessage('ERROR', message, "#800")
    }
    Label {
        id: myDev
        visible: opacity === 1
        padding: 5
        opacity: 0
//        visible: false
        text: "Coming Soon"
        color: '#fff'
        background: Rectangle {
            color: XsStyle.indevColor
//            gradient:styleGradient.accent_gradient
            radius: 10
        }
        anchors.left: statusRect.left
        anchors.leftMargin: 3
        anchors.top: statusRect.top
        anchors.bottom: statusRect.bottom
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        verticalAlignment: Label.AlignVCenter
        font.pixelSize: XsStyle.statusBarFontSize
        font.family: XsStyle.fontFamily
        font.hintingPreference: Font.PreferNoHinting
    }

    Label {
        id: myTitle
        padding: 5
        visible: textVisible
        text: "Welcome to " + Qt.application.name + '.'
        color: myTitleFG?myTitleFG:XsStyle.highlightColor
        Behavior on color {
            ColorAnimation {duration:XsStyle.colorDuration}
        }
        anchors.left: myDev.visible?myDev.right:statusRect.left
//        anchors.leftMargin: myDev.visible?0:10
        anchors.top: statusRect.top
        anchors.bottom: statusRect.bottom
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        verticalAlignment: Label.AlignVCenter
        font.bold: true
        font.pixelSize: XsStyle.statusBarFontSize
        font.family: XsStyle.fontFamily
        font.hintingPreference: Font.PreferNoHinting
        background: Rectangle {
            id: myTitleRect
//            border {
//                width: 1
//                color: XsStyle.highlightColor
//            }

            color: "transparent"
            radius: 5
//            Behavior on color {
//                ColorAnimation {duration:XsStyle.colorDuration}
//            }
        }
    }
    Label {
        id: myMessage
        visible: textVisible
        anchors.top: statusRect.top
        anchors.bottom: statusRect.bottom
        anchors.left: myTitle.right
        anchors.leftMargin: 5
        color: XsStyle.hoverColor
        verticalAlignment: Label.AlignVCenter
        font.pixelSize: XsStyle.statusBarFontSize
        font.family: XsStyle.fontFamily
        font.hintingPreference: Font.PreferNoHinting

    }
}
