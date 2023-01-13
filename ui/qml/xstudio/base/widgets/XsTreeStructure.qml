// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1
import xstudio.qml.clipboard 1.0

ListView{ id: treeView
    property bool isEditable: false
    property bool isIconVisible: false
    property int menuActionIndex: -1

    property alias scrollBar: scrollBar
    property alias delegateModel: delegateModel

    width: parent.width - x*2
    height: parent.height - y*2
    spacing: itemSpacing/2
    clip: true
    orientation: ListView.Vertical
    interactive: true
    // snapMode: ListView.SnapToItem
    currentIndex: -1

    ScrollBar.vertical: XsScrollBar {id: scrollBar; visible: treeView.height < treeView.contentHeight}

    model: DelegateModel { id: delegateModel

        function remove(row) {
            model.removeRows(row, 1, rootIndex)
        }

        function move(src_row, dst_row) {
            // invert logic if moving down

            if(dst_row > src_row) {
                model.moveRows(rootIndex, dst_row, 1, rootIndex, src_row)
            } else {
                model.moveRows(rootIndex, src_row, 1, rootIndex, dst_row)
            }
        }

        function insert(row, data) {
            model.insert(row, rootIndex, data)
        }

        function append(data, root=rootIndex) {
            model.insert(rowCount(root), root, data)
        }

        function rowCount(root=rootIndex) {
            return model.rowCount(root)
        }

        function get(row, role) {
            return model.get(row, rootIndex, role)
        }

        function set(row, value, role) {
            return model.set(row, value, role, rootIndex)
        }

    }

}