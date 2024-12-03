import QtQuick 2.12

Item {

    id: parent_item
    width: dynamic_widget ? dynamic_widget.width : 0
    height: dynamic_widget ? dynamic_widget.height : 0

    property var dynamic_widget

    // Each HUD item is provided via a backend Attribute - the attributes
    // provide 'qml_code' role data that contains the actual QML for the
    // visual element of the given HUD.
    property var code_: qml_code

    // The 'value' role data of the HUD attribute is a boolean that controls
    // its visiblity
    visible: hud_visible ? value != undefined ? value : false : false

    // Only instance the overlay item if it is visible

    onVisibleChanged: {
        if (visible && !dynamic_widget && typeof code_ == "string") {
            dynamic_widget = Qt.createQmlObject(qml_code, parent_item)
        }
    }

    onCode_Changed: {
        if (typeof code_ == "string" && visible) {
            dynamic_widget = Qt.createQmlObject(qml_code, parent_item)
        }
    }
}