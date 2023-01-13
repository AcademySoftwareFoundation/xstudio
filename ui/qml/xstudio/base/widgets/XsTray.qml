// SPDX-License-Identifier: Apache-2.0
// Comprised of 2 pieces:
// 1. HEADER (Rect + Text)
// 2. ListView (driven by Models) that uses Delegates to display a row of buttons

import QtQuick 2.12
import QtQml.Models 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import xStudio 1.0

Rectangle {
    // default property alias data: grid.data
    implicitWidth:  layout.implicitWidth
    implicitHeight: layout.implicitHeight

    default property alias data: layout.data
    property bool collapsed: false
    property bool resizable: false
    property string text: "test"
    objectName: "tray"
    color: "transparent"
    property alias layout: layout

    /*onCollapsedChanged: {
        if(collapsed) {
            implicitWidth = XsStyle.transportHeight * 2
            layout.opacity = 0
            popupid.visible = true
        } else {
            implicitWidth = layout.implicitWidth
            layout.opacity = 1
            popupid.visible = false
        }
    }*/

    Behavior on implicitWidth {
        NumberAnimation {
            duration: 200
        }
    }

    /*XsTrayButton {
        id: popupid
        text: parent.text
        visible: false
        checkable: false
        tooltip: "Open " + parent.tooltip + " popup menu."
        anchors.fill: parent
        font.pixelSize: XsStyle.popupControlFontSize
        onClicked: {
            collapsePopup.open();
        }
    }*/

    onWidthChanged: {
        if(resizable) {
            var count = 0
            var size = width
            for (var index in layout.children) {
                if("collapseMode" in layout.children[index]){
                    count = count + 1
                } else {
                    size  = size - layout.children[index].width
                }
            }

            size = size / count
            var collapse_all = false;
            for (var index in layout.children) {
                var curr_but = layout.children[index]
                if ("collapseMode" in curr_but && curr_but.width !== 1) {
                     var cmm = curr_but.getModeFromSize(Math.floor(size));
                     if (cmm == 3) {
                        collapse_all = true;
                        break;
                     }
                }
            }

            for (var index in layout.children) {
                var curr_but = layout.children[index]
                if ("collapseMode" in curr_but && curr_but.width !== 1) {
                    if (collapse_all) {
                        curr_but.collapseMode = 3;
                    } else {
                        curr_but.collapseMode = curr_but.getModeFromSize(Math.floor(size));
                    }
                }
            }
        }
    }

    GridLayout {
        rows: 1
        id: layout
        rowSpacing: 0
        columnSpacing: 0
        anchors.fill: parent
        visible: opacity != 0
        opacity: 1

        Behavior on opacity {
            NumberAnimation {duration: 200}
        }
    }

    Component.onCompleted: {
        popupItems.clear()
        for (var index in layout.children) {
            var curr_but = layout.children[index]
            if (curr_but.objectName === "tray_button") {
                popupItems.append({"icon": curr_but.icon.source.toString(), "label": curr_but.text, "remote": curr_but})
            }
        }
    }

    ListModel {
        id:popupItems
    }

    XsMenu {
        x: parent.x
        y: parent.y - height

        id:collapsePopup
        title: parent.text
        Instantiator {
            model: popupItems
            XsMenuItem {
                mytext: model.label
                myicon: model.icon
                onTriggered: { model.remote.toggle() }
            }
            onObjectAdded: {
                collapsePopup.insertItem(index, object)
            }
            onObjectRemoved: collapsePopup.removeItem(object)
        }
    }
}

