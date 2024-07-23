// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

XsPopup {

    // Note that the model gives use a string 'current_choice', plus 
    // a list of strings 'choices'

    id: the_popup
    height: view.height+ (topPadding+bottomPadding)
    width: view.width

    property var menu_model
    property var menu_model_index

    // N.B. the XsMenuModel exposes role data called 'choices' which lists the
    // items in the multi choice. The active option is exposed via 'current_choice'
    // role data
    // Multi-choice menus can also be made via the XsModuleData model. This
    // exposes a role data 'combo_box_options' fir the items in the multi-choice.
    // The active option is exposed via 'value' role data
    property var _choices: typeof choices !== "undefined" ? choices : typeof combo_box_options !== "undefined" ? combo_box_options : []
    property var _currentChoice: typeof current_choice !== "undefined" ? current_choice : typeof value !== "undefined" ? value : ""

    ListView {

        id: view
        orientation: ListView.Vertical
        spacing: 0
        width: 150 //contentWidth
        height: contentHeight
        contentHeight: contentItem.childrenRect.height
        contentWidth: contentItem.childrenRect.width
        snapMode: ListView.SnapToItem
        // currentIndex: -1

        model: DelegateModel {           

            model: _choices

            delegate: XsMenuItemToggle{

                // isRadioButton: true
                // radioSelectedChoice: current_choice
                // label: choices[index]
                
                // onChecked: {
                //     label= choices[index]
                //     current_choice = label
                //     radioSelectedChoice = current_choice
                // }

                isRadioButton: true
                radioSelectedChoice: _currentChoice
                onClicked:{
                    if (typeof current_choice!== "undefined") {
                        current_choice = name
                    } else if (value) {
                        value = name
                    }
                }

                parent_menu: the_popup
                parentWidth: view.width

                property var name: _choices[index]

                // Component.onCompleted: {
                //     label =  choices[index]
                //     // is_checked = current_choice == label
                // }
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


