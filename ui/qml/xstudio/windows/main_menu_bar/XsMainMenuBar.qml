import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

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