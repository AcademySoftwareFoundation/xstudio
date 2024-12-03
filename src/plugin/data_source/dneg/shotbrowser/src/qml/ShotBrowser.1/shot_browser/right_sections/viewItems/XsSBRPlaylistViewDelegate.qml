// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QuickFuture 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0


Rectangle {
    property var delegateModel: null

    property int itemSpacing: 1
    property color itemColorActive: palette.highlight
    property color itemColorNormal: "transparent"

    property var popupMenu: null

    property bool isMouseHovered: mArea.containsMouse ||
                                versionArrowBtn.hovered ||
                                notesBtn.hovered ||
                                authorDisplay.isHovered ||
                                typeDisplay.isHovered ||
                                timeDisplay.isHovered ||
                                dateDisplay.isHovered ||
                                deptDisplay.isHovered ||
                                nameDisplay.isHovered

    property bool isSelected: resultsSelectionModel.isSelected(delegateModel.modelIndex(index)) //delegateModel? selectionModel.isSelected(delegateModel.modelIndex(index)) : false

    color: isSelected? Qt.darker(itemColorActive, 2.75): XsStyleSheet.widgetBgNormalColor

    border.color: isMouseHovered? itemColorActive : "transparent"

    Connections {
        target: resultsSelectionModel
        function onSelectionChanged(selected, deselected) {
            isSelected = resultsSelectionModel.isSelected(delegateModel.modelIndex(index))
        }
    }

    MouseArea{
        id: mArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        // wierd workaround for flickable..
        propagateComposedEvents: false
        onReleased: {
            if(!propagateComposedEvents)
                propagateComposedEvents = true
        }

        onDoubleClicked: ShotBrowserHelpers.loadShotgridPlaylists([delegateModel.modelIndex(index)])

        onPressed: (mouse)=>{
            // required for doubleclick to work
            mouse.accepted = true

            if (mouse.button == Qt.RightButton){
                if(popupMenu.visible) {
                    popupMenu.visible = false
                }
                else{
                    if(!isSelected) {
                        if(mouse.modifiers == Qt.NoModifier) {
                            ShotBrowserHelpers.selectItem(resultsSelectionModel, delegateModel.modelIndex(index))
                        } else if(mouse.modifiers == Qt.ShiftModifier){
                            ShotBrowserHelpers.shiftSelectItem(resultsSelectionModel, delegateModel.modelIndex(index))
                        } else if(mouse.modifiers == Qt.ControlModifier) {
                            ShotBrowserHelpers.ctrlSelectItem(resultsSelectionModel, delegateModel.modelIndex(index))
                        }
                    }
                    popupMenu.popupDelegateModel = delegateModel
                    let ppos = mapToItem(popupMenu.parent, mouseX, mouseY)
                    popupMenu.x = ppos.x
                    popupMenu.y = ppos.y
                    popupMenu.visible = true
                }
            }

            else if(mouse.modifiers == Qt.NoModifier) {
                ShotBrowserHelpers.selectItem(resultsSelectionModel, delegateModel.modelIndex(index))
            } else if(mouse.modifiers == Qt.ShiftModifier){
                ShotBrowserHelpers.shiftSelectItem(resultsSelectionModel, delegateModel.modelIndex(index))
            } else if(mouse.modifiers == Qt.ControlModifier) {
                ShotBrowserHelpers.ctrlSelectItem(resultsSelectionModel, delegateModel.modelIndex(index))
            }
        }

        GridLayout {
            anchors.fill: parent
            anchors.margins: panelPadding
            rows: 2
            columns: 7
            rowSpacing: itemSpacing
            columnSpacing: itemSpacing*3

            XsPrimaryButton{ id: versionArrowBtn
                Layout.preferredWidth: visible? 20 : 0
                Layout.fillHeight: true
                Layout.rowSpan: 2

                // height: visible? parent.itemHeight-listItemSpacing : 0
                imgSrc: "qrc:/icons/chevron_right.svg"

                imageDiv.rotation: isActive ? 90 : 0

                property bool hasVersions: versionCountRole == 0 ? false : true

                visible: true

                enabled: hasVersions
                isActiveViaIndicator: false
                isActive: delegateModel.notifyModel.isExpanded(index)
                onClicked: {
                    if(!isActive) {
                        delegateModel.notifyModel.expandRow(index)
                        isActive = delegateModel.notifyModel.isExpanded(index)
                    } else {
                        delegateModel.notifyModel.collapseRow(index)
                        isActive = delegateModel.notifyModel.isExpanded(index)
                    }
                }
            }


            XsPrimaryButton{ id: notesBtn
                Layout.preferredWidth: 20
                Layout.fillHeight: true
                Layout.rowSpan: 2
                property bool hasNotes: noteCountRole <= 0 ? false : true

                text: "N"
                font.weight: hasNotes? Font.Bold:Font.Normal
                font.pixelSize: XsStyleSheet.fontSize*1.2
                // bgColorPressed: Qt.darker(palette.highlight, 2)
                forcedBgColorNormal: hasNotes? palette.highlight : bgColorNormal
                textDiv.color: hasNotes? palette.text : XsStyleSheet.hintColor
                enabled: false //hasNotes
                bgDiv.opacity: enabled? 1.0 : 0.5
                isActive: hasNotes
                isActiveViaIndicator: false
                isUnClickable: true
                // onClicked:
            }

            XsText{
                Layout.preferredWidth: parent.height //* 1.5
                Layout.fillHeight: true
                Layout.rowSpan: 2
                text: versionCountRole == -1 ? "-" : versionCountRole
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                // font.weight: text=="0"? Font.Normal : Font.Black
                color: text=="0"? XsStyleSheet.hintColor : XsStyleSheet.secondaryTextColor
                font.pixelSize: XsStyleSheet.fontSize*1.2
                horizontalAlignment: Text.AlignHCenter
            }

            XsText{ id: nameDisplay
                Layout.columnSpan: 3
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                Layout.fillHeight: true

                text: nameRole
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignTop
                topPadding: 2

                isHovered: nameTooltip.containsMouse

                MouseArea { id: nameTooltip
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
            XsText{ id: deptDisplay
                Layout.fillWidth: true
                Layout.minimumWidth: 20
                Layout.preferredWidth: 60
                Layout.maximumWidth: 190
                Layout.fillHeight: true
                Layout.columnSpan: 1
                Layout.alignment: Qt.AlignRight

                text: departmentRole ? departmentRole : "-"
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignTop
                topPadding: 2
                rightPadding: 2

                isHovered: deptTooltip.containsMouse

                MouseArea { id: deptTooltip
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
            XsText{ id: dateDisplay
                Layout.columnSpan: 1
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.preferredWidth: 60
                Layout.maximumWidth: 92
                Layout.alignment: Qt.AlignHCenter

                property var dateFormatted: createdDateRole.toLocaleString().split(" ")
                text: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""

                elide: Text.ElideRight
                color: XsStyleSheet.hintColor
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignBottom
                bottomPadding: 2

                isHovered: dateTooltip.containsMouse

                MouseArea { id: dateTooltip
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
            XsText{ id: timeDisplay
                Layout.columnSpan: 1
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.preferredWidth: 60
                Layout.maximumWidth: 85
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom

                property var dateFormatted: createdDateRole.toLocaleString().split(" ")
                property var timeFormatted: dateFormatted[4].split(":")
                text: typeof timeFormatted !== 'undefined'?
                    typeof dateFormatted[6] !== 'undefined'?
                        timeFormatted[0]+":"+timeFormatted[1]+" "+dateFormatted[5]+" "+dateFormatted[6] :
                        timeFormatted[0]+":"+timeFormatted[1]+" "+dateFormatted[5] : ""


                elide: Text.ElideRight
                color: XsStyleSheet.hintColor
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignBottom
                bottomPadding: 2

                isHovered: timeTooltip.containsMouse

                MouseArea { id: timeTooltip
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
            XsText{ id: typeDisplay
                Layout.fillWidth: true
                Layout.minimumWidth: text? 60 : 0
                // Layout.maximumWidth: 110
                Layout.alignment: Qt.AlignLeft

                text: playlistTypeRole ? playlistTypeRole : "-"
                elide: Text.ElideRight
                color: XsStyleSheet.hintColor
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignBottom
                bottomPadding: 2

                isHovered: typeTooltip.containsMouse

                MouseArea { id: typeTooltip
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
            XsText{ id: authorDisplay
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight

                text: authorRole ? authorRole : "-" //createdByRole
                elide: Text.ElideRight
                color: XsStyleSheet.hintColor
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignBottom
                bottomPadding: 2
                rightPadding: 2

                isHovered: authorTooltip.containsMouse

                MouseArea { id: authorTooltip
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }
    }
}
