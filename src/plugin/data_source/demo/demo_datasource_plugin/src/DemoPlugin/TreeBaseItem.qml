// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0

// this import allows us to use the DemoPluginDatamodel
import demoplugin.qml 1.0

Item {

    width: layout.implicitWidth
    height: layout.implicitHeight
    id: base
    property var data_model
    property var model_index
    property string text
    property bool expanded
    property bool expandable: true

    property bool selected: demoSelectionModel.isSelected(model_index)

    // we need this connection to update 'selected' when the selection model
    // is updated elsewhere
    Connections {
        target: demoSelectionModel
        function onSelectedIndexesChanged() {
            selected = demoSelectionModel.selectedIndexes.indexOf(model_index) != -1
        }
    }

    ColumnLayout {

        id: layout
        spacing: 0

        RowLayout {
            id: row_layout
            Layout.fillWidth: true
            spacing: 0

            // Here's an example of a simple, hand-rolled button
            MouseArea {
                Layout.fillHeight: true
                Layout.preferredWidth: expandable ? height : 0
                hoverEnabled: true
                onClicked: expanded = !expanded
                visible: expandable
                XsIcon {
                    anchors.fill: parent
                    rotation: expanded ? 90 : 0
                    source: "qrc:/icons/chevron_right.svg"
                    imgOverlayColor: (parent.containsMouse && index ? palette.highlight : XsStyleSheet.primaryTextColor)
                    opacity: index ? 1.0 : 0.5
                }
            }

            Text {

                text: base.text
                Layout.preferredHeight: 30
                color: palette.text
                font.pixelSize: XsStyleSheet.fontSize
                font.family: XsStyleSheet.fontFamily
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                leftPadding: 4

                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: (mouse) => {
                        select(mouse.modifiers == Qt.ControlModifier)
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

            }
        }

        property alias dm: delegateModel.delegate

        DelegateModel {

            id: delegateModel
            model: data_model
            rootIndex: model_index
        }

        // the top-level 'view' into our data.
        ListView {
            Layout.fillWidth: true
            Layout.leftMargin: indent
            Layout.preferredHeight: expanded ? contentHeight : 0
            model: delegateModel
            visible: expanded
        }

    }

    property alias childDelegate: layout.dm

    function select(ctrl) {
        if (ctrl) {
            demoSelectionModel.select(model_index, ItemSelectionModel.Toggle)
        } else if (!selected) {
            demoSelectionModel.select(model_index, ItemSelectionModel.ClearAndSelect)
        }
    }

}