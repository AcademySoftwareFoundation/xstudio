// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import Qt.labs.qmlmodels 1.0

import xStudio 1.1

DelegateChoice {
    roleValue: "Playlist"
    Rectangle {
        property bool isSelected: selectionModel.isSelected(searchResultsViewModel.index(index, 0))
        property bool isMouseHovered: mArea.containsMouse || noteDisplay.hovered ||
                                      dateToolTip.containsMouse || typeToolTip.containsMouse ||
                                      nameToolTip.containsMouse || deptToolTip.containsMouse ||
                                      authorToolTip.containsMouse || versionsButton.hovered
        width: searchResultsView.cellWidth
        height: searchResultsView.cellHeight-itemSpacing
        color: isSelected? Qt.darker(itemColorActive, 2.75): itemColorNormal
        border.color: isMouseHovered? itemColorActive: itemColorNormal
        clip: true

        Connections {
            target: selectionModel
            function onSelectionChanged(selected, deselected) {
                isSelected = selectionModel.isSelected(searchResultsViewModel.index(index, 0))
            }
        }

        MouseArea{
            id: mArea
            anchors.fill: parent
            hoverEnabled: true

            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                searchResultsDiv.itemClicked(mouse, index, isSelected)
            }

            onDoubleClicked: {
                if (mouse.button == Qt.LeftButton) {
                    selectionModel.select(searchResultsViewModel.index(index, 0), ItemSelectionModel.ClearAndSelect)
                    selectionModel.resetSelection()
                    selectionModel.countSelection(true)
                    selectionModel.prevSelectedIndex = index
                    applySelection(
                        function(items){
                            loadPlaylists(
                                items,
                                data_source.preferredVisual(currentCategory),
                                data_source.preferredAudio(currentCategory),
                                data_source.flagText(currentCategory),
                                data_source.flagColour(currentCategory)
                            )
                        }
                    )
                }
            }
        }


        GridLayout {
            anchors.fill: parent
            anchors.margins: framePadding
            rows: 2
            columns: 7

            XsButton{ id: noteDisplay
                property bool hasNotes: noteCountRole === undefined || noteCountRole == 0  ? false :true
                text: "N"
                Layout.preferredWidth: 20
                Layout.fillHeight: true
                Layout.rowSpan: 2
                font.pixelSize: fontSize*1.2
                font.weight: Font.Medium
                focus: false
                enabled: hasNotes
                bgColorNormal: hasNotes  ? "#c27b02" : palette.base
                textDiv.topPadding: 3

                onClicked: {
                    let mymodel = noteTreePresetsModel

                    mymodel.clearLoaded()
                    mymodel.insert(
                        mymodel.rowCount(),
                        mymodel.index(mymodel.rowCount(), 0),
                        {
                            "expanded": false,
                            "name": nameRole + " Notes",
                            "queries": [
                                {
                                    "enabled": true,
                                    "term": "Playlist",
                                    "value": nameRole
                                },
                                {
                                    "enabled": false,
                                    "term": "Note Type",
                                    "value": "Director"
                                },
                                {
                                    "enabled": false,
                                    "term": "Note Type",
                                    "value": "Client"
                                },
                                {
                                    "enabled": false,
                                    "term": "Note Type",
                                    "value": "VFXSupe"
                                },
                                {
                                    "enabled": false,
                                    "term": "Recipient",
                                    "value": data_source.getShotgunUserName()
                                }
                            ]
                        }
                    )
                    currentCategory = "Notes Tree"
                    mymodel.activePreset = mymodel.rowCount()-1
                }
            }

            XsButton{ id: versionsButton
                Layout.preferredWidth: noteDisplay.width
                Layout.preferredHeight: parent.height
                Layout.rowSpan: 2

                text: "Versions"
                textDiv.width: parent.height
                textDiv.opacity: hovered ? 1 : isMouseHovered? 0.8 : 0.6
                textDiv.rotation: -90
                textDiv.topPadding: 2.5
                textDiv.rightPadding: 3
                font.pixelSize: fontSize
                font.weight: Font.DemiBold
                padding: 0
                bgDiv.border.color: down || hovered ? bgColorPressed: Qt.darker(bgColorNormal,1.5)
                onClicked: {
                    let mymodel = shotPresetsModel
                    mymodel.clearLoaded()
                    mymodel.insert(
                        mymodel.rowCount(),
                        mymodel.index(mymodel.rowCount(), 0),
                        {
                            "expanded": false,
                            "name": nameRole + " Versions",
                            "queries": [
                                {
                                    "enabled": true,
                                    "term": "Playlist",
                                    "value": nameRole
                                }
                            ]
                        }
                    )
                    currentCategory = "Versions"
                    mymodel.activePreset = mymodel.rowCount()-1
                }
            }

            Text{ id: countDisplay
                text: itemCountRole
                color: isMouseHovered? textColorActive: textColorNormal
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Layout.preferredWidth: parent.height + 10
                Layout.maximumWidth: parent.height + 10
                Layout.preferredHeight: parent.height
                Layout.rowSpan: 2
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter

                font.pixelSize: fontSize*1.2
                font.family: fontFamily
                font.bold: true
            }

            Text{ id: nameDisplay
                text: nameRole || ""
                Layout.columnSpan: 3
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                font.pixelSize: fontSize*1.2
                font.family: fontFamily
                font.bold: true
                color: isMouseHovered? textColorActive: textColorNormal
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignLeft

                ToolTip.text: text
                ToolTip.visible: nameToolTip.containsMouse && truncated
                MouseArea {
                    id: nameToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }

            Text{
                text: departmentRole || ""
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: 140
                Layout.maximumWidth: 200
                font.pixelSize: fontSize
                font.family: fontFamily
                color: isMouseHovered? textColorActive: textColorNormal
                opacity: 0.6
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight

                ToolTip.text: text
                ToolTip.visible: deptToolTip.containsMouse && truncated
                MouseArea {
                    id: deptToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }

            Text{ id: typeDisplay
                text: typeRole || ""
                Layout.column: 3
                Layout.row: 1
                Layout.alignment: Qt.AlignLeft
                font.pixelSize: fontSize
                font.family: fontFamily
                color: isMouseHovered? textColorActive: textColorNormal
                opacity: 0.6
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                Layout.preferredWidth: 140
                Layout.maximumWidth: 140

                ToolTip.text: typeRole || ""
                ToolTip.visible: typeToolTip.containsMouse && truncated
                MouseArea {
                    id: typeToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }

            Text{ id: dateDisplay
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 68
                Layout.preferredWidth: 76
                Layout.maximumWidth: 76
                property var dateFormatted: createdDateRole.toLocaleString().split(" ")
                text: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""
                font.pixelSize: fontSize
                font.family: fontFamily
                color: isMouseHovered? textColorActive: textColorNormal
                opacity: 0.6
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft

                ToolTip.text: createdDateRole.toLocaleString()
                ToolTip.visible: dateToolTip.containsMouse
                MouseArea {
                    id: dateToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }

            Item{
                id: emptyDiv
                Layout.preferredHeight: 1
                Layout.fillWidth: true
            }

            Text{
                text: authorRole || ""
                Layout.alignment: Qt.AlignRight
                font.pixelSize: fontSize
                font.family: fontFamily
                color: isMouseHovered? textColorActive: textColorNormal
                opacity: 0.6
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight

                ToolTip.text: text
                ToolTip.visible: authorToolTip.containsMouse && truncated
                MouseArea {
                    id: authorToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }
        }
    }
}