import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Popup {

    id: the_popup
    height: view.height+ (topPadding+bottomPadding)
    width: view.width
    topPadding: XsStyleSheet.menuPadding
    bottomPadding: XsStyleSheet.menuPadding
    leftPadding: 0
    rightPadding: 0

    property var menu_model
    property var menu_model_index

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: "#EE444444" //bgColorNormal

    background: Rectangle{
        implicitWidth: 100
        implicitHeight: 200
        gradient: Gradient {
            GradientStop { position: 0.0; color: forcedBgColorNormal==bgColorNormal?"#33FFFFFF":"#EE222222" }
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

            // setting up the model and rootIndex like this causes us to
            // iterate over the children of the node in the 'menu_model' that
            // is found at 'menu_model_index'. Note that the delegate will
            // be assigned a value 'index' which is its index in the list of
            // children.
            model: the_popup.menu_model
            rootIndex: the_popup.menu_model_index
            delegate: chooser

            DelegateChooser {

                id: chooser
                role: "menu_item_type"
            
                DelegateChoice {
                    roleValue: "button"
                    
                    XsMenuItemNew { 
                        // again, we pass in the model to the menu item and
                        // step one level deeper into the tree by useing row=index
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(
                            index, // row = child index
                            0, // column = 0 (always, we don't use columns)
                            the_popup.menu_model_index // the parent index into the model
                        )
                        parent_menu: the_popup
                    }
                }
            
                DelegateChoice {
                    roleValue: "menu"
                    
                    XsMenuItemNew { 
                        // again, we pass in the model to the menu item and
                        // step one level deeper into the tree by useing row=index
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(
                            index, // row = child index
                            0, // column = 0 (always, we don't use columns)
                            the_popup.menu_model_index // the parent index into the model
                        )
                        parent_menu: the_popup
                    }
                }

                DelegateChoice {
                    roleValue: "divider"
                    
                    XsMenuDivider {}

                }

                DelegateChoice {
                    roleValue: "choice"
                    
                    XsMenuItemNew { 
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)
                    }
                    
                }

                DelegateChoice {
                    roleValue: "multichoice"
                    
                    XsMenuItemNew { 
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)
                    }
                    
                }

                DelegateChoice {
                    roleValue: "toggle"
                    
                    XsMenuItemToggle { 
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)
                        onChecked:{
                            isChecked = !isChecked
                        }
                    }

                }

            }
        }

    }
}


