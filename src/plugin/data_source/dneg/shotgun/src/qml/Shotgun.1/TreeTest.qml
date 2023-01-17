// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtQml.Models 2.14
import xStudio 1.1


XsDialog {
    id: dlg
    title: "test"

    width: 600
    height: 600

    property var model: null

    ItemSelectionModel {
        id: selectionModel
        model: dlg.model
    }

    TreeNode {
    	id: tree_node
        anchors.fill: parent
        model: dlg.model
        rootIndex:  null
        selection: selectionModel
    }
}

