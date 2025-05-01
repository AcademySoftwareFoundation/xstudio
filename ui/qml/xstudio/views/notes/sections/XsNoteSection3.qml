// SPDX-License-Identifier: Apache-2.0

import QtQuick

import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Rectangle{
    color: Qt.lighter(panelColor, 1.2) //bgColorNormal

    property bool isHovered: timeDiv.hovered || closeBtn.hovered ||
        authorDiv.hovered ||
        inSetDiv.hovered || outSetDiv.hovered || durLoopDiv.hovered ||
        showButton.hovered


    property real minHeight: itemHeight - itemSpacing/2
    property real minWidth: 40

    property real visibleWidth: showButton.width + detailsGrid.width + 1
    property real detailsWidth: 200


    XsSecondaryButton{ id: showButton
        width: 20
        height: parent.height - minHeight - 2
        anchors.bottom: parent.bottom
        imgSrc: "qrc:/icons/chevron_right.svg"
        forcedBgColorNormal: panelColor
        imageDiv.rotation: showDetails ? 0 : 180

        onClicked: {
            showDetails = !showDetails
        }
    }

    GridLayout { id: detailsGrid
        anchors.left: showButton.right
        anchors.leftMargin: 1
        width: showDetails? detailsWidth - showButton.width - 1 : 0
        Behavior on width {NumberAnimation{ duration: 250 }}
        height: parent.height - 1
        columnSpacing: 1
        rowSpacing: 1
        columns: 4
        // rows: 6
        visible: showDetails


        /// row0 - Empty for close button
        Item {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            Layout.minimumHeight: minHeight
        }

        /// row1 - Time
        Rectangle {
            Layout.columnSpan: 4
            Layout.fillWidth: true
            Layout.minimumWidth: minWidth*4
            Layout.minimumHeight: minHeight
            color: panelColor

            XsTextInput{ id: timeDiv
                anchors.centerIn: parent
                color: highlightColor
                text: createdRole
                readOnly: true
            }
        }

        /// row2 - Author
        Rectangle {
            Layout.columnSpan: 4
            Layout.fillWidth: true
            Layout.minimumHeight: minHeight
            color: panelColor

            XsTextInput { id: authorDiv
                anchors.centerIn: parent
                color: highlightColor
                text: authorRole
                readOnly: true
            }
        }

        /// row3 - In
        Rectangle {
            Layout.minimumWidth: minWidth
            Layout.minimumHeight: minHeight
            color: panelColor

            XsText {
                anchors.fill: parent
                text: "In"
            }
        }
        Rectangle {
            color: panelColor
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.minimumWidth: minWidth*2
            Layout.minimumHeight: minHeight

            XsText {
                anchors.fill: parent
                anchors.margins: 1
                text: startTimecodeRole
            }
        }
        Rectangle {
            Layout.minimumWidth: minWidth
            Layout.minimumHeight: minHeight
            color: panelColor

            XsSecondaryButton{
                id: inSetDiv
                anchors.fill: parent
                text: "Set"
                onClicked: {
                    if (ownerRole == currentPlayhead.mediaUuid) {
                        startFrameRole = currentPlayhead.mediaFrame
                    }
                }
            }
        }

        /// row4 - Out
        Rectangle {
            Layout.minimumWidth: minWidth
            Layout.minimumHeight: minHeight
            color: panelColor

            XsText {
                anchors.fill: parent
                text: "Out"
            }
        }
        Rectangle {
            color: panelColor
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.minimumWidth: minWidth*2
            Layout.minimumHeight: minHeight

            XsText {
                anchors.fill: parent
                anchors.margins: 1
                text: endTimecodeRole
            }
        }
        Rectangle {
            Layout.minimumWidth: minWidth
            Layout.minimumHeight: minHeight
            color: panelColor

            XsSecondaryButton{
                id: outSetDiv
                anchors.fill: parent
                text: "Set"
                onClicked: {
                    if (ownerRole == currentPlayhead.mediaUuid) {
                        endFrameRole = currentPlayhead.mediaFrame
                    }
                }

            }
        }

        /// row5 - Duration
        Rectangle {
            Layout.minimumWidth: minWidth
            Layout.minimumHeight: minHeight
            color: panelColor

            XsText {
                anchors.fill: parent
                text: "Dur"
            }
        }
        Rectangle {
            color: panelColor
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.minimumWidth: minWidth*2
            Layout.minimumHeight: minHeight

            XsText {
                anchors.fill: parent
                anchors.margins: 1
                text: durationTimecodeRole
            }
        }
        Rectangle {
            Layout.minimumWidth: minWidth
            Layout.minimumHeight: minHeight
            color: panelColor

            XsSecondaryButton{ id: durLoopDiv
                anchors.fill: parent
                text: "Loop"
            }
        }

    }

}