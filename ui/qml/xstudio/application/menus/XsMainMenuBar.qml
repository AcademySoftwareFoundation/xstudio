import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

import "./file_menu/"
import "./help_menu/"

XsMenuBar {

    id: menu_bar
    height: XsStyleSheet.menuHeight

    menu_model_name: "main menu bar"

    XsFileMenu {}
    XsSnippetMenu {}
    XsHelpMenu {}

}