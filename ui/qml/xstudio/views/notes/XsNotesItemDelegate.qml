// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts



import xStudio 1.0
import xstudio.qml.models 1.0

Item{

    property bool isActive: false

    property real listItemSpacing: 1
    property real borderWidth: 1

    property real itemSpacing: listItemSpacing
    property real itemHeight: 20

    property color highlightColor: palette.highlight
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor

    property bool isHovered: mArea.containsMouse ||
        sec1.isHovered ||
        sec2.isHovered ||
        sec3.isHovered

    Rectangle{

        id: frame
        width: parent.width
        height: parent.height - listItemSpacing
        color: "transparent"
        border.color: isHovered? highlightColor : bgColorNormal
        border.width: borderWidth

        MouseArea{ id: mArea
            anchors.fill: parent
            hoverEnabled: true
        }

        RowLayout{
            anchors.fill: parent
            anchors.margins: borderWidth
            spacing: 1

            XsNoteSection1{ id: sec1
                Layout.fillHeight: true
                Layout.preferredWidth: 150
            }
            XsNoteSection2{ id: sec2
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                Layout.fillHeight: true
            }
            XsNoteSection3{ id: sec3
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 20
                Layout.preferredWidth: visibleWidth
                Layout.maximumWidth: visibleWidth
            }
        }

        XsSecondaryButton{ id: closeBtn
            width: 40
            height: itemHeight - itemSpacing/2
            anchors.top: parent.top
            anchors.topMargin: 1.45
            anchors.rightMargin: 1
            anchors.right: parent.right
            imgSrc: "qrc:/icons/close.svg"
            forcedBgColorNormal: panelColor

            onClicked: {
                deleteNote(uuidRole)
            }
        }


    }

}