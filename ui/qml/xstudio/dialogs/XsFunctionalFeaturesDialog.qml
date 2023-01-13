// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialogModal {
    id: dlg

    height: 600
    width: 800
    onTop: false

    keepCentered: true
    centerOnOpen: true

    property string myHTMLTitle: "<h1>" + Qt.application.name + " v" + Qt.application.version + " Change Log</h1>"

    Item {
        id: wrapper

        anchors.fill: parent
        // anchors.topMargin: 10
        // anchors.rightMargin: 10
        // anchors.leftMargin: 10
        // anchors.bottomMargin: 10

        Image {
            id: xIcon
            source: "qrc:/images/xstudio_logo_256_v1.svg"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.topMargin: 10
            sourceSize.height: 128
            sourceSize.width: 128
            height: 128
            width: 128
        }

        TextEdit{
            id: titleText
            wrapMode: TextEdit.Wrap
            anchors.bottom: xIcon.bottom
            anchors.left: xIcon.right
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.bottomMargin: 0
            readOnly:true

            text: myHTMLTitle
            textFormat:TextArea.RichText
            font.pixelSize: 16
            font.family: XsStyle.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            color:"#fff"
        }

        Rectangle {
            height: 1
//                width: 100
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.bottom: flickArea.top
            anchors.bottomMargin: 10
            gradient: styleGradient.accent_gradient
            color: XsStyle.highlightColor
        }

        Flickable {
            id: flickArea
            anchors.top: xIcon.bottom
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: foot.top
            anchors.bottomMargin: 20
            anchors.leftMargin: 10
            anchors.rightMargin: 7
            height: 300
            implicitHeight: 300
            contentWidth: helpText.width
            contentHeight: helpText.height

            flickableDirection: Flickable.VerticalFlick
            clip: true

            TextEdit{
                id: helpText
                wrapMode: TextEdit.Wrap
                anchors.top: parent.top
                anchors.left: parent.left
                width: 700
                readOnly:true

                // text: myHTML
                text: {
                    let t = helpers.readFile(session.releaseDocsUrl())
                    const regex1 = /^[^@]+<div class="section" id="release-notes">/im;
                    t = t.replace(regex1, '');

                    const regex2 = /<footer>[^@]+$/im;
                    t = t.replace(regex2, '');

                    const regex3 = /<a[^>]+>[^<]+<\/a>/ig;
                    t = t.replace(regex3, '');
                    return t
                }
                textFormat: TextArea.RichText
                font.pixelSize: 12
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color:"#fff"
            }
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
            }
        }

        Rectangle {
            height: 1
//                width: 100
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.top: flickArea.bottom
            anchors.topMargin: 10
            gradient: styleGradient.accent_gradient
            color: XsStyle.highlightColor
        }

        Rectangle {

            id:foot
            height: 40
            color: "transparent"
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.left: parent.left
            anchors.topMargin: 20

            RoundButton {
                id: btnOK
                text: qsTr("OK")
                width: 80
                height: 32
                radius: 5
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 8

                background: Rectangle {
                    radius: parent.radius
                    color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                    gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                    anchors.fill: parent
                }
                contentItem: Text {
                    text: 'OK'
                    color: XsStyle.hoverColor
                    font.family: XsStyle.fontFamily
                    font.hintingPreference: Font.PreferNoHinting
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: mouseArea
                    hoverEnabled: true
                    anchors.fill: btnOK
                    cursorShape: Qt.PointingHandCursor
                    onClicked: close()
                }
            }
        }
    }
}

