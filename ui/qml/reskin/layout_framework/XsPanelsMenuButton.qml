
import QtQuick 2.15
import QtQuick.Controls 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle { 
    
    id: hamBtn
    // background: Rectangle{color:"red"}
    width: 15*1.7 
    height: width*1.1
    z: 1000
    property string panel_id

    onPanel_idChanged: {
        hamburgerMenu.menu_model_index = panels_menus_model.index(-1, -1)
    }

    MouseArea {
        
        anchors.fill: parent
        onClicked: {
            hamburgerMenu.x = x-hamburgerMenu.width
            hamburgerMenu.visible = true
        }
    }

    Image{
        width: parent.width-6
        height: parent.height-6
        anchors.centerIn: parent
        source: "qrc:///assets/icons/menu.svg"
    }

    // this gives us access to the global tree model that defines menus, 
    // sub-menus and menu items
    XsMenusModel {
        id: panels_menus_model
        modelDataName: panel_id

        onJsonChanged: {
            hamburgerMenu.menu_model_index = index(-1, -1)
        }
    }

    XsMenuNew { 
        id: hamburgerMenu
        visible: false
        menu_model: panels_menus_model
        menu_model_index: panels_menus_model.index(-1, -1)
    }


}

