// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

Rectangle{
    id: thisItem

    color: "transparent"// XsStyleSheet.widgetBgNormalColor

    property var delegateModel: null
    property var termModel: []
    property var termValueRole: valueRole
    // property var isSelected: false
    property bool isSelected: termSelection.isSelected(helpers.makePersistent(delegateModel.notifyModel.mapRowToModel(index)))

    onTermValueRoleChanged: {
        if(valueBox.count && valueBox.currentText != valueRole) {
            let i = valueBox.find(valueRole)
            if(i != -1)
                valueBox.currentIndex = i
            else {
                valueBox.editText = valueRole
            }
        }
    }

    Connections {
        target: termSelection
        function onSelectionChanged(selected, deselected) {
            isSelected = termSelection.isSelected(helpers.makePersistent(delegateModel.notifyModel.mapRowToModel(index)))
        }
    }

    function setTermValue(value) {
        valueRole = value
    }

    Item{ id: rowItems
        anchors.fill: parent

        Rectangle{ id: selectedBgDiv
            anchors.fill: parent
            color: isSelected ? Qt.darker(palette.highlight, 2): "transparent"
            // opacity: 0.6
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: (height+1) * delegateModel.notifyModel.depthAtRow(index)
            spacing: 1

            opacity: enabledRole && parentEnabledRole ? 1.0 : 0.5

            MouseArea {
                Layout.minimumWidth: 9
                Layout.maximumWidth: 9
                Layout.fillHeight: true

                cursorShape: Qt.PointingHandCursor

                onClicked: (mouse)=> {
                    if( mouse.modifiers == Qt.NoModifier ) {
                        termSelection.select(helpers.makePersistent(delegateModel.notifyModel.mapRowToModel(index)), ItemSelectionModel.ClearAndSelect)
                    } else if(mouse.modifiers == Qt.ShiftModifier ) {
                        if(termSelection.selectedIndexes.length) {
                            // find last selected entry ?
                            let m = termSelection.selectedIndexes[termSelection.selectedIndexes.length-1]
                            let s = Math.min(delegateModel.notifyModel.mapRowToModel(index).row, m.row)
                            let e = Math.max(delegateModel.notifyModel.mapRowToModel(index).row, m.row)
                            let items = []

                            for(let i=s; i<=e; i++) {
                                items.push(termSelection.model.index(i, 0, delegateModel.notifyModel.mapRowToModel(index).parent))
                            }
                            termSelection.select(helpers.createItemSelection(items), ItemSelectionModel.ClearAndSelect)
                        } else {
                            termSelection.select(helpers.makePersistent(delegateModel.notifyModel.mapRowToModel(index)), ItemSelectionModel.ClearAndSelect)
                        }
                    } else if(mouse.modifiers == Qt.ControlModifier ) {
                        termSelection.select(helpers.makePersistent(delegateModel.notifyModel.mapRowToModel(index)), ItemSelectionModel.Toggle)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: isSelected ? palette.highlight : XsStyleSheet.widgetBgNormalColor
                }
            }

            XsPrimaryButton { id: checkBox
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true
                isActive: enabledRole
                onClicked: enabledRole = !enabledRole
                imgSrc: enabledRole ? "qrc:/icons/check.svg" : ""
                isActiveViaIndicator: true
                activeIndicator.visible: false
            }

            XsPrimaryButton{ id: expandButton
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true

                imgSrc: "qrc:/icons/chevron_right.svg"
                visible: termRole == "Operator"
                isActive: {let i = delegateModel.notifyModel.isExpanded(index) ? true:false;i}
                imageDiv.rotation: isActive ? 90 : 0

                onClicked: {
                    if(isActive)
                        delegateModel.notifyModel.collapseRow(index)
                    else
                        delegateModel.notifyModel.expandRow(index)
                    isActive = !isActive
                }
            }

            XsPrimaryButton{ id: insertButton
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true
                imgSrc: "qrc:/icons/add.svg"
                isActiveViaIndicator: false
                visible: termRole == "Operator"
                onClicked: {
                    termTypeMenu.termModelIndex = helpers.makePersistent(delegateModel.notifyModel.mapRowToModel(index))
                    termTypeMenu.showMenu(
                        insertButton,
                        width/2,
                        height/2);

                    if(expandButton.isActive == false)
                        expandButton.clicked()
                }
            }

            XsComboBox {
                // Layout.fillWidth: true
                Layout.preferredWidth: termWidth - (termRole == "Operator" ? (height+1)*2 : 0)
                Layout.fillHeight: true
                model: termModel

                currentIndex: this.count ? find(termRole) : -1

                onActivated: (aindex) => {
                    if(termRole != textAt(aindex)) {
                        let ti = thisItem.delegateModel.notifyModel.mapRowToModel(index)
                        let r = ti.row+1

                        ShotBrowserEngine.presetsModel.insertTerm(textAt(aindex), ti.row, ti.parent)
                        ShotBrowserEngine.presetsModel.removeRows(r, 1, ti.parent)
                    }
                }
            }

            XsPrimaryButton{ id: equationBtn
                property bool isLiveLink: livelinkRole != undefined && livelinkRole
                property bool isNegate: negatedRole != undefined && negatedRole
                property bool isEqual: !isLiveLink && !isNegate
                property bool equalMenuEnabled: !isEqual
                Layout.preferredWidth: height
                Layout.fillHeight: true

                // anchors.fill: parent
                imgSrc:
                    isLiveLink? "qrc:/icons/link.svg" :
                    isNegate? "qrc:/shotbrowser_icons/exclamation.svg" :
                    "qrc:/shotbrowser_icons/equal.svg"
                isActiveViaIndicator: false
                isActive: !isEqual
                onClicked:{
                    if(equationMenu.visible)
                        equationMenu.visible = false
                    else {
                        equationMenu.termModelIndex = delegateModel.notifyModel.mapRowToModel(index)
                        equationMenu.showMenu(
                            equationBtn,
                            width/2,
                            height/2);
                    }
                }
            }

            XsComboBoxEditable {
                id: valueBox
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: btnWidth*4.2

                model: ShotBrowserEngine.presetsModel.termModel(termRole, entityType, projectId)
                enabled: !livelinkRole
                textRole: "nameRole"
                currentIndex: -1

                onCountChanged: {
                    if(count && currentText != termValueRole) {
                        let i = find(termValueRole)
                        if(i != -1) {
                            currentIndex = i
                        }
                        else {
                            // inject value into model.
                            model.insertRowsData(0,1, model.index(-1,-1), {"name": termValueRole})
                            currentIndex = 0
                        }
                    }
                }

                onAccepted: {
                    if (find(editText) === -1) {
                        setTermValue(editText)
                        model.insertRowsData(0, 1, model.index(-1,-1), {"name": editText})
                        currentIndex = 0
                    } else {
                        currentIndex = find(editText)
                        setTermValue(textAt(currentIndex))
                    }
                    focus = false
                }

                onActivated: (aindex) => {
                    setTermValue(textAt(aindex))
                }
            }
            XsPrimaryButton{
                Layout.preferredWidth: closeWidth
                Layout.fillHeight: true
                imgSrc: "qrc:/icons/close.svg"
                onClicked: {
                    // map to real model..
                    let i = delegateModel.model.mapRowToModel(index)
                    delegateModel.model.model.removeRows(i.row, 1, i.parent)
                }
            }

            XsPrimaryButton{ id: moreBtn
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true

                imgSrc: "qrc:/icons/more_vert.svg"
                // scale: 0.95
                isActive: termMenu.visible && isSelected
                onClicked:{
                    if(termMenu.visible) {
                        termMenu.visible = false
                    }
                    else{
                        if(!isSelected)
                            termSelection.select(delegateModel.notifyModel.mapRowToModel(index), ItemSelectionModel.ClearAndSelect)
                        termMenu.showMenu(moreBtn, width/2, height/2)
                    }
                }
            }
        }
    }
}

