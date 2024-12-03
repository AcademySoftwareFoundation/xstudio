// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xStudio 1.0

import xstudio.qml.helpers 1.0

XsWindow {

    id: dialog
    title: "Metadata for "
    property var metadata_digest
    minimumWidth: 600
    minimumHeight: 600

    property int highlightIndex: -1
    property bool globalPressed: false
    onGlobalPressedChanged: {
        if (globalPressed) {
            metadataSelected(r1.itemAt(highlightIndex).text)
        }
    }

    signal metadataSelected(string metadataPath)

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 10
        Item {

            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {

                id: flickable
                anchors.fill: parent
                anchors.margins: 10

                ScrollBar.vertical: XsScrollBar {
                    parent: flickable.parent
                    width: 6
                    anchors.top: flickable.top
                    anchors.bottom: flickable.bottom
                    anchors.left: flickable.right
                    anchors.leftMargin: 4
                    visible: flickable.height < flickable.contentHeight
                    policy: ScrollBar.AlwaysOn
                }
                ScrollBar.horizontal: XsScrollBar {
                    parent: flickable.parent
                    height: 6
                    anchors.left: flickable.left
                    anchors.right: flickable.right
                    anchors.top: flickable.bottom
                    anchors.topMargin: 4
                    visible: flickable.width < flickable.contentWidth
                    policy: ScrollBar.AlwaysOn
                }

                contentWidth: grid_layout.width
                contentHeight: grid_layout.height
                clip: true

                GridLayout {
                    id: grid_layout
                    columns: 2
                    rowSpacing: 0
                    columnSpacing: 0
                    Repeater {
                        id: r1
                        model: metadata_digest
                        XsLabel {
                            text: metadata_digest[index][0]
                            Layout.fillWidth: true
                            Layout.row: index
                            Layout.column: 0
                            horizontalAlignment: Text.AlignLeft
                            topPadding: 4
                            bottomPadding: 4
                            leftPadding: 20
                            rightPadding: 20
                            Rectangle {
                                width: parent.width
                                height: 1
                                opacity: 0.5
                            }
                            background: Rectangle {
                                color: highlightIndex == index ? palette.highlight : "transparent"
                                opacity: globalPressed ? 1.0 : 0.5
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onContainsMouseChanged: {
                                    if (containsMouse) highlightIndex = index
                                    else if (highlightIndex == index) highlightIndex = -1
                                }
                                onPressedChanged: {
                                    globalPressed = pressed
                                }
                            }
                        }
                    }
                    Repeater {
                        model: metadata_digest
                        id: r2
                        XsLabel {
                            text: metadata_digest[index][1]
                            Layout.fillWidth: true
                            Layout.row: index
                            Layout.column: 1
                            horizontalAlignment: Text.AlignLeft
                            topPadding: 4
                            bottomPadding: 4
                            leftPadding: 20
                            rightPadding: 20
                            Rectangle {
                                width: parent.width
                                height: 1
                                opacity: 0.5
                            }
                            Rectangle {
                                height: parent.height
                                width: 1
                                opacity: 0.5
                            }
                            background: Rectangle {
                                color: highlightIndex == index ? palette.highlight : "transparent"
                                opacity: globalPressed ? 1.0 : 0.5
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onContainsMouseChanged: {
                                    if (containsMouse) highlightIndex = index
                                    else if (highlightIndex == index) highlightIndex = -1
                                }
                                onPressedChanged: {
                                    globalPressed = pressed
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            height: 10
        }

        XsSimpleButton {

            text: qsTr("Close")
            Layout.alignment: Qt.AlignRight
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            onClicked: {
                dialog.visible = false
            }
        }

    }

}