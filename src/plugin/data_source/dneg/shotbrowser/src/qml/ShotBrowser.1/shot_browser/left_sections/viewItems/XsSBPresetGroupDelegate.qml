// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import ShotBrowser 1.0


MouseArea {
    id: dragArea

    property var delegateModel: null
    property var selectionModel: null
    property var expandedModel: null

    property bool isRunning: queryRunning && presetModelIndex() == currentPresetIndex
    // property bool isActive: isSelected //#TODO
    property bool isExpanded: expandedModel.isSelected(presetModelIndex())
    property bool isSelected: selectionModel.isSelected(presetModelIndex())

    property bool isParent:  true
    property bool isIconVisible: false
    property bool isMouseHovered: groupOnlyMArea.containsMouse || addBtn.hovered || editBtn.hovered || moreBtn.hovered || favBtn.hovered

    property color itemColorActive: palette.highlight
    property color itemColorNormal: XsStyleSheet.widgetBgNormalColor //palette.base

    hoverEnabled: true

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
        let i = presetModelIndex()
        return i.model.get(i,"entityRole")
    }

    onClicked: (mouse) => {
        if(mouse.modifiers == Qt.NoModifier) {
            selectionModel.select(presetModelIndex(), ItemSelectionModel.ClearAndSelect)
        } else if(mouse.modifiers == Qt.ShiftModifier){
            // ShotBrowserHelpers.shiftSelectItem(selectionModel, presetModelIndex())
        } else if(mouse.modifiers == Qt.ControlModifier) {
            // ShotBrowserHelpers.ctrlSelectItem(selectionModel, presetModelIndex())
        }
    }

    // onDoubleClicked:{
    //     openEditPopup()
    // }

    Connections {
        target: expandedModel
        function onSelectionChanged(selected, deselected) {
            isExpanded = expandedModel.isSelected(presetModelIndex())
        }
    }

    Connections {
        target: selectionModel
        function onSelectionChanged(selected, deselected) {
            isSelected = selectionModel.isSelected(presetModelIndex())
        }
    }

    Rectangle{ id: nodeDiv

        property bool isDivSelected: false
        property int slNumber: index+1

        color: isSelected? Qt.darker(palette.highlight, 2): Qt.lighter(palette.base, 1.5) //XsStyleSheet.widgetBgNormalColor

        opacity: hiddenRole ? 0.5 : 1.0

        border.width: 1
        border.color: isMouseHovered? itemColorActive : itemColorNormal

        anchors.fill: parent

        MouseArea{ id: groupOnlyMArea
            anchors.fill: row
            hoverEnabled: true
            propagateComposedEvents: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onClicked: (mouse)=>{

                if (mouse.button == Qt.RightButton) {
                    if(groupMenu.visible) {
                        groupMenu.visible = false
                    }
                    else{
                        showGroupMenu(groupOnlyMArea, mouseX, mouseY)
                    }
                }

                mouse.accepted = false
            }
        }
        RowLayout{ id: row
            spacing: 1
            anchors.fill: parent

            XsPrimaryButton{ id: expandButton
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true

                imgSrc: "qrc:/icons/chevron_right.svg"
                isActive: isExpanded
                enabled: isParent
                opacity: enabled? 1 : 0.5
                imageDiv.rotation: isExpanded? 90 : 0

                onClicked: {
                    let ind = presetModelIndex()
                    // some wierd interaction..
                    // if(presetsTreeModel.isExpanded(index))
                    //     presetsTreeModel.collapseRow(index)
                    // else
                    //     presetsTreeModel.expandRow(index)

                    expandedModel.select(presetModelIndex(), ItemSelectionModel.Toggle)
                    expandedModel.select(ind.model.index(1, 0, ind), ItemSelectionModel.Select)
                }
            }


            XsText{ id: groupNameDiv
                Layout.fillWidth: true
                Layout.minimumWidth: 30
                // Layout.preferredWidth: 100
                Layout.fillHeight: true

                text: nameRole+"..."
                color: Qt.lighter(XsStyleSheet.hintColor, 1.2)
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: XsStyleSheet.fontSize*1.2
                font.weight: isSelected? Font.Bold : Font.Normal
                leftPadding: 5

                // anchors.verticalCenter: parent.verticalCenter
                // anchors.left: parent.left
                // width: parent.width

                elide: Text.ElideRight
            }

            XsText{
                Layout.fillWidth: true
                Layout.fillHeight: true

                Layout.minimumWidth: 15
                Layout.preferredWidth: 50
                Layout.maximumWidth: 50

                visible: !isMouseHovered && !(addBtn.isActive || editBtn.isActive || moreBtn.isActive)

                text: entityRole
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                color: Qt.lighter(XsStyleSheet.panelBgColor, 2.2)
                elide: Text.ElideRight
            }

            Item{
                Layout.preferredWidth: 1
                Layout.fillHeight: true
            }

            XsSecondaryButton{ id: addBtn
                Layout.topMargin: 1
                Layout.bottomMargin: 1
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true

                visible: isMouseHovered || addBtn.isActive || editBtn.isActive

                imgSrc: "qrc:/icons/add.svg"
                scale: 0.95

                onClicked: {
                    let i = ShotBrowserEngine.presetsModel.index(1, 0, presetModelIndex())
                    ShotBrowserEngine.presetsModel.insertPreset(ShotBrowserEngine.presetsModel.rowCount(i), i)
                    let ind = presetModelIndex()
                    expandedModel.select(presetModelIndex(), ItemSelectionModel.Select)
                    expandedModel.select(ind.model.index(1, 0, ind), ItemSelectionModel.Select)
                }
            }

            XsSecondaryButton{ id: editBtn
                Layout.topMargin: 1
                Layout.bottomMargin: 1
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true
                visible: isMouseHovered || editBtn.isActive

                imgSrc: "qrc:///shotbrowser_icons/edit.svg"
                scale: 0.95
                isActive: presetEditPopup.presetIndex == ShotBrowserEngine.presetsModel.index(0, 0, presetModelIndex()) && presetEditPopup.visible

                onClicked: {
                    openEditPopup()
                }
            }

            XsSecondaryButton{ id: moreBtn
                Layout.topMargin: 1
                Layout.bottomMargin: 1
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true
                visible: isMouseHovered || moreBtn.isActive || editBtn.isActive

                imgSrc: "qrc:/icons/more_vert.svg"
                scale: 0.95
                isActive: groupMenu.visible && groupMenu.presetModelIndex == presetModelIndex()
                onClicked:{
                    if(groupMenu.visible) {
                        groupMenu.visible = false
                    }
                    else{
                        showGroupMenu(moreBtn, width/2, height/2)
                    }
                }
            }
            XsSecondaryButton{ id: favBtn
                Layout.topMargin: 1
                Layout.bottomMargin: 1
                Layout.minimumWidth: height
                Layout.maximumWidth: height
                Layout.fillHeight: true

                visible: isMouseHovered || favouriteRole || editBtn.isActive

                showHoverOnActive: favouriteRole && !isMouseHovered
                isColoured: favouriteRole// && isMouseHovered
                imgSrc: "qrc:///shotbrowser_icons/favorite.svg"
                // isActive: favouriteRole
                scale: 0.95
                onClicked: favouriteRole = !favouriteRole
            }
        }
    }



    function openEditPopup(){

        if( presetEditPopup.visible == true && presetEditPopup.entityName == nameRole) {
            presetEditPopup.visible = false
            return
        }

        presetEditPopup.title = "Edit '"+nameRole+"' Group"
        presetEditPopup.entityName = nameRole
        presetEditPopup.entityCategory = "Group"

        if(!presetEditPopup.visible){
            presetEditPopup.x = appWindow.x + appWindow.width/3
            presetEditPopup.y = appWindow.y + appWindow.height/4
        }

        presetEditPopup.visible = true
        presetEditPopup.presetIndex = helpers.makePersistent(ShotBrowserEngine.presetsModel.index(0, 0, presetModelIndex()))
        presetEditPopup.entityType = entityType()
    }


    function showGroupMenu(refitem, xpos, ypos){

        groupMenu.presetModelIndex = presetModelIndex()
        groupMenu.filterModelIndex = filterModelIndex()

        groupMenu.showMenu(
            refitem,
            xpos,
            ypos);

    }

}