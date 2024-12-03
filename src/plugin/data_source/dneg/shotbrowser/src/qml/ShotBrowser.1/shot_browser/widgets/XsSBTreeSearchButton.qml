// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

Item{
    id: button

    property bool isExpanded: false
    property var model: null

    property color textColorActive: "white"
    property color textColorNormal: "light grey"
    property color itemColorActive: palette.highlight
    property string hint: ""

    signal indexSelected(index: var)

    function selectionMade(index) {
        indexSelected(index)
        isExpanded = false
    }

    onIsExpandedChanged: {
        if(isExpanded) {
            searchField.forceActiveFocus()
            searchPop.open()
        } else {
            searchPop.close()
            searchField.clearSearch()
            searchField.focus = false
        }
    }

    XsPrimaryButton{ id: searchBtn
        width: XsStyleSheet.primaryButtonStdWidth
        height: parent.height
        imgSrc: "qrc:/icons/search.svg"
        text: "Search"
        isActive: isExpanded

        onClicked: isExpanded = !isExpanded
    }


    Item{
        id: widget
        visible: isExpanded
        width: parent.width - searchBtn.width
        height: parent.height // + combo.popupOptions.height
        anchors.left: searchBtn.right

        XsSearchBar { id: searchField
            width: parent.width
            height: parent.height

            placeholderText: width? "Search...":""
            forcedHover: clearButton.hovered

            onAccepted: {
                if(searchList.currentIndex!==-1) {
                    selectionMade(sequenceModelDelegate.modelIndex(searchList.currentIndex))
                    model.setFilterWildcard("")
                    text = ""
                }
            }

            onTextEdited: {
                model.filterCaseSensitivity =  Qt.CaseInsensitive
                model.setFilterWildcard(text)
                searchList.currentIndex = 0
            }

            Keys.onPressed: (event)=> {
                if (event.key == Qt.Key_Down) {
                    if(sequenceModelDelegate.count>0) {
                        if(searchList.currentIndex===-1) searchList.currentIndex=0
                        else if(searchList.currentIndex!==-1) searchList.currentIndex+=1
                    }
                }
                else if (event.key == Qt.Key_Up) {
                    if(sequenceModelDelegate.count>0 && searchList.currentIndex>-1)
                        searchList.currentIndex-=1
                }
            }
        }
        XsSecondaryButton{
            id: clearButton
            imgSrc: "qrc:/icons/close.svg"
            visible: searchField.length !== 0
            width: visible? 16 : 0
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 6
            onClicked: {
                clearSearch()
            }
        }

    }

    function clearSearch()
    {
        searchField.textEdited()
        searchField.clear()
    }

    DelegateModel {
        id: sequenceModelDelegate
        model: button.model

        delegate: Rectangle {
            property bool isHovered: mouseArea.containsMouse
            property var modelDelegate: sequenceModelDelegate

            width: searchList.width-2
            height: btnHeight/1.3
            color: searchList.currentIndex==index? palette.highlight : Qt.darker(palette.base, 1.5)

            XsText{
                text: nameRole
                color: searchList.currentIndex==index? textColorActive: textColorNormal
                elide: Text.ElideRight
                width: parent.width - 2
                height: parent.height
                ToolTip.text: text
                ToolTip.visible: mouseArea.containsMouse && truncated
            }

            MouseArea{id: mouseArea; anchors.fill: parent; hoverEnabled: true
                onEntered: searchList.currentIndex = index
                onClicked: selectionMade(modelDelegate.modelIndex(index))
            }
        }
    }

    Popup {
        id: searchPop

        width: Math.max(searchField.width, 250)
        height: Math.min(16, Math.max(5, sequenceModelDelegate.count+2)) * searchItemHeight
        x: widget.x + searchField.x
        y: widget.y + searchField.y + searchField.height

        property real searchItemHeight: btnHeight/1.3

        closePolicy: searchBtn.hovered ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onClosed: button.isExpanded = false

        ListView {
            id: searchList
            clip:true

            anchors.fill: parent
            anchors.margins: 1

            model: sequenceModelDelegate

            orientation: ListView.Vertical
            snapMode: ListView.SnapToItem
            currentIndex: -1

            ScrollBar.vertical: XsScrollBar { id: searchScrollBar; padding: 2}

            onContentHeightChanged: {
                if(height < contentHeight) searchScrollBar.policy = ScrollBar.AlwaysOn
                else searchScrollBar.policy = ScrollBar.AsNeeded
            }
        }

        background: Rectangle{
            anchors.fill: parent
            color: Qt.darker(palette.base, 1.5)
            border.color: Qt.lighter(XsStyleSheet.panelTabColor, 1.3)
            border.width: 1
        }
    }
}