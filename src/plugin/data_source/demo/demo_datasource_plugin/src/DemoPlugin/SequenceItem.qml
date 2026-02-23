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

TreeBaseItem {

    id: sequence_item
    text: sequence ? sequence : ""

    childDelegate: 
        ShotItem {
            data_model: sequence_item.data_model
            model_index: dataModel.index(index, 0, sequence_item.model_index)
        }
    
    /*DelegateModel {

        id: delegateModel
        model: data_model
        rootIndex: model_index
        delegate: ShotItem {
            data_model: sequence_item.data_model
            model_index: dataModel.index(index, 0, sequence_item.model_index)
        }
    }

    // the top-level 'view' into our data.
    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: expanded ? contentHeight : 0
        Layout.leftMargin: indent
        model: delegateModel
        visible: expanded
    }*/

}