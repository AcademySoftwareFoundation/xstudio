// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

import xStudio 1.0

import "./widgets"
import "./delegates"

Rectangle{ id: header
    width: parent.width
    height: XsStyleSheet.widgetStdHeight

    color: XsStyleSheet.widgetBgNormalColor

    property bool isSomeColumnResizedByDrag: false
    property alias model: repeater.model

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
            // visible: false
            Layout.preferredWidth: 20
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
                    "title": "Frame Range"
                }
                columns_root_model.insertRowsData(
                    columns_root_model.length-1,
                    1,
                    columns_root_model.index(-1, -1),
                    new_column)
            }


        }

    }

}