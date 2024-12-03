// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.14
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import QuickFuture 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

XsWindow{

    title: "Publish "+notesType+" Notes Feedback"
    property bool isPlaylistNotes: true
    property string notesType: isPlaylistNotes? "Playlist": "Selected Media"
    property string message: ""
    property string detail: ""

    property real itemHeight: btnHeight
    property real itemSpacing: 1

    width: 400
    height: 620
    minimumWidth: 400
    // maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    palette.base: XsStyleSheet.panelTitleBarColor

    function parseFeedback(data) {
        let obj = JSON.parse(data)
        message = obj["data"]["status"]

        if(obj["succeed_title"].length) {
            detail =  "\nSucceeded:\n"
            for(let i=0; i< obj["succeed_title"].length; i++) {
                detail += obj["succeed_title"][i]+"\n"
            }
        }

        if(obj["failed_title"].length) {
            detail =  "\nFailed:\n"
            for(let i=0; i< obj["failed_title"].length; i++) {
                detail += obj["failed_title"][i]+"\n"
                detail += obj["failed"][i]["errors"]+"\n"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight/2
        }

        Item{ id: msgDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            XsText{
                width: parent.width - itemSpacing*2
                height: itemHeight
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                color: XsStyleSheet.accentColor //errorColor
                wrapMode: Text.Wrap
                text: message
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight/2
        }

        Item{ id: detailDiv
            Layout.fillWidth: true
            Layout.fillHeight: true

            XsText{
                horizontalAlignment: Text.AlignLeft
                width: parent.width - itemSpacing*2
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                color: XsStyleSheet.accentColor //errorColor
                wrapMode: Text.Wrap
                text: detail
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight*2
        }

        Item{ id: buttonsDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            RowLayout{
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10


                XsPrimaryButton{
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width/2
                    Layout.fillHeight: true
                    text: "Okay"
                    onClicked: {
                        close()
                    }
                }
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight
        }
    }
}

