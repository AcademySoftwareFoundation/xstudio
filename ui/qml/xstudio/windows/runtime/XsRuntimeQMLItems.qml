// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item {

    XsModuleData {

        id: dynamic_items
        // each playhead exposes its attributes in a model named after the
        // playhead UUID. We connect to the model like this:
        modelDataName: "dynamic qml items"
    }

    XsListView {
        model: dynamic_items
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
            onWidget_visibleChanged: {
                if (!widget_visible) {
                    attr_enabled = false
                }
            }
            onItemEnabledChanged: {
                if (itemEnabled && dynamic_widget) {
                    dynamic_widget.visible = true
                }
            }
            function xstudio_callback(data) {
                if (typeof data == "object") {
                    // 'callback_data' is role data on the attribute in the 'dynamic_items'
                    // model. Setting it here sets it in the backend
                    callback_data = data
                }
            }
        }
    }
}