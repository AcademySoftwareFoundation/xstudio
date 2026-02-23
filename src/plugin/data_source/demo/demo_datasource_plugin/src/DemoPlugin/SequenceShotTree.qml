// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item {

    id: tree
    Layout.fillWidth: true
    Layout.fillHeight: true
    property int production_index: projects_list.indexOf(current_project)
    property var rootIndex: dataModel.index(production_index, 0)
    onProduction_indexChanged: {
        rootIndex = dataModel.index(production_index, 0)
    }

    // If the data in dataModel has been re-set (or initialised the first time)
    // we need to evaluate rootIndex again
    Connections {
        target: dataModel
        function onModelReset() {
            rootIndex = dataModel.index(projects_list.indexOf(current_project), 0)
        }
    }

    DelegateModel {

        id: delegateModel
        model: dataModel

        // setting the rootIndex here we point to the Nth row ... at the lowest
        // level each row corresponds to one of our 'productions'
        rootIndex: tree.rootIndex
        delegate: SequenceItem {
            data_model: dataModel
            model_index: dataModel.index(index, 0, rootIndex)
        }
    }

    ColumnLayout {

        anchors.fill: parent

        Rectangle {

            Layout.fillWidth: true
            Layout.preferredHeight: 40
            border.color: XsStyleSheet.menuDividerColor
            border.width: 1

            gradient: Gradient {
                GradientStop { position: 0.0; color: XsStyleSheet.panelBgColor }
                GradientStop { position: 0.33; color: XsStyleSheet.panelBgFlatColor }
                GradientStop { position: 1.0; color: XsStyleSheet.panelBgColor }
            }

            XsText {
                anchors.centerIn: parent
                text: "Sequence & Shot Tree"
            }

        }

        // This list view shows a bunch of SequenceItem items as driven by the 
        // rows in delegateModel
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: view
            model: delegateModel
            clip: true

            // this forces the ListView to build up to 1024 items that are in the
            // view but off screen. It stops the scroll bar playing up as otherwise
            // it doesn't know the total Y dimension of the contents as it assumes
            // offscreen items are the same height as the ones on-screen
            cacheBuffer: 1024

            CustomScrollBar {        
                target: view
            }    

        }
    }



}
