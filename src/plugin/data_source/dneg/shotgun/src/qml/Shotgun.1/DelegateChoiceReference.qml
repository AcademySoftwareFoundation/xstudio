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
    roleValue: "Reference"
    Rectangle { id: shotDelegate
        property bool isSelected: selectionModel.isSelected(searchResultsViewModel.index(index, 0))
        property bool isMouseHovered: mArea.containsMouse || nameDisplay.hovered || stepDisplay.hovered ||
                                      authorDisplay.hovered || pipelineStatusDisplay.hovered ||
                                      pipeStatusDisplay.hovered || submitToClientDisplay.hovered ||
                                      frameToolTip.containsMouse || dateToolTip.containsMouse// || versionsButton.hovered || allVersionsButton.hovered
        width: searchResultsView.cellWidth
        height: searchResultsView.cellHeight-itemSpacing
        color:  isSelected ? Qt.darker(itemColorActive, 2.75): itemColorNormal
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
            onDoubleClicked: {
                if (mouse.button == Qt.LeftButton) {
                    selectionModel.select(searchResultsViewModel.index(index, 0), ItemSelectionModel.ClearAndSelect)
                    selectionModel.resetSelection()
                    selectionModel.countSelection(true)
                    selectionModel.prevSelectedIndex = index
                    applySelection(
                        function(items){
                            addShotsToPlaylist(
                                items,
                                data_source.preferredVisual("Versions"),
                                data_source.preferredAudio("Versions"),
                                data_source.flagText("Versions"),
                                data_source.flagColour("Versions")
                            )
                        }
                    )
                }
            }

            onClicked: {
                searchResultsDiv.itemClicked(mouse, index, isSelected)
            }
        }

        GridLayout {
            anchors.fill: parent
            anchors.margins: framePadding
            rows: 2
            columns: 8
            rowSpacing: itemSpacing

            Rectangle{ id: indicators

                Layout.preferredWidth: 20
                Layout.fillHeight: true
                Layout.columnSpan: 1
                Layout.rowSpan: 2
                color: "transparent"

                Column{
                    spacing: itemSpacing
                    anchors.fill: parent
                    y: itemSpacing

                    XsButton{ id: pipeStatusDisplay
                        property bool hasNotes: noteCountRole === undefined || noteCountRole == 0  ? false :true
                        text: "N"
                        width: 20
                        height: parent.height/3 - itemSpacing/2
                        font.pixelSize: fontSize*1.2
                        font.weight: Font.Medium
                        focus: false
                        enabled: hasNotes
                        bgColorNormal: hasNotes ? "#c27b02" : palette.base
                        textDiv.topPadding: 2.5

                        onClicked: {
                            let mymodel = noteTreePresetsModel
                            mymodel.clearLoaded()
                            mymodel.insert(
                                mymodel.rowCount(),
                                mymodel.index(mymodel.rowCount(),0),
                                {
                                    "expanded": false,
                                    "name": nameRole + " Notes",
                                    "queries": [
                                        {
                                            "enabled": true,
                                            "term": "Twig Name",
                                            "value": "^"+twigNameRole+"$"
                                        },
                                        {
                                            "enabled": shotRole != undefined,
                                            "term": "Shot",
                                            "value": shotRole != undefined ? shotRole : ""
                                        },
                                        {
                                            "enabled": true,
                                            "term": "Version Name",
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
                                        }
                                    ]
                                }
                            )
                            currentCategory = "Notes Tree"
                            mymodel.activePreset = mymodel.rowCount()-1
                        }
                    }
                    XsButton{ id: dailiesStatusDisplay
                        property bool hasDailes: submittedToDailiesRole === undefined ? false :true
                        text: "D"
                        width: pipeStatusDisplay.width
                        height: pipeStatusDisplay.height
                        font.pixelSize: fontSize*1.2
                        font.weight: Font.Medium
                        focus: false
                        enabled: false //hasDailes
                        bgColorNormal: hasDailes ? "#2b7ffc" : palette.base
                        textDiv.topPadding: 2.5
                    }
                    XsButton{ id: submitToClientDisplay
                        property bool isSent: dateSubmittedToClientRole === undefined ? false : true
                        enabled: false //isSent
                        text: "C"
                        width: pipeStatusDisplay.width
                        height: pipeStatusDisplay.height
                        font.pixelSize: fontSize*1.2
                        font.weight: Font.DemiBold
                        focus: false
                        bgColorNormal: isSent  ? "green" : palette.base
                        textDiv.topPadding: 2.5

                        onClicked: createPreset("Sent To Client", "True")
                    }

                }

            }
            Item{ id: thumbnail
                Layout.preferredWidth: parent.height * 1.5
                Layout.fillHeight: true
                Layout.rowSpan: 2

                property bool failed: thumbRole == undefined

                Text{ id: noThumbnailDisplay
                    text: parent.failed ? "No ShotGrid Thumbnail" : "Loading..."
                    width: parent.width
                    height: parent.height
                    wrapMode: Text.WordWrap
                    font.pixelSize: fontSize*1.2
                    font.weight: Font.Black
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: isSelected? Qt.darker("dark grey",1) : parent.failed? XsStyle.mainBackground : textColorNormal
                    opacity: isSelected? 0.3 : 0.9
                }
                Image {
                    visible: !parent.failed
                    anchors.fill: parent
                    source: parent.failed ?  "" : thumbRole

                    asynchronous: true
                    cache: true
                    sourceSize.height: height
                    sourceSize.width:  width
                    opacity: 0
                    Behavior on opacity {NumberAnimation {duration: 150}}

                    onStatusChanged: {
                        if (status == Image.Error) { parent.failed=true; opacity=0 }
                        else if (status == Image.Ready) opacity=1
                        else opacity=0
                    }
                }
            }

            XsTextButton{ id: nameDisplay
                text: " "+nameRole
                isClickable: false
                onTextClicked: createPreset("Twig Name", twigNameRole)
                Layout.columnSpan: stepDisplay.visible? 3 : 5

                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                // Layout.preferredWidth: 150
                textDiv.font.pixelSize: fontSize*1.2
                textDiv.font.weight: Font.Medium
                textDiv.elide: Text.ElideMiddle
                textDiv.horizontalAlignment: Text.AlignLeft
                textDiv.verticalAlignment: Text.AlignVCenter
                forcedMouseHover: isMouseHovered

                ToolTip.text: nameRole
                ToolTip.visible: hovered && textDiv.truncated
            }

            XsTextButton{ id: stepDisplay
                visible: text != ""
                text: pipelineStepRole ? pipelineStepRole : ""
                isClickable: false
                textDiv.font.pixelSize: fontSize*1.2
                textDiv.font.weight: Font.Medium
                textDiv.elide: Text.ElideRight
                textDiv.horizontalAlignment: Text.AlignRight
                textDiv.rightPadding: 3
                forcedMouseHover: isMouseHovered
                Layout.alignment: Qt.AlignRight
                Layout.fillWidth: true
                Layout.minimumWidth: 65
                Layout.preferredWidth: 70
                Layout.maximumWidth: 92
                Layout.columnSpan: 2

                ToolTip.text: text
                ToolTip.visible: hovered && textDiv.truncated

                onTextClicked: createPreset("Pipeline Step", textDiv.text)
            }

            Rectangle{ id: siteDisplay
                Layout.fillWidth: true
                Layout.minimumWidth: 50
                Layout.preferredWidth: 83
                Layout.maximumWidth: 93
                Layout.fillHeight: true
                Layout.columnSpan: 1
                Layout.rowSpan: 2
                color: "transparent"

                Grid{ id: col
                    spacing: itemSpacing/3
                    rows: 3
                    columns: 2
                    flow: Grid.TopToBottom
                    anchors.fill: parent

                    Repeater{
                        model: siteModel //["chn","lon","mtl","mum","syd",van"]

                        XsButton{ id: onDiskDisplay
                            property int onDisk: {
                                if(index==0) onSiteChn
                                else if(index==1) onSiteLon
                                else if(index==2) onSiteMtl
                                else if(index==3) onSiteMum
                                else if(index==4) onSiteSyd
                                else if(index==5) onSiteVan
                                else false
                            }
                            text: siteName
                            width: siteDisplay.width/2
                            height: siteDisplay.height/3 -col.spacing
                            font.pixelSize: fontSize/1.2
                            font.weight: Font.Medium
                            focus: false
                            enabled: false
                            borderWidth: 0
                            bgColorNormal: onDisk ? Qt.darker(siteColour, onDisk == 1 ? 1.5:1.0) : palette.base
                            textDiv.topPadding: 2
                        }
                    }
                    ListModel{
                        id: siteModel
                        ListElement{siteName:"chn"; siteColour:"#508f00"}
                        ListElement{siteName:"lon"; siteColour:"#2b7ffc"}
                        ListElement{siteName:"mtl"; siteColour:"#979700"}
                        ListElement{siteName:"mum"; siteColour:"#ef9526"}
                        ListElement{siteName:"syd"; siteColour:"#008a46"}
                        ListElement{siteName:"van"; siteColour:"#7a1a39"}
                    }
                }

            }



            Text{ id: dateDisplay
                Layout.alignment: Qt.AlignLeft
                // Layout.fillWidth: true
                property var dateFormatted: createdDateRole.toLocaleString().split(" ")
                text: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""
                font.pixelSize: fontSize
                font.family: fontFamily
                color: isMouseHovered? textColorActive: textColorNormal
                opacity: 0.6
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                Layout.fillWidth: true
                Layout.minimumWidth: 68
                Layout.preferredWidth: 74
                Layout.maximumWidth: 74

                ToolTip.text: createdDateRole.toLocaleString()
                ToolTip.visible: dateToolTip.containsMouse //&& truncated
                MouseArea {
                    id: dateToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }

            Text{ id: frameDisplay
                text: frameRangeRole ? frameRangeRole : ""
                font.pixelSize: fontSize
                font.family: fontFamily
                color: isMouseHovered? textColorActive: textColorNormal
                opacity: 0.6
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.minimumWidth: 60
                Layout.preferredWidth: 80
                Layout.maximumWidth: 80

                ToolTip.text: frameRangeRole ? frameRangeRole : ""
                ToolTip.visible: frameToolTip.containsMouse && truncated
                MouseArea {
                    id: frameToolTip
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                }
            }

            XsTextButton{ id: authorDisplay
                text: authorRole? authorRole : ""
                isClickable: false
                textDiv.font.pixelSize: fontSize
                opacity: 0.6
                textDiv.elide: Text.ElideRight
                textDiv.horizontalAlignment: Text.AlignLeft
                forcedMouseHover: isMouseHovered
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                onTextClicked: createPreset("Author", textDiv.text)

                ToolTip.text: authorRole
                ToolTip.visible: hovered && textDiv.truncated
            }


            XsTextButton{
                text: tagRole ? ""+tagRole : ""
                isClickable: false
                textDiv.font.pixelSize: fontSize
                opacity: 0.6
                textDiv.elide: Text.ElideRight
                textDiv.horizontalAlignment: Text.AlignLeft
                forcedMouseHover: isMouseHovered
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft
                // Layout.columnSpan:
                // ToolTip.text: tagRole
                ToolTip.text: tagRole ? tagRole.join("\n") : ""
                ToolTip.visible: hovered && textDiv.truncated
            }

            XsTextButton{ id: pipelineStatusDisplay
                text: pipelineStatusFullRole? pipelineStatusFullRole : ""
                isClickable: false

                Layout.alignment: Qt.AlignRight
                Layout.columnSpan: 1
                Layout.fillWidth: true
                Layout.minimumWidth: 50
                Layout.preferredWidth: 60
                Layout.maximumWidth: 120
                textDiv.font.pixelSize: fontSize
                opacity: 0.6
                textDiv.elide: Text.ElideRight
                textDiv.horizontalAlignment: Text.AlignRight
                textDiv.verticalAlignment: Text.AlignVCenter
                textDiv.rightPadding: 3
                forcedMouseHover: isMouseHovered

                ToolTip.text: pipelineStatusFullRole
                ToolTip.visible: hovered && textDiv.truncated
            }

        }
    }
}
