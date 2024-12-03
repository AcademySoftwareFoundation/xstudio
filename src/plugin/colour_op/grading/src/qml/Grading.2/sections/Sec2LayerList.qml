// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import Grading 2.0

Item{ id: listDiv

    property alias bookmarkList: bookmarkList

    Rectangle{
        anchors.fill: parent
        color: panelColor

        XsListView { id: bookmarkList
            width: parent.width - x*2
            height: parent.height - y
            x: itemSpacing
            y: itemSpacing
            model: bookmarkFilterModel
            spacing: itemSpacing

            property bool userSelect: false
            property var lastSelected: (new Map())

            ScrollBar.vertical: XsScrollBar {
                visible: bookmarkList.height < bookmarkList.contentHeight
            }

            onCurrentIndexChanged: {
                if(userSelect) {
                    userSelect = false
                } else if (currentIndex < 0) {
                    attrs.grading_bookmark = helpers.QUuidToQString("00000000-0000-0000-0000-000000000000")
                } else if (currentIndex == 0 && bookmarkFilterModel.length) {
                    if(currentIndex != bookmarkFilterModel.length) {
                        if(lastSelected.get(bookmarkFilterModel.currentMedia)  != undefined) {
                            var index = bookmarkFilterModel.sourceModel.search(
                                helpers.QVariantFromUuidString(lastSelected.get(bookmarkFilterModel.currentMedia)), "uuidRole")
                            if (index.valid) {
                                var row = bookmarkFilterModel.mapFromSource(index).row
                                if (row >= 0 && row < bookmarkList.count && row != currentIndex) {
                                    userSelect = true
                                    currentIndex = row
                                }
                            }
                        }
                    }
                }
            }

            onCurrentItemChanged: {
                if (currentItem) {
                    var backendUuid = helpers.QVariantFromUuidString(attrs.grading_bookmark)
                    var selectedUuid = currentItem.uuid
                    lastSelected.set(bookmarkFilterModel.currentMedia, selectedUuid)

                    if (backendUuid != selectedUuid && selectedUuid) {
                        attrs.grading_bookmark = helpers.QUuidToQString(selectedUuid)
                    }
                }
            }
            onCountChanged: {
                var index = bookmarkFilterModel.sourceModel.search(
                    helpers.QVariantFromUuidString(attrs.grading_bookmark), "uuidRole")
                if (index.valid) {
                    var row = bookmarkFilterModel.mapFromSource(index).row
                    if (row >= 0 && row < bookmarkList.count && row != currentIndex) {
                        currentIndex = row
                    }
                }
            }

            delegate: XsPrimaryButton { id: bookmark
                width: bookmarkList.width
                height: btnHeight * 1.1

                isActive: isSelected
                isActiveIndicatorAtLeft: true
                activeIndicator.width: (1*3) * 3

                property var uuid: uuidRole

                readonly property bool isSelected: index == ListView.view.currentIndex
                property bool isHovered: hovered

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: (mouse) => {
                        if (mouse.button == Qt.LeftButton){
                            bookmarkList.userSelect = true
                            bookmarkList.currentIndex = index
                        }
                        else if(mouse.button == Qt.RightButton){
                            if(moreMenu.visible) moreMenu.visible = false
                            else{
                                moreMenu.x = x + width
                                moreMenu.y = y + height
                                moreMenu.visible = true
                            }
                        }
                    }
                }

                RowLayout{
                    spacing: 0
                    anchors.fill: parent

                    XsText{ id: nameDiv
                        Layout.fillWidth: true
                        Layout.minimumWidth: 50
                        Layout.preferredWidth: 100
                        Layout.fillHeight: true

                        text: layerUIName()
                        font.weight: isSelected? Font.Bold : Font.Normal
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: bookmark.activeIndicator.width + 5
                        elide: Text.ElideRight

                        function layerUIName() {
                            var name = "";
                            if (userDataRole.layer_name) {
                                name = userDataRole.layer_name
                                if (startFrameRole == endFrameRole) {
                                    name += " - Frame " + startFrameRole
                                }
                            } else {
                                name = "Grade Layer " + (index+1)
                            }

                            return name
                        }
                    }
                    Item{ id: maskDiv
                        Layout.preferredWidth: height * 1.2
                        Layout.fillHeight: true

                        XsPrimaryButton{ id: maskBtn
                            width: parent.width - 1
                            height: parent.height - 4
                            anchors.verticalCenter: parent.verticalCenter

                            isActiveViaIndicator: false
                            isActive: false
                            visible: userDataRole.mask_active
                            imgSrc: "qrc:/grading_icons/mask_domino.svg"
                            text: "Mask Active"
                            scale: 0.95
                        }
                    }
                    Item{ id: visibilityDiv
                        Layout.preferredWidth: height * 1.2
                        Layout.fillHeight: true

                        XsPrimaryButton{ id: visibilityBtn
                            width: parent.width - 1
                            height: parent.height - 4
                            anchors.verticalCenter: parent.verticalCenter

                            isActive: !userDataRole.grade_active
                            imgSrc: userDataRole.grade_active? "qrc:/icons/visibility.svg" : "qrc:/icons/visibility_off.svg"
                            isActiveViaIndicator: false
                            text: "Visibility"
                            scale: 0.95

                            onClicked: {
                                var tmp = userDataRole
                                tmp.grade_active = !tmp.grade_active
                                userDataRole = tmp
                            }
                        }
                    }
                }
            }
        }
    }
}