import QtQuick




import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    id: widget

    property var dynamic_widget
    property real minWidth: dynamic_widget ? dynamic_widget.minWidth : 0
    height: dynamic_widget ? dynamic_widget.height : 0

    onHeightChanged: {
        if (is_in_bar && dynamic_widget) dynamic_widget.height = height
    }

    onWidthChanged: {
        if (is_in_bar) dynamic_widget.width = width
    }

    width: minWidth

    property var custom_menu_qml_: custom_menu_qml ? custom_menu_qml : null

    onCustom_menu_qml_Changed: {
        dynamic_widget = Qt.createQmlObject(custom_menu_qml_, widget)
        dynamic_widget.parent_menu = parent_menu
    }

    property real leftIconSize: 0

    property var parent_menu
    property var menu_model
    property var menu_model_index

    property bool is_in_bar: false

    function hideSubMenus() {
        if (dynamic_widget && dynamic_widget.hideSubMenus != undefined) {
            dynamic_widget.hideSubMenus()
        }
    }

    MouseArea
    {
        id: menuMouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
    }
}