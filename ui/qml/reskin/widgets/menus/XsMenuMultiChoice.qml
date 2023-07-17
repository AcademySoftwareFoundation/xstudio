import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Popup {

    // Note that the model gives use a string 'current_choice', plus 
    // a list of strings 'choices'

    id: the_popup
    height: view.height+ (topPadding+bottomPadding)
    width: view.width
    topPadding: XsStyleSheet.menuPadding
    bottomPadding: XsStyleSheet.menuPadding
    leftPadding: 0
    rightPadding: 0

    property var menu_model
    property var menu_model_index

    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: "#EE444444" //bgColorNormal

    background: Rectangle{
        implicitWidth: 100
        implicitHeight: 200
        gradient: Gradient {
            GradientStop { position: 0.0; color: forcedBgColorNormal==bgColorNormal?"#33FFFFFF":"#EE222222"}
            GradientStop { position: 1.0; color: forcedBgColorNormal }
        }
    }

    ListView {

        id: view
        orientation: ListView.Vertical
        spacing: 0
        width: contentWidth
        height: contentHeight
        contentHeight: contentItem.childrenRect.height
        contentWidth: contentItem.childrenRect.width
        snapMode: ListView.SnapToItem

        model: DelegateModel {           

            model: choices

            delegate: XsMenuItemToggle{

                radioSelectedChoice: current_choice
                isRadioButton: true
                label: choices[index]
                
                onChecked: {
                    current_choice = label
                }

            }

            // delegate:
            // Rectangle { 
                
            //     width: label_metrics.width + checkbox.width
            //     height: 30
            //     id: menu_item
            //     property var name: choices[index]
                        
            //     Rectangle {
            //         id: checkbox
            //         anchors.bottom: parent.bottom
            //         anchors.top: parent.top
            //         anchors.left: parent.left
            //         width: height
            //         color: "transparent"
            //         border.color: "black"
            
            //         Text {
            //             anchors.centerIn: parent
            //             text: name == current_choice ? "x" : ""
            //         }
            //     }
            
            //     Text {
            //         id: label
            //         anchors.bottom: parent.bottom
            //         anchors.top: parent.top
            //         anchors.left: checkbox.right
            //         text: name
            //     }
            
            //     TextMetrics {
            //         id:     label_metrics
            //         font:   label.font
            //         text:   label.text
            //     }
            
            //     MouseArea {
            //         anchors.fill: parent
            //         onClicked: {
            //             current_choice = name
            //         }
            //     }
            
            // }

        }

    }
}


