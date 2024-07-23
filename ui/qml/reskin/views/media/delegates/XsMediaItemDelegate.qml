// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle {

    id: contentDiv
    width: parent.width; 
    height: parent.height

    color: "transparent"
    property color highlightColor: palette.highlight
    property color bgColorPressed: XsStyleSheet.widgetBgNormalColor
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: highlightColor

    property color hintColor: XsStyleSheet.hintColor
    property color errorColor: XsStyleSheet.errorColor

    property bool isSelected: mediaSelectionModel.selectedIndexes.includes(media_item_model_index)

    property var selectionIndex: mediaSelectionModel.selectedIndexes.indexOf(media_item_model_index)+1

    property bool isMissing: false 
    property bool isActive: false
    property real panelPadding: XsStyleSheet.panelPadding
    property real itemPadding: XsStyleSheet.panelPadding/2

    property real headerThumbWidth: 1

    // property real rowHeight:  XsStyleSheet.widgetStdHeight
    property real itemHeight: (rowHeight-8) //16 

    signal activated() //#TODO: for testing only
    
    //font.pixelSize: textSize
    //font.family: textFont
    //hoverEnabled: true
    opacity: enabled ? 1.0 : 0.33

    property var columns_model

    Item {

        anchors.fill: parent

        Rectangle{ id: rowDividerLine
            width: parent.width; height: headerThumbWidth
            color: bgColorPressed
            anchors.bottom: parent.bottom
        }

        RowLayout{ 
            
            id: row
            spacing: 0 
            height: rowHeight
            anchors.verticalCenter: parent.verticalCenter

            Repeater {

                // Note: columns_model is set-up in the ui_qml.json preference
                // file. Look for 'media_list_columns_config' item in that
                // file. It specifies the title, size, data_type and so-on for
                // each column in the media list view. The DelegateChooser
                // here creates graphics/text items that go into the media list
                // table depedning on the 'data_type'. To add new ways to view
                // data like traffic lights, icons and so-on create a new 
                // indicator class with a new correspondinf 'data_type' in the
                // ui_qml.json
                model: columns_model
                delegate: chooser

                DelegateChooser {

                    id: chooser
                    role: "data_type" 
                
                    DelegateChoice {
                        roleValue: "flag"                        
                        XsMediaFlagIndicator{ 
                            Layout.preferredWidth: size
                            Layout.minimumHeight: itemHeight
                        }
                    }
    
                    DelegateChoice {
                        roleValue: "metadata"                        
                        XsMediaTextItem { 
                            raw_text: metadataFieldValues ? metadataFieldValues[index] : ""
                            Layout.preferredWidth: size
                            Layout.minimumHeight: itemHeight
                        }
                    }

                    DelegateChoice {
                        roleValue: "role_data"                        
                        XsMediaTextItem { 
                            Layout.preferredWidth: size
                            Layout.minimumHeight: itemHeight
                            raw_text: {
                                var result = ""
                                if (object == "MediaSource") {
                                    let image_source_idx = media_item_model_index.model.search_recursive(
                                        media_item_model_index.model.get(media_item_model_index, "imageActorUuidRole"),
                                        "actorUuidRole",
                                        media_item_model_index)
                                    result = media_item_model_index.model.get(image_source_idx, role_name)
                                } else if (object == "Media") {
                                    result = media_item_model_index.model.get(media_item_model_index, role_name)
                                }
                                return "" + result;
                            }
                        }
                    }

                    DelegateChoice {
                        roleValue: "index"                        
                        XsMediaTextItem {
                            text: selectionIndex ? selectionIndex : ""
                            Layout.preferredWidth: size
                            Layout.minimumHeight: itemHeight
                        }
                    }

                    DelegateChoice {
                        roleValue: "notes"                        
                        XsMediaNotesIndicator{ 
                            Layout.preferredWidth: size
                            Layout.minimumHeight: itemHeight
                        }
                    }

                    DelegateChoice {
                        roleValue: "thumbnail"                        
                        XsMediaThumbnailImage { 
                            Layout.preferredWidth: size
                            Layout.minimumHeight: itemHeight
                        }
                    }

                }

            }
        }
    }

    //background:
    Rectangle {
        id: bgDiv
        anchors.fill: parent
        border.color: contentDiv.down || contentDiv.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: contentDiv.down || isSelected ? bgColorPressed : forcedBgColorNormal
        
        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: contentDiv.activeFocus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }

        // Rectangle{anchors.fill: parent; color: "grey"; opacity:(index%2==0?.2:0)}
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: {
            if (mouse.modifiers == Qt.ControlModifier) {

                toggleSelection() 
                    
            } else if (mouse.modifiers == Qt.ShiftModifier) {

                inclusiveSelect()

            } else {

                exclusiveSelect()            

            }
        }

    }


    function toggleSelection() {

        if (!(mediaSelectionModel.selection.count == 1 && 
            mediaSelectionModel.selection[0] == media_item_model_index)) {
            mediaSelectionModel.select(
                media_item_model_index,
                ItemSelectionModel.Toggle
                )
            }

    }

    function exclusiveSelect() {

        mediaSelectionModel.select(
            media_item_model_index,
            ItemSelectionModel.ClearAndSelect
            | ItemSelectionModel.setCurrentIndex
            )

    }

    function inclusiveSelect() {

        // For Shift key and select find the nearest selected row, 
        // select items between that row and the row of THIS item
        var row = media_item_model_index.row
        var d = 10000
        var nearest_row = -1
        var selection = mediaSelectionModel.selectedIndexes

        for (var i = 0; i < selection.length; ++i) {
            var delta = Math.abs(selection[i].row-row)
            if (delta < d) {
                d = delta
                nearest_row = selection[i].row
            }
        }

        if (nearest_row!=-1) {

            var model = media_item_model_index.model
            var first = Math.min(row, nearest_row)
            var last = Math.max(row, nearest_row)

            for (var i = first; i <= last; ++i) {

                mediaSelectionModel.select(
                    model.index(
                        i,
                        media_item_model_index.column,
                        media_item_model_index.parent
                        ),
                    ItemSelectionModel.Select
                    )
            }
        }    
    }


    // onClicked: {
    //     isSelected = true
    // }
    /*onDoubleClicked: {
        isSelected = true
        activated() //#TODO
    }
    onPressed: {
        mediaSelectionModel.select(media_item_model_index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.setCurrentIndex)
    }
    onReleased: {
        focus = false
    }
    onPressAndHold: {
        isMissing = !isMissing
    }*/
    
}