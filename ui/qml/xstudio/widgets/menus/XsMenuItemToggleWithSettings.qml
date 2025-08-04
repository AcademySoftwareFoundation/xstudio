import QtQuick

import QtQuick.Layouts




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
            Layout.preferredWidth: height
            Layout.fillHeight: true
            Layout.margins: 4
            imgSrc: "qrc:/icons/settings.svg"
            onClicked: {
                widget.menu_model.nodeActivated(widget.menu_model_index,"settings_button", helpers.contextPanel(widget))
            }
        }
    }

}