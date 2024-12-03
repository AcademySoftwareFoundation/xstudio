// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.models 1.0
import xStudio 1.0

import "../widgets"

RowLayout { id: color_pref
    width: parent.width
    height: XsStyleSheet.widgetStdHeight

    XsLabel {
        Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
        Layout.preferredWidth: parent.width/2 //prefsLabelWidth
        Layout.maximumWidth: parent.width/2

        text: displayNameRole ? displayNameRole : nameRole
        horizontalAlignment: Text.AlignRight
    }

    // Rectangle shows the colour (held in 'valueRole')
    Rectangle {  id: control
        Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
        Layout.preferredWidth: 22
        Layout.preferredHeight: 22
        color: valueRole
        border.color: hovered ? "white" : "transparent"
        border.width: 1
        radius: width/2
        property bool hovered: ma.containsMouse

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                if (menu_loader.item == undefined) {
                    menu_loader.sourceComponent = popup
                }
                menu_loader.item.showMenu(
                    ma,
                    mouse.x,
                    mouse.y);
            }

        }

    }

    XsPreferenceInfoButton {
    }


    Loader {
        id: menu_loader
    }
    Component {
        id: popup

        XsPopupMenu {
            visible: false
            menu_model_name: "colour popup" + color_pref
        }
    }


    // build list of colours and names that come from the 'optionsRole' which
    // should be JSON object with keys which are the colour names and values
    // which are the colours.
    // e.g.: { "Blue": "#307bf6", "Purple" : "#9b56a3", "Pink": "#e65d9c" }

    property var colourNames: []
    property var colourValues: []
    property var opts: optionsRole
    onOptsChanged: {
        var names = []
        var colours = []
        Object.keys(opts).forEach(function(key) {
            names.push(key)
            colours.push(optionsRole[key])
        })
        colourNames = names
        colourValues = colours
    }

    // repeater to build custom menu
    Repeater {

        model: colourValues
        Item {
            XsMenuModelItem {
                text: color_pref.colourNames[index]
                menuPath: ""
                menuModelName: "colour popup" + color_pref

                // declare this as a custom menu item
                menuItemType: "custom"

                // puts the colour we want into userData property of the menu
                // model item - the user data is stored in the model and can
                // be accessed with the ui element is intantiated as 'user_data'
                // context data
                userData: color_pref.colourValues[index]

                // here we can create any widget we like that gets inserted
                // into the pop-up menu. It muse have a 'minWidth' property
                // which gives the width of the contents.
                customMenuQml: `
                    import QtQuick 2.15
                    import QtQuick.Layouts 1.3
                    import xStudio 1.0
                    Rectangle {
                        width: parent.width
                        height: XsStyleSheet.menuHeight
                        property var minWidth: metrics.width + 20
                        color: user_data // here we retrieve the colour from user_data
                        MouseArea
                        {
                            anchors.fill: parent
                            onClicked: {
                                valueRole = user_data
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            Rectangle {
                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                color: user_data == XsStyleSheet.accentColor ? "black" : "transparent"
                                width: 7
                                height: 7
                                radius: 3.5
                            }
                            XsText {
                                id: labelDiv
                                text: name ? name : "Unknown" //+ (sub_menu && !is_in_bar ? "   >>" : "")
                                font.pixelSize: XsStyleSheet.fontSize
                                font.family: XsStyleSheet.fontFamily
                                color: "black"
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                Layout.fillWidth: true
                            }
                            TextMetrics {
                                id:     metrics
                                font:   labelDiv.font
                                text:   labelDiv.text
                            }

                        }
                    }
                    `
                onActivated: {
                    valueRole = value
                }
            }
        }
    }

}
