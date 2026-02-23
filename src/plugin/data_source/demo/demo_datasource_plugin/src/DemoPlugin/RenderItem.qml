// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.qmlmodels
import QtQuick.Effects

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0

// this import allows us to use the DemoPluginDatamodel
import demoplugin.qml 1.0

Item {

    id: render_item
    width: layout.width
    height: layout.height
    property var model_index
    property bool selected: assetSelectionModel.isSelected(model_index)

    // we need this connection to update 'selected' when the selection model
    // is updated elsewhere
    Connections {
        target: assetSelectionModel
        function onSelectedIndexesChanged() {
            selected = assetSelectionModel.selectedIndexes.indexOf(model_index) != -1
        }
    }

    RowLayout {
        
        id: layout
        spacing: 0
        Repeater {

            model: DelegateModel {

                model: columns_model

                delegate: chooser

                DelegateChooser {
                    id: chooser
                    role: "columnType"

                    DelegateChoice {
                        roleValue: "text"

                        Item {
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: columnWidth
                            Rectangle {
                                y: parent.height-1
                                height: 1
                                width: parent.width
                                color: XsStyleSheet.menuDividerColor
                            }
                            Rectangle {
                                height: parent.height
                                width: 1
                                color: XsStyleSheet.menuDividerColor
                            }
                            Text {
                                anchors.fill: parent
                                // the 'roleName' value provided by columns_model can be used
                                // to look up the corresponding dataVale from dataModel
                                // property var value: assetObject[roleName]
                                text: versionsModel.data(model_index, roleName)
                                color: palette.text
                                font.pixelSize: XsStyleSheet.fontSize
                                font.family: XsStyleSheet.fontFamily
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 12

                                // hack! We need this so when the user changes status from the
                                // menu the view is refreshed. I still haven't worked how to
                                // 'link' to role data when using dynamic columns that show
                                // specific role data
                                property var foo: status
                                onFooChanged: {
                                    text = versionsModel.data(model_index, roleName)
                                }
                            }
                        }
                    }

                    DelegateChoice {
                        roleValue: "icon"

                        Item {
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: columnWidth
                            Rectangle {
                                y: parent.height-1
                                height: 1
                                width: parent.width
                                color: XsStyleSheet.menuDividerColor
                            }
                            Rectangle {
                                height: parent.height
                                width: 1
                                color: XsStyleSheet.menuDividerColor
                            }
                            XsImage {
                                id: img
                                anchors.fill: parent
                                property var path: media_path // role data from our model
                                property var imgOverlayColor
                                onPathChanged: {

                                    if (version_type == "OTIO") {
                                        source = "qrc:///icons/movie.svg"
                                        imgOverlayColor = XsStyleSheet.primaryTextColor
                                    } else {
                                        // for our demo, the URL encodes the frame range of 
                                        // the source. We use this to choose the frame number
                                        // that we want for the thumbnail
                                        const myRe = new RegExp("\\.([0-9]+)\\-([0-9]+)\\.", "g");
                                        var myArray = myRe.exec(path);
                                        var inpt = parseInt(myArray[1])
                                        var outpt = parseInt(myArray[2])

                                        // xSTUDIO's thumbnail manager will load a thumbnail for them
                                        // URL following image://thumbnail/ and followed by "@<frame_number>"
                                        source = "image://thumbnail/" + media_path + "@" + Math.round((inpt+outpt)/2)
                                    }
                                }

                                layer {
                                    enabled: imgOverlayColor != undefined
                                    effect: 
                                    MultiEffect {
                                        source: img
                                        brightness: 1.0
                                        colorization: 1.0
                                        colorizationColor: img.imgOverlayColor
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }
        Rectangle {
            Layout.preferredHeight: 30
            width: 1
            color: XsStyleSheet.menuDividerColor
        }

    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        onPressed: (mouse) => {
            select(mouse.modifiers == Qt.ControlModifier, mouse.modifiers == Qt.ShiftModifier)
        }
        onDoubleClicked: {
            add_selection_to_playlist(currentMediaContainerIndex.valid ? false : true, undefined, true)
        }
    }

    // mouse over highlight
    Rectangle {
        anchors.fill: parent
        opacity: 0.8
        color: "transparent"
        border.color: palette.highlight
        visible: ma.containsMouse
        z: 100
    }

    // selection highlight
    Rectangle {
        anchors.fill: parent
        opacity: 0.4
        color: palette.highlight
        visible: selected
    }

    // divide line between rows
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: gridLineColour
    }

    function select(ctrl, shift) {
        if (shift) {

            // Rather awkward hand rolled logic for SHIT+Select

            // Find nearest item that is already selected
            var dist = 10000
            var nearest = undefined
            for (var i = 0; i < assetSelectionModel.selectedIndexes.length; ++i) {
                var idx = assetSelectionModel.selectedIndexes[i]
                if (idx != model_index) {
                    var d = Math.abs(idx.row-model_index.row)
                    if (d < dist) {
                        nearest = idx
                        dist = d
                    }
                }
            }

            if (nearest) {
                if (nearest.row < model_index.row) {
                    for (var r = nearest.row+1; r <= model_index.row; ++r) {
                        assetSelectionModel.select(model_index.model.index(r, 0), ItemSelectionModel.Select)
                    }
                } else {
                    for (var r = model_index.row; r < nearest.row; ++r) {
                        assetSelectionModel.select(model_index.model.index(r, 0), ItemSelectionModel.Select)
                    }
                }
            } else {
                assetSelectionModel.select(model_index, ItemSelectionModel.Toggle)
            }

        } else if (ctrl) {
            assetSelectionModel.select(model_index, ItemSelectionModel.Toggle)
        } else if (!selected) {
            assetSelectionModel.select(model_index, ItemSelectionModel.ClearAndSelect)
        }
    }

}