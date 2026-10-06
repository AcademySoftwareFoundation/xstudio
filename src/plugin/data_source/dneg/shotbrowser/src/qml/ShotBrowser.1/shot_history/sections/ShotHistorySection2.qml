// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import QtQuick.Controls.Basic

import xStudio 1.0
import ShotBrowser 1.0

Rectangle{
    color: "transparent"

    property bool descriptionClicked: false
    property bool descriptionHovered: descriptionEdit.hovered
    property int textHeightDiff: 0

    Component.onCompleted: {
        textHeightDiff = descriptionEdit.implicitHeight - descriptionEdit.height
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing

        ShotHistoryTextRow{ id: authorDiv
            Layout.fillWidth: true
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight

            textDiv.leftPadding: panelPadding
            textDiv.horizontalAlignment: Text.AlignLeft

            text: authorRole
            textColor: XsStyleSheet.primaryTextColor
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: itemSpacing
            ShotHistoryTextRow{ id: dateDiv
                Layout.fillWidth: true
                Layout.minimumHeight: XsStyleSheet.widgetStdHeight

                textDiv.leftPadding: panelPadding
                textDiv.horizontalAlignment: Text.AlignLeft

                property var dateFormatted: createdDateRole.toLocaleString().split(" ")

                text: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""
            }
            ShotHistoryTextRow{ id: frameRangeDiv
                Layout.fillWidth: true
                Layout.minimumHeight: XsStyleSheet.widgetStdHeight

                textDiv.leftPadding: panelPadding
                textDiv.horizontalAlignment: Text.AlignLeft

                text: frameRangeRole
            }
        }

        Rectangle{ id: descriptionDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: XsStyleSheet.widgetBgNormalColor

            TapHandler {
                onTapped: descriptionClicked = true
            }


            TextArea{ id: descriptionEdit
                anchors.fill: parent
                enabled: false
                readOnly: true

                font.pixelSize: XsStyleSheet.fontSize
                font.family: XsStyleSheet.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: XsStyleSheet.primaryTextColor

                text: descriptionRole
                padding: panelPadding
                wrapMode: TextEdit.Wrap
            }

            XsIcon{
                width: XsStyleSheet.secondaryButtonStdWidth
                height: XsStyleSheet.secondaryButtonStdWidth
                anchors.right: parent.right
                anchors.rightMargin: 7
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                imgOverlayColor: XsStyleSheet.accentColor
                source: "qrc:///shotbrowser_icons/arrow_right.svg"
                visible: descriptionEdit.implicitHeight > descriptionEdit.height
                rotation: 90
            }
        }
        // ShotHistoryTextRow{
        //     Layout.fillWidth: true
        //     Layout.minimumHeight: XsStyleSheet.widgetStdHeight

        //     textDiv.leftPadding: panelPadding
        //     textDiv.horizontalAlignment: Text.AlignLeft

        //     text: descriptionRole
        // }
    }
}