// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

import xstudio.qml.module 1.0
import xstudio.qml.embedded_python 1.0

XsMenu {
    id: panel_menu
    title: qsTr("Panels")


    // XsMenuItem {
    //     mytext: qsTr("Session Window")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Viewer")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Timeline")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Media")
    //     enabled: false
    // }

    // connect to the backend module to give access to attributes
    XsModuleAttributes {
        id: anno_tool_backend_settings
        attributesGroupNames: "annotations_tool_settings_0"
    }

    // XsMenuSeparator { }
    XsMenuItem {
        mytext: qsTr("Notes")
        shortcut: "N"
        onTriggered: sessionWidget.toggleNotes()
    }
    XsMenuItem {
        mytext: qsTr("Drawing Tools")
        enabled: anno_tool_backend_settings.annotations_tool_active != undefined
        shortcut: "D"
        onTriggered: {
            anno_tool_backend_settings.annotations_tool_active = !anno_tool_backend_settings.annotations_tool_active
        }
    }
    XsMenuItem {
        mytext: qsTr("Colour Correction")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Image Transforms")
        enabled: false
    }

    XsMenuSeparator {
        id: panel_sep
    }

    // XsModuleMenuBuilder {
    //     parent_menu: panel_menu
    //     root_menu_name: "panel_menu"
    // }

    // XsMenuItem {
    //     mytext: qsTr("Shot Browser")
    //     // enabled: false
    //     shortcut: "B"
    //     onTriggered: sessionWidget.toggleShotgunBrowser()
    // }

    // XsMenuSeparator { }
    // XsMenuItem {
    //     mytext: qsTr("Analyse")
    //     enabled: false
    // }
    //    XsMenuItem {
    //        mytext: qsTr("HUD + Mask")
    //        enabled: false
    //    }
    // XsMenuItem {
    //     mytext: qsTr("Media Info")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Display Options")
    //     enabled: false
    // }

    // XsMenuSeparator { }
    // XsMenuItem {
    //     mytext: qsTr("Conform Media")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Trim Clips")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Compare Offset")
    //     enabled: false
    // }

    // XsMenuSeparator { }
    // XsMenuItem {
    //     mytext: qsTr("Find")
    //     enabled: false
    // }
    //    XsMenuItem {
    //        mytext: qsTr("Media Filter")
    //        enabled: false
    //    }

    // XsMenuSeparator { }
    // XsMenuItem {
    //     mytext: qsTr("Export Media")
    //     enabled: false
    // }

    // XsMenuSeparator {}
    // XsMenuItem {
    //     mytext: qsTr("Sync Connections")
    //     enabled: false
    // }
    // XsMenuItem {
    //     mytext: qsTr("Stream Settings")
    //     enabled: false
    // }

    XsMenuSeparator { }
    XsErrorMessage {
        id: snippet_output
        // parent: sessionWidget
        x: Math.max(0, (sessionWidget.width - width) / 2)
        y: (sessionWidget.height - height) / 2
        textHorizontalAlignment: TextEdit.AlignLeft
        textFontSize: 12
    }
    EmbeddedPython {
        id: python
        onStdoutEvent: {
            snippet_output.title = "Snippet Stdout"
            snippet_output.text = str
            snippet_output.show()
        }
        onStderrEvent: {
            snippet_output.title = "Snippet StdErr"
            snippet_output.text = str
            snippet_output.show()
        }
    }
    XsMenuItem {
        mytext: qsTr("Python Console...")
        onTriggered: openDialog("qrc:/widgets/XsPythonWidget.qml")
    }
    XsMenuItem {
        mytext: qsTr("Output Log...")
        onTriggered: app_window.toggleLogDialog()
    }
    XsMenu {
        id: snippet_menu
        title: qsTr("Snippets")
        Instantiator {
            model: python.snippetMenus
            delegate:
            XsMenu {
                id: snippet_submenu
                title: modelData["name"]
                Instantiator {
                    model: modelData.snippets
                    delegate:
                    XsMenuItem {
                        id: snippet_submenu
                        mytext: modelData["name"]
                        onTriggered: python.pyExec(modelData["script"])
                    }
                    onObjectAdded: snippet_submenu.insertItem(index, object)
                    onObjectRemoved: snippet_submenu.removeItem(object)
                }
            }
            onObjectAdded: snippet_menu.insertMenu(index, object)
            onObjectRemoved: snippet_menu.removeMenu(object)
        }
    }

    XsMenuSeparator { }
    XsMenuItem {
        mytext: qsTr("Settings")
        onTriggered: sessionWidget.toggleSettingsDialog()
    }

    XsMenuSeparator { 
        id: bod
    }

    XsModuleMenuBuilder {
        parent_menu: panel_menu
        root_menu_name: "panels_menu"
        insert_after: bod
    }

    property var thedialog: undefined
    function openDialog(qml_path)
    {
        var component = Qt.createComponent(qml_path);
        if (component.status == Component.Ready)
        {
            thedialog = component.createObject(app_window);
            thedialog.visible = true
        }
        else {
            // Error Handling
            console.error("Error loading component: ", component.errorString());
        }
    }

}
