import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.12

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    // Note that the model gives use a boolean 'is_checked', plus 'name' and
    // 'hotkey' strings.

    id: widget
    height: XsStyleSheet.menuHeight

    property var menu_model
    property var menu_model_index
    property var parent_menu

    property real leftIconSize: menu_item.leftIconSize

    signal clicked()

    property real minWidth: menu_item.minWidth + XsStyleSheet.menuHeight

    property bool isChecked: false

    function hideSubMenus() {}

    RowLayout {

        spacing: 0
        anchors.fill: parent

        XsMenuItemToggle {

            Layout.fillWidth: true
            id: menu_item
            menu_model: widget.menu_model
            menu_model_index: widget.menu_model_index
            parent_menu: widget.parent_menu
            isChecked: widget.isChecked

            onClicked: {
                widget.clicked()
            }

        }

        XsSecondaryButton{

            id: settingsBtn
            width: height
            Layout.fillHeight: true
            Layout.margins: 4
            imgSrc: "qrc:/icons/settings.svg"
            // isActive:
            onClicked: {
                // isActive = !isActive
                widget.menu_model.nodeActivated(widget.menu_model_index,"settings_button", helpers.contextPanel(widget))
            }
        }
    }

}