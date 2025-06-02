// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

XsPrimaryButton{ id: thisItem
    isActiveIndicatorAtLeft: true

    property var delegateModel: null
    property var selectionModel: null
    property string groupName: ""
    property bool isUserPreset: updateRole == undefined

    // opacity: hiddenRole ? 0.5 : 1.0

    property bool isActive: presetModelIndex() == currentPresetIndex
    property bool isSelected: selectionModel.isSelected(presetModelIndex())
    property bool isModified: updateRole != undefined ? updateRole : false
    property bool isRunning: queryRunning && isActive
    property bool itemDragging: isDragging && isSelected
    property int itemDraggingOffset: itemDragging ? draggingOffset : 0

    normalColor:  Qt.darker("#33FFFFFF", XsStyleSheet.darkerFactor)
    bgColorNormal:  Qt.darker("#1AFFFFFF", XsStyleSheet.darkerFactor)

    property int oldY: 0
    property var oldParent: null

    x: height

    onItemDraggingOffsetChanged: {

        if(itemDragging) {
            let offset = itemDraggingOffset

            // calculate offset based on position in selection.
            let ordered = [].concat(selectionModel.selectedIndexes)
            ordered.sort((a,b) => a.row - b.row)
            for(let i=0; i < ordered.length; i++) {
                if(ordered[i] == presetModelIndex())
                    break
                offset += 1
            }

            thisItem.y = draggingY + (offset  * (btnHeight-1)) - parent.contentY + (btnHeight/2)
        }
    }

    onItemDraggingChanged: {
        if(itemDragging) {
            thisItem.x = height * 2
            oldY = mapToItem(thisItem.parent, 0, 0).y
            oldParent = parent
            thisItem.parent = thisItem.parent.parent
        } else if(oldParent) {
            parent = oldParent
            thisItem.y = oldY
            thisItem.x = height
            oldParent = null
        }
    }

    Connections {
        target: selectionModel
        function onSelectionChanged(selected, deselected) {
            isSelected = selectionModel.isSelected(presetModelIndex())
        }
    }

    // TapHandler {
    //     acceptedModifiers: Qt.NoModifier
    //     onSingleTapped: {
    //         let g = mapToGlobal(0,0)
    //         control.tapped(Qt.LeftButton, g.x, g.y, Qt.NoModifier)
    //     }
    // }

    // TapHandler {
    //     acceptedModifiers: Qt.ShiftModifier
    //     onSingleTapped: {
    //         let g = mapToGlobal(0,0)
    //         control.tapped(Qt.LeftButton, g.x, g.y, Qt.ShiftModifier)
    //     }
    // }

    // TapHandler {
    //     acceptedModifiers: Qt.ControlModifier
    //     onSingleTapped: {
    //         let g = mapToGlobal(0,0)
    //         control.tapped(Qt.LeftButton, g.x, g.y, Qt.ControlModifier)
    //     }
    // }

    Item {
        id: dummy
    }

    DragHandler {
        cursorShape: Qt.PointingHandCursor
        xAxis.enabled: false
        target: dummy

        dragThreshold: 5

        onTranslationChanged: {
            let offset = Math.floor(translation.y / (btnHeight-1))
            let row_count = filterModelIndex().model.rowCount(filterModelIndex().parent)

            if(filterModelIndex().row + offset < 0) {
                draggingOffset = -(filterModelIndex().row+1)
            }
            else if(filterModelIndex().row + offset > row_count-1){
                draggingOffset = (row_count - 1 - filterModelIndex().row)
            } else {
                draggingOffset = offset
            }
        }
        onActiveChanged: {
            if(active) {
                // primary drag
                draggingOffset = 0
                draggingY = mapToItem(thisItem.parent, 0, 0).y
                isDragging = true
            } else {
                isDragging = false
                if(draggingOffset) {
                    // need to move stuff..
                    let pis = []
                    let ordered = [].concat(selectionModel.selectedIndexes)
                    ordered.sort((a,b) => a.row - b.row)
                    for(let i = 0; i< ordered.length; i++)
                        pis.push(helpers.makePersistent(ordered[i]))

                    // have list of persistent indexes.
                    let destRow = (presetModelIndex().row+draggingOffset) +1


                    if(draggingOffset < 0) {
                        for(let i = 0;i < pis.length; i++) {
                            ShotBrowserEngine.presetsModel.moveRows(
                                pis[i].parent,
                                pis[i].row,
                                1,
                                pis[i].parent,
                                destRow + i
                            )
                        }
                    } else {
                        for(let i = 0;i < pis.length; i++) {
                            ShotBrowserEngine.presetsModel.moveRows(
                                pis[i].parent,
                                pis[i].row,
                                1,
                                pis[i].parent,
                                destRow
                            )
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: (mouse) => {
            if (mouse.button == Qt.RightButton && selectionModel.isSelected(presetModelIndex())) {
                showPresetMenu(ma, mouseX, mouseY) //btnHeight - is the spacing at left for Presets apart from UIDataModels
            } else {
                if(mouse.modifiers == Qt.NoModifier) {
                    resultViewTitle = groupName+" : "+nameRole
                    selectionModel.select(presetModelIndex(), ItemSelectionModel.ClearAndSelect)
                    activatePreset(presetModelIndex())
                } else if(mouse.modifiers == Qt.ShiftModifier){
                    ShotBrowserHelpers.shiftSelectItem(selectionModel, presetModelIndex())
                } else if(mouse.modifiers == Qt.ControlModifier) {
                    ShotBrowserHelpers.ctrlSelectItem(selectionModel, presetModelIndex())
                }

                if (mouse.button == Qt.RightButton) {
                    showPresetMenu(ma, mouseX, mouseY) //btnHeight - is the spacing at left for Presets apart from UIDataModels
                }
            }
        }
    }

    function presetModelIndex() {
        try {
            let fi = filterModelIndex()
            return fi.model.mapToSource(fi)
        } catch (err) {
            return helpers.qModelIndex()
        }
    }

    function filterModelIndex() {
        return delegateModel.mapRowToModel(index)
    }

    function entityType() {
        let i = presetModelIndex().parent.parent
        return i.model.get(i,"entityRole")
    }

    Rectangle{ id: selectedBgDiv
        anchors.fill: parent
        color: isSelected ? Qt.darker(palette.highlight, 2): "transparent"
        // opacity: 0.6
    }

    Rectangle{ id: activeIndicatorDiv
        anchors.bottom: parent.bottom
        width: borderWidth*9
        height: parent.height
        color: isActive ? bgColorPressed : "transparent"
    }

    RowLayout{
        anchors.fill: parent
        spacing: 1

        XsText{ id: nameDiv
            Layout.fillWidth: true
            Layout.minimumWidth: 50
            Layout.preferredWidth: 100
            Layout.fillHeight: true

            text: nameRole + (isModified ? "*" : "")
            font.weight: isSelected? Font.Bold : Font.Normal
            horizontalAlignment: Text.AlignLeft
            leftPadding: busyIndicator.width //25
            elide: Text.ElideRight
        }

        XsIcon {
            visible: isModified && checksumSystemRole != checksumRole
            opacity: 0.2
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true
            source: "qrc:///icons/upgrade.svg"
            scale: 0.95
        }

        XsSecondaryButton{ id: editBtn
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true

            visible: thisItem.hovered || editBtn.isActive

            imgSrc: "qrc:///shotbrowser_icons/edit.svg"
            isActive: presetEditPopup.presetIndex == presetModelIndex() && presetEditPopup.visible
            scale: 0.95

            onClicked: {
                openEditPopup()
            }
        }

        XsSecondaryButton{ id: moreBtn
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true

            visible: thisItem.hovered || moreBtn.isActive || editBtn.isActive

            imgSrc: "qrc:/icons/more_vert.svg"
            scale: 0.95
            isActive: presetMenu.visible && presetMenu.presetModelIndex == presetModelIndex()
            onClicked:{
                if(presetMenu.visible) {
                    presetMenu.visible = false
                }
                else{
                    showPresetMenu(moreBtn, width/2, height/2)
                }
            }
        }

        XsIcon {
            visible: isUserPreset
            opacity: 0.2
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true
            // height: parent.height
            // imgOverlayColor: toolTipMArea.containsMouse? palette.highlight : XsStyleSheet.secondaryTextColor
            source: "qrc:///icons/person.svg"
            scale: 0.95
        }

        Item {
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true
            visible: !favBtn.visible
        }

        XsSecondaryButton{ id: favBtn
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true

            visible: thisItem.hovered || favouriteRole || editBtn.isActive

            showHoverOnActive: favouriteRole && !thisItem.hovered
            isColoured: favouriteRole// && thisItem.hovered
            imgSrc: "qrc:///shotbrowser_icons/favorite.svg"
            scale: 0.95
            opacity: groupFavouriteRole ? 1.0 : 0.5
            onClicked: favouriteRole = !favouriteRole
        }

        XsSecondaryButton{ id: hiddenBtn
            Layout.topMargin: 1
            Layout.bottomMargin: 1
            Layout.preferredWidth: height
            Layout.fillHeight: true

            visible: prefs.showPresetVisibility

            opacity: parentHiddenRole ? 0.4 : 0.7

            // showHoverOnActive: favouriteRole && !thisItem.hovered
            isColoured: hiddenRole// && thisItem.hovered
            imgSrc: hiddenRole ? "qrc:///icons/visibility_off.svg" : "qrc:///icons/visibility.svg"
            scale: 0.95
            onClicked: hiddenRole = !hiddenRole
        }
    }


    function openEditPopup(){

        if( presetEditPopup.visible == true && presetEditPopup.entityName == nameRole) {
            presetEditPopup.visible = false
            return
        }

        presetEditPopup.title = "Edit '"+nameRole+"' Preset"
        presetEditPopup.entityName = nameRole
        presetEditPopup.entityCategory = "Preset"

        if(!presetEditPopup.visible){
            presetEditPopup.x = appWindow.x + appWindow.width/3
            presetEditPopup.y = appWindow.y + appWindow.height/4
        }

        presetEditPopup.visible = true
        presetEditPopup.presetIndex = helpers.makePersistent(presetModelIndex())
        presetEditPopup.entityType = entityType()
    }

    function showPresetMenu(refItem, xpos, ypos){

        presetMenu.presetModelIndex = presetModelIndex()
        presetMenu.filterModelIndex = filterModelIndex()
        if(!isSelected) {
            selectionModel.select(presetModelIndex(), ItemSelectionModel.Select)
        }

        presetMenu.showMenu(
            refItem,
            xpos,
            ypos);

    }

    XsBusyIndicator{ id: busyIndicator
        x: nameDiv.x + 4
        width: height
        height: parent.height
        running: visible
        visible: isRunning
        scale: 0.5
    }

}

