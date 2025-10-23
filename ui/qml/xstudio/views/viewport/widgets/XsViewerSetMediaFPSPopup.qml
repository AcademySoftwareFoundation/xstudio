// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0

XsPopup {

    id: the_popup
    width: 140
    height: 120
    bottomPadding: 0
    topPadding: 0

    // get to the 'value' and 'user_data' role data of the 'FPS' attribute that
    // is exposed in the viewport toolbar
    XsAttributeValue {
        id: __fps_attr
        attributeTitle: "FPS"
        model: __vp_toolbar_attrs
    }

    XsAttributeValue {
        id: __fps_attr_user_data
        attributeTitle: "FPS"
        role: "user_data"
        model: __vp_toolbar_attrs
    }

    XsModuleData {
        id: __vp_toolbar_attrs
        modelDataName: view.name + "_toolbar"
    }

    property alias fps_attr: __fps_attr.value
    property alias fps_attr_user_data: __fps_attr_user_data.value

    onVisibleChanged: {

        if (visible) {
            editFPS.text = fps_attr
            editFPS.forceActiveFocus()
            editFPS.selectAll()
        }

    }

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 12

        XsText{ 
            id: label
            Layout.alignment: Qt.AlignHCenter
            text: "Set Media FPS:"
            // opacity: muted? 0.7:1
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
        }

        Item {
            Layout.fillHeight: true
        }

        XsTextField { 
            id: editFPS
            Layout.preferredWidth: 60
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            Layout.alignment: Qt.AlignHCenter
            onAccepted: {
                // backend will handle this (viewport.cpp)
                fps_attr_user_data = text
                the_popup.close()
            }
            property var value_: fps_attr
            onValue_Changed: {
                if (typeof(fps_attr) == "string") {
                    text = fps_attr
                }
            }
            validator: DoubleValidator
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {

            Layout.fillWidth: true

            XsSimpleButton {

                id: reset
                Layout.alignment: Qt.AlignLeft
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                margin: 5
                text: "Reset"
                onClicked: {
                    fps_attr_user_data = "reset"
                    editFPS.text = fps_attr
                }
            }

            Item {
                Layout.fillWidth: true
            }

            XsSimpleButton {

                Layout.alignment: Qt.AlignRight
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                margin: 5
                text: "Done"
                onClicked: {
                    fps_attr_user_data = editFPS.text
                    the_popup.close()
                }
            }
        }

    }

}
