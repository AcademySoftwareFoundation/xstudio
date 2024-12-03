// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

Rectangle{
    color: "transparent"

    property bool isHovered: notesDiv.isHovered

    ColumnLayout {
        anchors.fill: parent
        spacing: 1

        Rectangle{ id: subjectDiv
            Layout.fillWidth: true
            Layout.minimumHeight: itemHeight
            color: Qt.lighter(panelColor, 1.2)

            RowLayout {
                anchors.margins: itemSpacing
                anchors.fill: parent

                XsText{
                    text: "Subject: "
                    font.bold: true
                    color: XsStyleSheet.hintColor
                    height: parent.height
                }
                XsTextInput{
                    Layout.fillWidth: true
                    focus: false
                    color: highlightColor
                    text: subjectRole
                    height: parent.height
                }
            }
        }

        Rectangle{ id: notesDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: notesEdit.activeFocus? Qt.lighter(panelColor, 1.4) : Qt.lighter(panelColor, 1.2)

            property bool isHovered: mArea.containsMouse || notesEdit.activeFocus

            MouseArea{ id: mArea
                anchors.fill: parent;
                hoverEnabled: true
                z: notesEdit.activeFocus? 0:1
                onClicked: notesEdit.forceActiveFocus()
            }

            Flickable{ id: flickDiv

                anchors.margins: itemSpacing
                anchors.fill: parent

                contentWidth: notesEdit.paintedWidth
                contentHeight: notesEdit.paintedHeight

                clip: true
                boundsMovement: Flickable.StopAtBounds

                ScrollBar.vertical: XsScrollBar{}

                function ensureVisible(r) {
                    if (contentX >= r.x)
                        contentX = r.x;
                    else if (contentX+width <= r.x+r.width)
                        contentX = r.x+r.width-width;
                    if (contentY >= r.y)
                        contentY = r.y;
                    else if (contentY+height <= r.y+r.height)
                        contentY = r.y+r.height-height;
                }

                XsTextEdit{ id: notesEdit

                    width: flickDiv.width
                    height: lineCount<=6? flickDiv.height : flickDiv.height*lineCount

                    text: noteRole
                    hint: activeFocus? "" : "Enter note here..."
                    onCursorRectangleChanged: flickDiv.ensureVisible(cursorRectangle)

                    focus: (mArea.containsMouse || activeFocus)
                    onFocusChanged: if(focus) forceActiveFocus()

                    onEditingFinished: { //#TODO
                        noteRole = text
                    }

                    onTextChanged: {
                        if (noteRole != text) {
                            noteRole = text
                        }
                    }

                }
            }

        }
    }

}