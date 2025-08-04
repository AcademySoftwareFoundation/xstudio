// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item {

    property var dynamic_set_name: "dynamic qml items"

    DelegateModel {
        id: dynamic_items

        model: XsModuleData {

            // each playhead exposes its attributes in a model named after the
            // playhead UUID. We connect to the model like this:
            modelDataName: dynamic_set_name
        }

        delegate: Item {
            id: parent_item

            property var source: qml_code
            property var itemEnabled: attr_enabled
            property var dynamic_widget
            property var widget_visible: dynamic_widget ? dynamic_widget.visible : false

            onSourceChanged: {
                if (typeof source != "string" || dynamic_widget) return
                if (source.endsWith(".qml")) {
                    let component = Qt.createComponent(source)
                    if (component.status == Component.Ready) {
                        dynamic_widget = component.createObject(
                            parent_item, {})
                    } else {
                        console.log("Couldn't load qml file.", component, component.errorString())
                    }
                } else {
                    dynamic_widget = Qt.createQmlObject(source, parent_item)
                }

            }

            onItemEnabledChanged: {
                if (itemEnabled && dynamic_widget) {
                    dynamic_widget.visible = true
                }
            }

            onWidget_visibleChanged: {
                if (!widget_visible) {
                    attr_enabled = false
                }
            }

            function xstudio_callback(data) {
                if (typeof data == "object") {
                    // 'callback_data' is role data on the attribute in the 'dynamic_items'
                    // model. Setting it here sets it in the backend
                    callback_data = data
                }
            }

            function python_callback(python_func_name, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10) {

                var v = [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10]
                while (v.length) {
                    if (v[v.length-1] == null || v[v.length-1] == undefined) {
                        v.pop()
                    } else {
                        break;
                    }
                }
                return helpers.python_callback(python_func_name, module_uuid, v)

            }
        }
    }


    Repeater {
        model: dynamic_items
    }
}