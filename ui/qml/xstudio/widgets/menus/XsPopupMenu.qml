import QtQuick



import xStudio 1.0
import xstudio.qml.models 1.0

XsMenu {

    id: popup_menu
    menu_model: filteredMenuModel
    property var menu_model_index: filteredMenuModel.index(-1, -1)

    property string menu_model_name

    // Here we declare the menu model. Doing this creates an empty JsonTree
    // in the backend with 'modelDataName' as its unqie identifier (so in this
    // case "shotgun menu bar" is the ID.... or, if the model with the
    // given modelDataName already exists, then we connect to it.

    // The JsonTree desrcibes menu(s) and sub-menus - we can add to it and
    // modfiy it from QML or the backend - we just need the ID to get to it.

    // N.B. - this XsPanelMenuModelFilter MUST be declared before 'the_model'
    // that drives it. Otherwise, at clean-up time, the_model is destroyed and
    // it triggers an obscure bug in Qt.
    XsPanelMenuModelFilter {
        id: filteredMenuModel
        sourceModel: the_model
        panelAddress: helpers.contextPanelAddress(popup_menu)
    }

    XsMenusModel {
        id: the_model
        modelDataName: menu_model_name
        onJsonChanged: {
            menu_model_index = filteredMenuModel.index(-1, -1)
        }
    }

    function showMenu(refItem, refX, refY, altRightEdge) {

        // this function should be defined at the parent Window level
        repositionPopupMenu(popup_menu, refItem, refX, refY, altRightEdge)
    }

}

