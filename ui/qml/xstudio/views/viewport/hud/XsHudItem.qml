import QtQuick
import xStudio 1.0
import xstudio.qml.models 1.0

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

    XsAttributeValue {
        id: __media_item_hud_data
        attributeTitle: title
        model: hud_data_attrs
    }

    // For regular viewports, __media_item_hud_data value is set through the
    // backend module attributes system: The python HUD plugin gets a callback
    // when the on-screen media changes. It returns JSON data for doing overlay
    // graphics relating to that media item. The HUD Plugin base class uses that
    // json to set the data on an attribute that we connect to using 
    // __media_item_hud_data above. The trouble is, this is asynchronous and the
    // attribute can be one video frame refresh behind the viewport image. This
    // is not a problem during playback or normal viewport as it refreshes almost
    // immediately. For offscreen rendering to do video output, though, we need
    // the HUD graphics to be frame accurate.
    // Therefore the Offscreen xstudio viewport will instead do a sync (blocking)
    // call to the python HUD Plugins to get the data for the on-screen media.
    // That data is pushed into 'hud_plugins_display_data' QML property (defined
    // in a parent object of this object). The key for pushing the data into 
    // the hud_plugins_display_data is the plugin UUID (which we can 
    // access here with module_uuid, provided by the attributes module that 
    // instanciates the HUD QML Items). Got it?!
    property var media_item_hud_data: {
        if (hud_plugins_display_data && module_uuid in hud_plugins_display_data) {
            return hud_plugins_display_data[module_uuid][imageIndex]
        } else {
            return __media_item_hud_data.value
        }
    }

}