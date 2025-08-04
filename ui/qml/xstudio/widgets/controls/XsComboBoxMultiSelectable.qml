// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Controls.Basic
import QtQuick.Layouts


import xStudio 1.0


Item{ id: widget

    property color bgColorEditable: "light grey"
    property color bgColorActive: palette.highlight
    property color bgColorNormal: palette.base

    property color textColorActive: palette.text
    property color indicatorColorNormal: "light grey"

    property real borderWidth: 1
    property real borderRadius: 6
    property real framePadding: 6
    property real itemSpacing: 1

    property int displayItemCount: 10

    property alias forcedBg: searchTextField.forcedBg
    property string hint: "multi-select"

    property bool isFiltered: false

    // only show items in the selection model.
    onIsFilteredChanged: {
        if(valuesModel.selectionFilter != undefined) {
            if(isFiltered) {
                valuesModel.selectionFilter = sourceSelectionModel.selection
            } else {
                valuesModel.selectionFilter = empty.selection
            }
        }
    }
    property var valuesModel

    property int valuesCount: valuesModel ? valuesModel.length: 0
    onValuesCountChanged:{
        valuesPopup.currentIndex=-1
    }

    property int checkedCount: sourceSelectionModel.selectedIndexes.length
    property alias checkedIndexes: sourceSelectionModel.selectedIndexes
    property alias theSelection: sourceSelectionModel.selection
    property alias theSelectionModel: sourceSelectionModel


    property alias popup: valuesPopup
    signal open()
    signal close()
    signal clear()

    Connections {
        target: valuesModel ? valuesModel.sourceModel : null
        function onModelReset() {
            clear()
            checkedCount = sourceSelectionModel.selectedIndexes.length
        }
    }


    Rectangle{ id: searchField
        width: parent.width
        height: parent.height
        color: "transparent"

        border.color: bgColorNormal
        border.width: borderWidth

        XsTextField { id: searchTextField
            width: parent.width
            height: widget.height
            placeholderText: hint
            forcedHover: arrowButton.hovered

            onAccepted: {
                focus = false
                if(valuesModel.length>0)
                {
                    if(valuesPopup.currentIndex==-1) valuesPopup.currentIndex=0
                    valuesPopup.focus = true
                }
            }
            onTextEdited: {
                valuesModel.setFilterWildcard(text)
                valuesPopup.visible = true
                searchTextField.focus = true
            }

            onFocusChanged: {
                if(focus)
                {
                    // valuesPopup.currentIndex=-1
                    if(isFiltered) countDisplay.isCountBtnClicked = true
                    else arrowButton.isArrowBtnClicked = true
                    valuesPopup.visible = true
                    searchTextField.focus = true
                }
            }
            Keys.onPressed: (event)=> {
                if (event.key == Qt.Key_Down) {
                    if(valuesModel.length>0) {
                        if(valuesPopup.currentIndex===-1) valuesPopup.currentIndex=0
                        else if(valuesPopup.currentIndex!==-1) valuesPopup.currentIndex+=1
                        valuesPopup.focus = true
                    }
                }
                else if (event.key == Qt.Key_Up) {
                    if(valuesModel.length>0) {
                        if(valuesPopup.currentIndex!==-1) {
                            valuesPopup.currentIndex-=1
                            valuesPopup.focus = true
                        }
                    }
                }
            }

        }
        XsPrimaryButton{ id: arrowButton
            property bool isArrowBtnClicked: false
            text: ""
            imgSrc: "qrc:/icons/arrow_drop_down.svg"
            imageDiv.width: 25
            imageDiv.height: 20
            imageDiv.rotation: isActive? 180: 0
            width: height
            height: widget.height - framePadding
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: searchTextField.right
            anchors.rightMargin: framePadding/2
            isActive: arrowButton.isArrowBtnClicked //valuesPopup.visible

            onClicked: {
                isFiltered = false

                if(arrowButton.isArrowBtnClicked){
                    valuesPopup.visible = false
                    arrowButton.isArrowBtnClicked = false
                }
                else{
                    valuesPopup.visible = true
                    arrowButton.isArrowBtnClicked = true
                }

                countDisplay.isCountBtnClicked = false
            }
        }
        XsPrimaryButton{ id: clearButton
            text: ""
            imgSrc: "qrc:/icons/close.svg"
            visible: searchTextField.length!=0
            width: height
            height: widget.height - framePadding
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: checkedCount>0 && countDisplay.visible? countDisplay.left: arrowButton.left
            anchors.rightMargin: framePadding/2
            onClicked: {
                searchTextField.clear()
                searchTextField.textEdited()
            }
        }
        XsPrimaryButton{ id: countDisplay
            property bool isCountBtnClicked: false
            text: checkedCount
            font.pixelSize: text.length==1? 10 : 9
            visible: checkedCount>0
            width: height
            height: widget.height - framePadding*1.10
            isActive: isCountBtnClicked //isFiltered
            // opacity: 1:0.7
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: arrowButton.left
            anchors.rightMargin: framePadding
            onClicked: {

                if(countDisplay.isCountBtnClicked){
                    valuesPopup.visible = false
                    isFiltered = true

                    countDisplay.isCountBtnClicked = false
                }
                else{
                    isFiltered = true
                    valuesPopup.visible = true

                    countDisplay.isCountBtnClicked = true
                }

                arrowButton.isArrowBtnClicked = false
            }
        }
    }

    Rectangle{ id: valuesPopupBg
        anchors.fill: valuesPopup; color: Qt.darker(bgColorNormal, 1.5); visible: valuesPopup.visible
    }
    ListView{ id: valuesPopup
        z: 10
        property real valuesItemHeight: widget.height/1.3
        model: valuesModel

        Rectangle{ anchors.fill: parent; color: "transparent";
            border.color: Qt.lighter(bgColorNormal, 1.3); border.width: borderWidth
        }

        visible: false
        onVisibleChanged: {
            if(visible) focus=true
        }
        clip: true
        orientation: ListView.Vertical
        snapMode: ListView.SnapToItem
        currentIndex: -1

        x: searchField.x
        y: searchField.y + searchField.height
        width: searchTextField.width
        height: valuesCount>=displayItemCount? valuesItemHeight*displayItemCount: valuesItemHeight*valuesCount
        ScrollBar.vertical: XsScrollBar { id: valuesScrollBar; padding: 2}
        onContentHeightChanged: {
            if(height < contentHeight) valuesScrollBar.policy = ScrollBar.AlwaysOn
            else {
                valuesScrollBar.policy = ScrollBar.AsNeeded
            }
        }

        Keys.onUpPressed: {
            if(currentIndex!=-1) currentIndex-=1

            if(currentIndex==-1) searchTextField.focus=true
        }
        Keys.onDownPressed: {
            if(currentIndex!=model.length-1) currentIndex+=1
        }
        Keys.onPressed: (event)=> {
            if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return) {
                updateField(currentIndex)
            }
        }

        delegate:
        Rectangle{
            width: valuesPopup.width
            height: valuesPopup.valuesItemHeight
            color: checkBox.checked ? Qt.darker(bgColorNormal, 1.1): Qt.darker(bgColorNormal, 1.5)

            XsCheckBox{ id: checkBox
                text: nameRole
                x: 5
                width: parent.width - x
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                checked: sourceSelectionModel.selectedIndexes.includes(valuesModel.mapToSource(valuesModel.index(index, 0)))
                onClicked: updateField(index)

                Connections {
                    target: sourceSelectionModel
                    function onSelectionChanged(unsel,sel) {
                        checkBox.checked = sourceSelectionModel.selectedIndexes.includes(valuesModel.mapToSource(valuesModel.index(index, 0)))
                    }
                }

                onHoveredChanged:{
                    if(hovered) valuesPopup.currentIndex=index
                }

                forcedHover: valuesPopup.currentIndex==index
                indicatorItem.implicitWidth: parent.height/1.2
                indicatorItem.implicitHeight: parent.height/1.2
            }
        }
    }

    function selectFromNames(names=[]) {
        let indexes = []

        for(let i =0;i<names.length;i++)
            indexes.push(valuesModel.sourceModel.search(names[i], "nameRole"))

        sourceSelectionModel.select(helpers.createItemSelection(indexes),  ItemSelectionModel.ClearAndSelect)
    }

    function updateField(index)
    {
        valuesPopup.currentIndex=index

        if(index!==-1) sourceSelectionModel.select(valuesModel.mapToSource(valuesModel.index(index, 0)), ItemSelectionModel.Toggle)
        if(!checkedCount) {
            countDisplay.isCountBtnClicked = false
            arrowButton.isArrowBtnClicked = true
            isFiltered=false
        }
    }

    onOpen: {
        countDisplay.isCountBtnClicked = false
        arrowButton.isArrowBtnClicked = true
        isFiltered = false

        searchTextField.focus = true
        valuesPopup.visible = true
    }

    onClose: {
        valuesPopup.visible = false
        searchTextField.focus = false

        countDisplay.isCountBtnClicked = false
        arrowButton.isArrowBtnClicked = false
        isFiltered = false
    }

    onClear: {
        sourceSelectionModel.clearSelection()

        valuesPopup.visible = false

        searchTextField.clear()
    }

    ItemSelectionModel {
        id: sourceSelectionModel
        model: valuesModel  ? valuesModel.sourceModel : null
        onSelectionChanged: {
            // this is mank..
            checkedCount = sourceSelectionModel.selectedIndexes.length
            if(isFiltered) {
                valuesModel.selectionFilter = sourceSelectionModel.selection
            }
        }
    }
    ItemSelectionModel {
        id: empty
        model: valuesModel
    }


}