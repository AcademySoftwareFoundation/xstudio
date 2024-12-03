// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

XsWindow {
    id: dlg
    width: 600
    height: 560

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop|Qt.AlignLeft

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
                    font.family: XsStyleSheet.fontFamily
                    color: XsStyleSheet.primaryTextColor
                    font.bold: true
                }
                Rectangle {
                    height: 1
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    // gradient: styleGradient.accent_gradient
                    color: XsStyleSheet.accentColor
                    Layout.alignment: Qt.AlignTop
                }
                Text {
                    text: `
<i>Architect</i><BR>
Chas Jarrett<P>

<i>Lead Engineers</i><BR>
Al Crate and Ted Waine<P>

<i>UI Developer</i><BR>
Ron Kurian Maniangat<P>

<i>Developers</i><BR>
Remi Achard, Clement Jovet and Tomas Berzinskas<P>

<i>UI / UX</i><BR>
Alex Tibbs<P>

<i>Project Management</i><BR>
Carly Russell-Swain, Hannah Costello and Sam Melamed<P>

<i>Thanks To</i><BR>
Jason Brown, Richard Jenns and Katherine Roberts<BR>
`

                    textFormat: Text.RichText
                    font.family: XsStyleSheet.fontFamily
                    font.pixelSize: XsStyleSheet.fontSize
                    font.hintingPreference: Font.PreferNoHinting
                    color: XsStyleSheet.primaryTextColor
                }

                Rectangle {
                    height: 1
                    Layout.fillWidth: true
                    // gradient: styleGradient.accent_gradient
                    color: XsStyleSheet.accentColor
                }

                Text {
                    text:'<style>a:link { color:'+XsStyleSheet.accentColor+'; }</style>Overpass Font Copyright (c) 2016 by Red Hat, Inc. All rights reserved.<BR>This Font Software is licensed under the SIL Open Font License, Version 1.1.<BR><a href="http://scripts.sil.org/OFL">http://scripts.sil.org/OFL</a>'
                    textFormat: Text.RichText
                    font.family: XsStyleSheet.fontFamily
                    font.pixelSize: 10
                    font.hintingPreference: Font.PreferNoHinting
                    // anchors.top: btmDivLine.bottom
                    // anchors.left: parent.left
                    // color: XsStyleSheet.mainColor
                    onLinkActivated: Qt.openUrlExternally(link)
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }

                Text {
                    text:'<style>a:link { color:'+XsStyleSheet.accentColor+'; }</style>Open Source Icon Library Feather <a href="https://feathericons.com">https://feathericons.com</a><BR>is licensed under the <a href="https://github.com/colebemis/feather/blob/master/LICENSE">MIT License</a>.'
                    textFormat: Text.RichText
                    font.family: XsStyleSheet.fontFamily
                    font.pixelSize: 10
                    font.hintingPreference: Font.PreferNoHinting
                    // anchors.top: overPassText.bottom
                    // anchors.bottom: parent.bottom
                    // anchors.left: parent.left
                    color: XsStyleSheet.primaryTextColor
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

            XsPrimaryButton{
                id: btnOK
                text: qsTr("Okay")
                width: XsStyleSheet.primaryButtonStdWidth*2
                height: XsStyleSheet.primaryButtonStdHeight
                onClicked: {
                    close()
                }
            }
        }

    }
}
