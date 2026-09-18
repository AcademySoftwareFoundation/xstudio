// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Qt.labs.qmlmodels

import xStudio 1.0

import ".."
import "."

Item {

    id: itemGroup
    property var thumbsModel
    property var thumbsModelIndex
    property var depth: 0

    //Layout.preferredHeight: num_thumbnail_rows*130 + header.height + 10
    implicitHeight: header.height + layout.height//num_expanded_subitems*XsStyleSheet.widgetStdHeight + header.height
    //visible: (depth == 0 || is_visible)

    property var yInFlick: y+parentYInFlick
    property var parentYInFlick: 0

    FSListItem {
        id: header
        width: parent.width
        visible: depth != 0
        height: depth != 0 ? XsStyleSheet.widgetStdHeight : 0
        modelIndex: thumbsModelIndex
    }

    DelegateModel {
        id: delegateModel
        model: thumbsModel
        rootIndex: thumbsModelIndex
        delegate: chooser
    }

    DelegateChooser {
        id: chooser
        role: "is_folder"
        DelegateChoice {
            roleValue: false
            FSListItem {
                Layout.fillWidth: true
                modelIndex: thumbsModel.index(index, 0 , thumbsModelIndex)
            }
        }
        DelegateChoice {
            roleValue: true
            Item {
                id: container
                width: itemGroup.width
                Layout.fillWidth: true
                property var group: undefined
                implicitHeight: group ? group.implicitHeight : 0
                Component.onCompleted: {
                    let component = Qt.createComponent("./FSDirectoryGroup.qml")
                    if (component.status == Component.Ready) {
                        group = component.createObject(
                            container,
                            {
                                width: parent.width,
                                parentYInFlick: itemGroup.yInFlick,
                                depth: itemGroup.depth + 1,
                                thumbsModel: itemGroup.thumbsModel,
                                thumbsModelIndex: thumbsModel.index(index, 0, itemGroup.thumbsModelIndex)
                            })
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.top: header.bottom
        width: parent.width
        id: layout
        spacing: 0
        Repeater {
            id: thumbRepeater
            model: (depth == 0 || is_expanded) ? delegateModel : undefined
        }
    }

    function getItemAtIndex(index, depth) {
        let p = index
        let d = depth
        while (d) {
            p = p.parent
            d--
        }
        if (depth == 1) return thumbRepeater.itemAt(p.row)
        return thumbRepeater.itemAt(p.row).getItemAtIndex(index, depth-1)
    }
    
} // delegate