
import QtQuick 2.15
import QtQuick.Controls 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0

XsSecondaryButton { 
    
    id: hamBtn
    // background: Rectangle{color:"red"}
    width: 15*1.7 
    height: width*1.1
    z: 1000

    imgSrc: "qrc:/icons/menu.svg"
    smooth: true
    antialiasing: true

    property string panelId
    onPanel_idChanged: {
        hamburgerMenu.menu_model_index = panels_menus_model.index(-1, -1)
    }

    onClicked: {
        hamburgerMenu.x = -hamburgerMenu.width
        hamburgerMenu.visible = true
    }
    // MouseArea {
    //     anchors.fill: parent
    //     onClicked: {
    //         hamburgerMenu.x = x-hamburgerMenu.width
    //         hamburgerMenu.visible = true
    //     }
    // }

    // this gives us access to the global tree model that defines menus, 
    // sub-menus and menu items
    XsMenusModel {
        id: panels_menus_model
        modelDataName: panelId

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

