// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xStudio 1.1


XsDialog {
    id: dlg
    title: "test"

    width: 600
    height: 600

    property alias model: tree_node.model

    TreeNode {
    	id: tree_node
        anchors.fill: parent
        model: null
        rootIndex:  null
    }
}

