// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import ShotBrowser 1.0

Rectangle{
    color: "transparent"

    property bool isHovered: notesEdit.hovered
    property int textHeightDiff: 0

    Component.onCompleted: {
        textHeightDiff = notesEdit.implicitHeight+(panelPadding*2) - notesEdit.height
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 1

        NotesHistoryTextRow{ id: subjectDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            text: versionNameRole
        }

        NotesHistoryTextRow{ id: titleDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            text: subjectRole
            textColor: XsStyleSheet.hintColor
        }

        Rectangle{ id: notesDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: XsStyleSheet.widgetBgNormalColor

            TextArea{ id: notesEdit
                anchors.fill: parent
                enabled: false
                readOnly: true

                font.pixelSize: XsStyleSheet.fontSize
                font.family: XsStyleSheet.fontFamily
                font.hintingPreference: Font.PreferNoHinting

                text: contentRole
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
                imgOverlayColor: palette.highlight
                source: "qrc:///shotbrowser_icons/arrow_right.svg"
                visible: notesEdit.implicitHeight > notesEdit.height
                rotation: 90
            }
        }
    }
}

