// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialogModal {
    id: dlg
    width: 600
    height: 560

    keepCentered: true
    centerOnOpen: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop|Qt.AlignLeft
            // color: "transparent"
            Image {
                id: xplayerIcon
                source: "qrc:/images/xstudio_logo_256_v1.svg"
                sourceSize.height: height
                sourceSize.width: width
                Layout.minimumHeight: 128
                Layout.minimumWidth: 128
                Layout.maximumHeight: 128
                Layout.maximumWidth: 128
                Layout.alignment: Qt.AlignTop|Qt.AlignHCenter

            }
            ColumnLayout {
                Layout.leftMargin: 10
                spacing: 10
                Text {
                    text: Qt.application.name + ' v' + Qt.application.version
                    font.pixelSize: 40
                    font.hintingPreference: Font.PreferNoHinting
                    font.family: XsStyle.fontFamily
                    color: XsStyle.hoverColor
                    font.bold: true
                }
                Rectangle {
                    height: 1
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    gradient: styleGradient.accent_gradient
                    color: XsStyle.highlightColor
                    Layout.alignment: Qt.AlignTop
                }



                Text {
                    text: `
<i>Architect</i><BR>
Chas Jarrett<P>

<i>Lead Engineers</i><BR>
Al Crate and Ted Waine<P>

<i>Developers</i><BR>
Ron Kurian Maniangat, Clement Jovet, Remi Achard and Tomas Berzinskas<P>

<i>UI / UX</i><BR>
Alex Tibbs<P>

<i>Project Management</i><BR>
Carly Russell-Swain, Hannah Costello and Sam Melamed<P>

<i>Thanks To</i><BR>
Jason Brown, Richard Jenns and Katherine Roberts<BR>
`

                    textFormat: Text.RichText
                    font.family: XsStyle.fontFamily
                    font.pixelSize: XsStyle.menuFontSize
                    font.hintingPreference: Font.PreferNoHinting
                    color: XsStyle.hoverColor
                }

                Text {
                    text:"Documentation is available here:<BR><style>a:link { color:'+XsStyle.highlightColor+'; }</style><a href='"+studio.userDocsUrl()+"'>Documentation</a>"
                    textFormat: Text.RichText
                    font.family: XsStyle.fontFamily
                    font.pixelSize: XsStyle.menuFontSize
                    font.hintingPreference: Font.PreferNoHinting
                    color: XsStyle.hoverColor
                    onLinkActivated: Qt.openUrlExternally(link)
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }

                Rectangle {
                    height: 1
                    Layout.fillWidth: true
                    gradient: styleGradient.accent_gradient
                    color: XsStyle.highlightColor
                }

                Text {
                    text:'<style>a:link { color:'+XsStyle.highlightColor+'; }</style>Overpass Font Copyright (c) 2016 by Red Hat, Inc. All rights reserved.<BR>This Font Software is licensed under the SIL Open Font License, Version 1.1.<BR><a href="http://scripts.sil.org/OFL">http://scripts.sil.org/OFL</a>'
                    textFormat: Text.RichText
                    font.family: XsStyle.fontFamily
                    font.pixelSize: 10
                    font.hintingPreference: Font.PreferNoHinting
                    // anchors.top: btmDivLine.bottom
                    // anchors.left: parent.left
                    color: XsStyle.mainColor
                    onLinkActivated: Qt.openUrlExternally(link)
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }

                Text {
                    text:'<style>a:link { color:'+XsStyle.highlightColor+'; }</style>Open Source Icon Library Feather <a href="https://feathericons.com">https://feathericons.com</a><BR>is licensed under the <a href="https://github.com/colebemis/feather/blob/master/LICENSE">MIT License</a>.'
                    textFormat: Text.RichText
                    font.family: XsStyle.fontFamily
                    font.pixelSize: 10
                    font.hintingPreference: Font.PreferNoHinting
                    // anchors.top: overPassText.bottom
                    // anchors.bottom: parent.bottom
                    // anchors.left: parent.left
                    color: XsStyle.mainColor
                    onLinkActivated: Qt.openUrlExternally(link)
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
            }
        }
        DialogButtonBox {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.minimumHeight: 35
            Layout.alignment: Qt.AlignRight|Qt.AlignBottom

            // horizontalPadding: 10
            // verticalPadding: 20
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            RoundButton {
                id: btnOK
                text: qsTr("OK")
                width: 100
                height: 36
                radius: 5
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                background: Rectangle {
                    radius: 5
    //                color: XsStyle.highlightColor//mouseArea.containsMouse?:XsStyle.controlBackground
                    color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                    gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                    anchors.fill: parent
                }
                contentItem: Text {
                    text: btnOK.text
                    color: XsStyle.hoverColor//:XsStyle.mainColor
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
