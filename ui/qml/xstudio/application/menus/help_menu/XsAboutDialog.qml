// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

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

<i>Lead Developers</i><BR>
Al Crate and Ted Waine<P>

<i>Developers</i><BR>
Olaf Razzoli<P>

<i>Contributing Developers</i><BR>
Remi Achard, Clement Jovet and Tomas Berzinskas<P>

<i>Project Management</i><BR>
Carly Russell-Swain<P>

<i>Thanks To</i><BR>
Ron Kurian Maniangat, Alex Tibbs, Sam Melamed, Hannah Costello,<BR> Jason Brown, Richard Jenns and Katherine Roberts<P>
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
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.minimumHeight: 35
            Layout.alignment: Qt.AlignRight|Qt.AlignBottom

            XsPrimaryButton{
                id: btnOK
                text: qsTr("Okay")
                width: XsStyleSheet.primaryButtonStdWidth*2
                height: XsStyleSheet.primaryButtonStdHeight
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                anchors.right: parent.right
                anchors.rightMargin: 10
                onClicked: {
                    close()
                }
            }
        }
    }
}
