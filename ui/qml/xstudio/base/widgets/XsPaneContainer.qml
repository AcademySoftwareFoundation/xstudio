// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.12

import xStudio 1.0

Rectangle {

    id: booboo
    color: "transparent"
    clip: true
    property string title: ""
    property bool has_header: true
    property bool header_visible: true

    property bool hidden: false
    property bool has_borders: true

    default property alias content: rowLayout.children

    property alias container_header: container_header

    function hide() {
        hidden = true
    }

    function unhide() {
        hidden = false
    }

    Rectangle {

        color: "transparent"//XsStyle.controlBackground
        anchors.fill: parent

        ListModel {
            id: corners
            ListElement {
                rr: 0
                idx: 0
            }
            ListElement {
                rr: 270
                idx: 1
            }
            ListElement {
                rr: 90
                idx: 2
            }
            ListElement {
                rr: 180
                idx: 3
            }

        }

        Repeater {
            model: corners

            Rectangle {

                property var rad: 6
                width: rad
                height: rad
                anchors.top: idx < 2 ? undefined : parent.top
                anchors.bottom: idx < 2 ? parent.bottom : undefined
                anchors.left: idx == 1 || idx == 3 ? undefined : parent.left
                anchors.right: idx == 0 || idx == 2 ? undefined : parent.right
                z: 200
                layer.enabled: true
                layer.samples: 16
                visible: has_borders

                transform: Rotation { origin.x: rad*0.5; origin.y: rad*0.5; angle: rr}

                color: "transparent"
                Shape {
    
                    ShapePath {
                        strokeColor: "transparent"
                        fillColor: XsStyle.mainBackground
                        startX: 0; startY: 0.5
                        PathLine { x: rad-rad*Math.cos(0.314); y: rad*Math.sin(0.314)+0.5 }
                        PathLine { x: rad-rad*Math.cos(0.314*2); y: rad*Math.sin(0.314*2)+0.5 }
                        PathLine { x: rad-rad*Math.cos(0.314*3); y: rad*Math.sin(0.314*3)+0.5 }
                        PathLine { x: rad-rad*Math.cos(0.314*4); y: rad*Math.sin(0.314*4)+0.5 }
                        PathLine { x: rad; y: rad+0.5 }
                        PathLine { x: 0; y: rad+0.5 }
                    }
    
                    ShapePath {
                        strokeWidth: 1
                        strokeColor: XsStyle.controlBackground
                        fillColor: "transparent"
                        startX: 0; startY: 0.5
                        PathLine { x: rad-rad*Math.cos(0.314); y: rad*Math.sin(0.314)+0.5 }
                        PathLine { x: rad-rad*Math.cos(0.314*2); y: rad*Math.sin(0.314*2)+0.5 }
                        PathLine { x: rad-rad*Math.cos(0.314*3); y: rad*Math.sin(0.314*3)+0.5 }
                        PathLine { x: rad-rad*Math.cos(0.314*4); y: rad*Math.sin(0.314*4)+0.5 }
                        PathLine { x: rad; y: rad+0.5 }
                    }
    
                }
            }
        }
        

        Rectangle {

            anchors.fill: parent

            color: "transparent"
            z: 100

            Rectangle {

                id: header_box
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: XsStyle.panelHeaderHeight*opacity
                color: XsStyle.mainBackground
                z: 100
                visible: has_header && opacity != 0
                opacity: header_visible ? 1.0 : 0.0

                Behavior on opacity {
                    NumberAnimation {duration: 200}
                }

                Rectangle {
                    id: container_header
                    color: "transparent"
                    anchors.fill: parent
                }
            }

            RowLayout {
                id: rowLayout
                anchors.top: header_box.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }

            Rectangle {
                id:title_divider
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: header_box.bottom
                //border.width: 2
                //border.color: "red"//XsStyle.controlBackground
                height: 1
                color: XsStyle.controlBackground
                //z: 100
                visible: has_header

            }

            
        }

        Rectangle {

            id: borderRect
            anchors.fill: parent
            border.width: 1
            border.color: XsStyle.controlBackground
            color: "transparent"
            z: 100
            visible: has_borders
            height: 1

        }
    }

    property string header_component: ""
    
    Component.onCompleted: {

        if (header_component) {
            var component = Qt.createComponent(header_component);
            if (component.status == Component.Ready) {
                var r = component.createObject(
                    container_header,
                    {}
                );
                r.z = 1000
            } else {
                // Error Handling
                console.log("Error loading component:", component.errorString());
            }
        }
    }
}