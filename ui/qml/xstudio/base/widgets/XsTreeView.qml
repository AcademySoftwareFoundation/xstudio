// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle

import xStudio 1.1


TreeView {
    // alternatingRowColors: false
    // backgroundVisible: false
    // headerVisible: false
    // sortIndicatorVisible: true
    selectionMode: SelectionMode.MultiSelection 

    TableViewColumn {
        title: "Name"
        role: "nameRole"
        width: parent.width
    }
}

// ListView {
//     property alias model: model
    


//     id: widget
//     anchors.fill: parent
//     anchors.margins: 1

//     model: treeModel
//     selectionEnabled: true

//     contentItem: Row {
//         spacing: 10

//         Rectangle {
//             width: parent.height * 0.6
//             height: width
//             radius: width
//             y: width / 3
//             color: currentRow.hasChildren ? "tomato" : "lightcoral"
//         }

//         Text {
//             verticalAlignment: Text.AlignVCenter

//             color: currentRow.isSelectedIndex ? widget.selectedItemColor : widget.color
//             text: currentRow.currentData
//             font: widget.font
//         }
//     }

//     handle: Item {
//         width: 20
//         height: 20
//         Rectangle {
//             anchors.centerIn: parent
//             width: 10
//             height: 2
//             color: "black"
//             visible: currentRow.hasChildren

//             Rectangle {
//                 anchors.centerIn: parent
//                 width: parent.height
//                 height: parent.width
//                 color: parent.color
//                 visible: parent.visible && !currentRow.expanded
//             }
//         }
//     }

//     highlight: Item {

//         Rectangle {
//             color: "pink"
//             width: parent.width * 0.9
//             height: parent.height
//             anchors.left: parent.left
//             radius: 20
//         }
        
//         Rectangle {
//             color: "pink"
//             width: parent.width * 0.2
//             height: parent.height
//             anchors.right: parent.right
//             radius: 20
//         }
        
//         Behavior on y { NumberAnimation { duration: 150 }}
//     }
// }

