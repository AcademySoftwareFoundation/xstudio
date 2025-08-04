// SPDX-License-Identifier: Apache-2.0
import QtQuick




import QtQuick.Layouts

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

import xStudio 1.0

import "./widgets"
import "./delegates"
import "./data_indicators"

Rectangle{ id: header
    width: parent.width
    height: XsStyleSheet.widgetStdHeight

    color: XsStyleSheet.widgetBgNormalColor

    property bool isSomeColumnResizedByDrag: false
    property alias model: repeater.model
    property alias barWidth: titleBar.width

    property alias columns_model: columns_model

    Component {
        id: configureDialog
        XsMediaListConfigureDialog {
            onVisibleChanged: {
                if (!visible) {
                    loader.sourceComponent = undefined
                }
            }
        }
    }

    Loader {
        id: loader
    }

    function configure(index) {
        loader.sourceComponent = configureDialog
        loader.item.model_index = index
        loader.item.visible = true
    }

    DelegateModel {

        id: columns_model
        model: columns_root_model

        delegate: XsMediaHeaderColumn{

            Layout.preferredWidth: size ? size : 20
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight

            titleBarTotalWidth: titleBar.width

            onHeaderColumnResizing:{
                if(isDragged) {
                    isSomeColumnResizedByDrag = true
                }
                else {
                    isSomeColumnResizedByDrag = false
                }
            }

        }
    }


    Component.onCompleted: {

    }

    property var metadataPaths: []

    RowLayout{ id: titleBar
        // width: parent.width
        height: parent.height
        spacing: 0

        Repeater{
            id: repeater
            model: columns_model
        }

        // For adding a new tab
        XsSecondaryButton{

            id: addBtn
            Layout.preferredWidth: 25
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            z: 1
            imgSrc: "qrc:/icons/add.svg"

            onClicked: {
                dialogHelpers.multiChoiceDialog(
                    addColumn,
                    "Add Column",
                    "Do you want to add a column to the media list?",
                    ["Add Column", "Cancel"],
                    undefined)

            }

            function addColumn(button) {
                if (button != "Add Column") return
                var new_column = {
                    "data_type": "media_standard_details",
                    "info_key": "Frame Range - MediaSource (Image)",
                    "resizable": true,
                    "size": 120,
                    "sortable": false,
                    "title": "New Column"
                }
                var r = columns_root_model.rowCount()
                columns_root_model.insertRowsSync(
                    r,
                    1,
                    columns_root_model.index(-1, -1))                
                columns_root_model.set(columns_root_model.index(r,0), new_column, "jsonRole")
                
            }


        }

    }

    // Expandable empty item
    Item{
        height: parent.height
        anchors.left: titleBar.right
        anchors.right: parent.right
    }


}