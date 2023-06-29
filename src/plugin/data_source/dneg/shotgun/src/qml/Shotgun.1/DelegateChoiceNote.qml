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
    roleValue: "Note"

    Rectangle {
        id: root
        property bool isSelected: selectionModel.isSelected(searchResultsViewModel.index(index, 0))
        property bool isMouseHovered: mArea.containsMouse || nameDisplayText.hovered || toToolTip.containsMouse || fromToolTip.containsMouse ||
                                      dateToolTip.containsMouse || bodyToolTip.containsMouse || subjectToolTip.containsMouse
        width: searchResultsView.cellWidth
        height: searchResultsView.cellHeight-itemSpacing
        color: isSelected? Qt.darker(itemColorActive, 2.50): "#2e2e2e"
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
                    applySelection(function(items){
                        addShotsToPlaylist(items,
                            data_source.preferredVisual(currentCategory),
                            data_source.preferredAudio(currentCategory),
                            data_source.flagText(currentCategory),
                            data_source.flagColour(currentCategory)
                        )
                    })
                }
            }
        }


        GridLayout {
            anchors.fill: parent
            anchors.margins: framePadding
            rows: 7
            columns: 3

            Rectangle{ id: infoDisplay
                Layout.rowSpan: 7
                Layout.columnSpan: 1
                Layout.alignment: Qt.AlignLeft
                Layout.preferredWidth: 150
                Layout.maximumWidth: 170
                Layout.preferredHeight: parent.height
                color: root.isSelected? Qt.darker(itemColorActive, 3):"#242424"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: framePadding
                    rows: 7
                    columns: 2

                    Item{
                        id: mainImage
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.height/2
                        Layout.rowSpan: 3
                        Layout.columnSpan: 2

                        property bool failed: thumbRole == undefined

                        Text{ id: noThumbnailDisplay
                            text: parent.failed? "No ShotGrid Thumbnail":"Loading..."
                            width: parent.width
                            height: parent.height
                            wrapMode: Text.WordWrap
                            font.pixelSize: fontSize*1.2
                            font.weight: Font.Black
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: textColorNormal
                            opacity: parent.failed? 0.4 : 0.9
                        }
                        Image { id: thumbnail
                            visible: !parent.failed
                            anchors.fill: parent
                            source: parent.failed ? "": thumbRole

                            cache: true
                            asynchronous: true
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

                    Text{
                        text: "Date :"
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    }

                    Text{ id: dateDisplay
                        Layout.alignment: Qt.AlignRight
                        property var dateFormatted: createdDateRole.toLocaleString().split(" ")
                        text: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight

                        ToolTip.text: dateFormatted[0].substr(0,dateFormatted[0].length-1) //createdDateRole.toLocaleString().substr(0,3)
                        ToolTip.visible: dateToolTip.containsMouse
                        MouseArea {
                            id: dateToolTip
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                        }
                    }

                    Text{
                        text: "Time :"
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    }

                    Text{ id: timeDisplay
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        property var dateFormatted: createdDateRole.toLocaleString().split(" ")
                        property var timeFormatted: dateFormatted[4].split(":")
                        text: typeof timeFormatted !== 'undefined'? 
                            typeof dateFormatted[6] !== 'undefined'? 
                                timeFormatted[0]+":"+timeFormatted[1]+" "+dateFormatted[5]+" "+dateFormatted[6] :
                                timeFormatted[0]+":"+timeFormatted[1]+" "+dateFormatted[5] : ""
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }

                    Text{
                        text: "Type :"
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    }

                    Text{ id: typeDisplay
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        // Layout.fillWidth: true
                        text: noteTypeRole ? noteTypeRole : ""
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }


                    Text{
                        text: "From :"
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                        Layout.preferredHeight: typeDisplay.height
                    }

                    Text{
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        text: createdByRole ? createdByRole : ""
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                        Layout.preferredHeight: typeDisplay.height

                        XsToolTip{
                            text: parent.text
                            visible: fromToolTip.containsMouse && parent.truncated
                            width: textDivMetrics.width == 0? 0 : 150
                            x: 0
                       }
                        MouseArea {
                            id: fromToolTip
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                        }
                    }

                    Text{
                        text: "To :"
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                        Layout.preferredHeight: typeDisplay.height
                    }

                    Text{
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        text: addressingRole ? addressingRole.join("\n") : ""
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: isMouseHovered? textColorActive: textColorNormal
                        opacity: 0.6
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                        Layout.preferredHeight: typeDisplay.height

                        XsToolTip{
                            text: parent.text
                            visible: toToolTip.containsMouse && parent.truncated
                            width: textDivMetrics.width == 0? 0 : 150
                            x: 0
                       }
                        MouseArea {
                            id: toToolTip
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                        }
                    }

                }
            }

            Rectangle{  id: nameDisplay
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                Layout.columnSpan: 1
                Layout.preferredHeight: itemHeight
                color: root.isSelected? Qt.darker(itemColorActive, 3):"#242424" //"transparent"

                XsTextButton{ id: nameDisplayText
                    text: versionNameRole || ""

                    anchors.left: nameDisplay.left
                    anchors.leftMargin: framePadding
                    width: nameDisplay.width - framePadding*2

                    textDiv.font.pixelSize: fontSize*1.2
                    textDiv.font.weight: Font.Bold
                    textDiv.elide: Text.ElideMiddle
                    textDiv.horizontalAlignment: Text.AlignLeft
                    // textDiv.verticalAlignment: Text.AlignVCenter
                    anchors.bottom: nameDisplay.bottom
                    anchors.bottomMargin: (parent.height - height)/2 - 1
                    forcedMouseHover: isMouseHovered
                    isClickable: shotNameRole !== undefined

                    onTextClicked: {
                        let mymodel = noteTreePresetsModel
                        // mymodel.clearLoaded()
                        mymodel.insert(
                            mymodel.rowCount(),
                            mymodel.index(mymodel.rowCount(), 0),
                            {
                                "expanded": false,
                                "name": shotNameRole + " Notes",
                                "queries": [
                                    {
                                        "enabled": true,
                                        "term": "Twig Name",
                                        "value": "^"+twigNameRole+"$"
                                    },
                                    {
                                        "enabled": true,
                                        "term": "Shot",
                                        "value": shotNameRole
                                    },
                                    {
                                        "enabled": false,
                                        "term": "Version Name",
                                        "value": versionNameRole
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

                    ToolTip.text: versionNameRole || ""
                    ToolTip.visible: hovered & textDiv.truncated
                }
            }

            Rectangle{ id: thumbnailsDisplay
                Layout.rowSpan: 7
                Layout.columnSpan: 1
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                Layout.preferredWidth: mainImage.width/2.2 //nameDisplay.width/5
                Layout.preferredHeight: parent.height
                color: root.isSelected? Qt.darker(itemColorActive, 3):"#242424"

                // ColumnLayout{
                //     spacing: framePadding
                //     anchors.horizontalCenter: parent.horizontalCenter
                //     y: framePadding

                //     Repeater{
                //         model: Math.floor(Math.random() * 3) + 1; // #TODO
                //         Image {
                //             width: thumbnailsDisplay.width - framePadding*2
                //             height: width/2
                //             property bool failed: false
                //             source: !parent.visible ? "" : ( failed ? "qrc:/feather_icons/film.svg": thumbRole )
                //             asynchronous: true
                //             sourceSize.height: height
                //             sourceSize.width:  width
                //             onStatusChanged: if (status == Image.Error) failed=true
                //             layer {
                //                 enabled: failed
                //                 effect: ColorOverlay {
                //                     color: XsStyle.mainBackground
                //                 }
                //             }
                //         }
                //     }
                // }

            }


            Rectangle{  id: subjectDisplay
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.preferredWidth: 100
                Layout.fillWidth: true
                Layout.preferredHeight: itemHeight
                color: root.isSelected? Qt.darker(itemColorActive, 3):"#242424"

                Text{
                   text: subjectRole ? subjectRole : ""
                   font.pixelSize: fontSize
                   font.family: fontFamily
                   color: isMouseHovered? textColorActive: textColorNormal
                   opacity: 0.6
                   elide: Text.ElideRight
                   horizontalAlignment: Text.AlignLeft
                   anchors.bottom: parent.bottom
                   anchors.bottomMargin: (parent.height - height)/2 - 1
                   padding: framePadding-.1
                   width: parent.width

                   XsToolTip{
                        text: parent.text
                        visible: subjectToolTip.containsMouse & parent.truncated
                        width: textDivMetrics.width == 0? 0 : parent.width>500? 520:parent.width+20
                        x: 0
                   }
                   MouseArea {
                       id: subjectToolTip
                       anchors.fill: parent
                       hoverEnabled: true
                       propagateComposedEvents: true
                   }
                }
            }

            Rectangle{  id: bodyDisplay
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.preferredWidth: 100
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.rowSpan: 5
                color: root.isSelected? Qt.darker(itemColorActive, 3):"#242424"

                XsTextEdit{  id: bodyTextDisplay
                    text: contentRole ? contentRole : ""
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    color: isMouseHovered? textColorActive: textColorNormal
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignTop
                    padding: framePadding- 0.1
                    readOnly: true
                    width: parent.width
                    height: parent.height
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.AutoText
                    textDiv.height: parent.height

                    XsToolTip{
                        id: toolTip
                        text: bodyTextDisplay.text
                        visible: bodyToolTip.containsMouse && bodyTextDisplay.lineCount>7
                        width: textDivMetrics.width == 0? 0 : bodyTextDisplay.width>500? 520: bodyTextDisplay.width+20
                        x: 0
                    }
                    MouseArea {
                        id: bodyToolTip
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                    }
                }
                Rectangle{anchors.fill: overlapImage; color: bodyDisplay.color; radius: 2; visible: overlapImage.visible}
                Image {
                    id: overlapImage
                    visible: bodyTextDisplay.lineCount>7
                    source: "qrc:/feather_icons/corner-down-left.svg"
                    width: itemHeight/2
                    height: itemHeight/2
                    sourceSize.width: width
                    sourceSize.height: height
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    smooth: true
                    opacity: 0.7
                    layer {
                        enabled: true
                        effect:
                        ColorOverlay {
                            color: toolTip.visible? palette.highlight : bodyTextDisplay.color
                        }
                    }
                }
            }

        }
    }
}