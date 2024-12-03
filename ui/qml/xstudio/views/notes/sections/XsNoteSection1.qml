// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import xStudio 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.bookmarks 1.0

Rectangle{ id: sec1
    color: "transparent"

    property bool isHovered: thumbMArea.containsMouse ||
        noteTypeCombo.hovered || noteTypeCombo.popupOptions.opened

    property var metadataChanged: metadataChangedRole
    property var decoratorModel: []

    onMetadataChangedChanged: updateDecorations()

    function updateDecorations() {
        let meta = bookmarkModel.getJSONObject(bookmarkFilterModel.mapToSource(bookmarkFilterModel.index(index,0)), "/ui/decorators")
        if(meta != undefined) {
            let tmp = []
            for (const [key, value] of Object.entries(meta)) {
              tmp.push([key, value])
            }
            decoratorModel = tmp
        }
    }

    Item{
        anchors.fill: parent

        XsImagePainter { id: thumb
            image: thumbnailRole
            width: parent.width
            height: width / (16/9)

            MouseArea { id: thumbMArea
                anchors.fill: parent
                propagateComposedEvents: true
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: jumpToNote(ownerRole, startFrameRole, frameFromTimecodeRole)
            }

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.leftMargin: 6
                spacing: 4

                Repeater {
                    model: decoratorModel

                    XsPrimaryButton {
                        imgSrc: modelData[1].icon || modelData[1]
                        height: 20
                        width: 20
                        background: Item{}
                        imgOverlayColor: XsStyleSheet.hintColor
                        toolTip: modelData[1].tooltip || ""

                        layer.enabled: true
                        layer.effect: DropShadow{
                            verticalOffset: 1
                            horizontalOffset: 1
                            color: "#010101"
                            radius: 1
                            samples: 3
                            spread: 0.5
                        }
                    }
                }
            }
        }

        Rectangle{visible: isActive; anchors.fill: parent; color: "transparent"; border.width: borderWidth*2; border.color: highlightColor }

        XsText{
            visible: sec1.isHovered
            text: "Go To Frame: "+startFrameRole
            anchors.centerIn: thumb
            style: Text.Outline
            font.pixelSize: XsStyleSheet.fontSize + 4
            color: highlightColor
        }

        XsPreference {
            id: bookmark_categories_value
            index: globalStoreModel.searchRecursive("/core/bookmark/category", "pathRole")
        }
        XsBookmarkCategories {
            id: bookmark_categories
            value: bookmark_categories_value.value
        }
        Rectangle {
            width: parent.width
            height: itemHeight
            anchors.bottom: parent.bottom
            color: panelColor
            z: -1

            XsComboBox {
                id: noteTypeCombo
                model: bookmark_categories
                width: parent.width
                height: itemHeight - itemSpacing/2
                textRole: "textRole"
                displayText: (categoryRole == "" || categoryRole == undefined) ? "Note Type" : categoryRole
                textColorNormal: popupOptions.opened? palette.text : categoryRole=="-"? "grey" : palette.text

                onActivated: {
                    let ind = model.index(index, 0)
                    let col = model.get(ind, "colourRole")
                    let cat = model.get(ind, "valueRole")
                    if(col == undefined)
                        col = ""
                    categoryRole = cat
                    colourRole = col
                    note_category = cat
                    note_colour = col
                }

                Rectangle {
                    anchors.fill: parent
                    color: colourRole != undefined ? colourRole : transparent
                    opacity: 0.3
                }
            }
        }

    }


}