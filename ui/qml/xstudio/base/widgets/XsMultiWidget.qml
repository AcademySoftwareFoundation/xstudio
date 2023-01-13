// SPDX-License-Identifier: Apache-2.0
// one widget to rule them all.

import QtQuick 2.12
import QtQml.Models 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import xStudio 1.0

Rectangle {
	id: multi
    property string text: ""
    property string tooltip: ""
    property int selected: 0

    default property alias data: layout.data

    implicitHeight: layout.implicitHeight
    implicitWidth: layout.implicitWidth

    color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.transportBackground
    gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient

    property alias mouseArea: mouseArea

    ListModel {
        id:popupItems
    }

    GridLayout {
        rows: 1
        id: layout
        rowSpacing: 0
        columnSpacing: 0
        anchors.fill: parent
        visible: true
        opacity: 1

        Behavior on opacity {
            NumberAnimation {duration: 200}
        }
    }

    Component.onCompleted: {
        popupItems.clear()
        for (var index in layout.children) {
            var curr_but = layout.children[index]
            if (curr_but.label) {
                popupItems.append({"icon": (curr_but.icon ? curr_but.icon.source.toString() : ""), "label": curr_but.label, "index": index})
            }
        }
    }

    XsMenu {
        id:collapsePopup
        title: parent.text

        y: parent.y - height

    	hasCheckable: true
	    ActionGroup{
	        id: actgrp
	    }

        Instantiator {
            model: popupItems
            XsMenuItem {
                mytext: model.label
                myicon: model.icon
                mycheckable: true
                mychecked: multi.selected == model.index
                onTriggered: { multi.selected = model.index }
                actiongroup: actgrp
            }
            onObjectAdded: {
                collapsePopup.insertItem(index, object)
            }
            onObjectRemoved: collapsePopup.removeItem(object)
        }
    }

    MouseArea {
        id: mouseArea
        hoverEnabled: true
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            collapsePopup.open();
        }

        onHoveredChanged: {
            if (tooltip) {
                if (mouseArea.containsMouse) {
                    status_bar.normalMessage(tooltip, text)
                } else {
                    status_bar.clearMessage()
                }
            }
        }
    }
}
