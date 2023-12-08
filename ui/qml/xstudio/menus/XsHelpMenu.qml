// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Dialogs 1.1

import xstudio.qml.helpers 1.0
import xStudio 1.0

XsMenu {
    XsModelNestedPropertyMap {
        id: custom_help_menu
        index: app_window.globalStoreModel.search_recursive("/ui/qml/custom_help_menu", "pathRole")
    }

    function openDoc(url) {
        if (url == "") {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
            dialog.title = "Error"
            dialog.text = "Unable to resolve location of user docs."
            dialog.show()
        } else {
            if(!helpers.openURL(url)) {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.width = 600
                dialog.title = "Error"
                dialog.text = "Failed to launch webbrowser, this is the link, please manually visit this.<BR><BR>"+url+"<BR><BR>"
                dialog.show()
            }
        }
    }

    title: qsTr("Help")

    XsMenuItem {
        mytext: qsTr("User Documentation")
        shortcut: "F1"
        onTriggered: openDoc(studio.userDocsUrl())
    }

    XsMenuItem {
        mytext: qsTr("Hot Keys")
        onTriggered: openDoc(studio.hotKeyDocsUrl())
    }

    XsMenuItem {
        mytext: custom_help_menu.values.title
        visible: custom_help_menu.values.title != ""
        onTriggered: openDoc(custom_help_menu.values.url)
    }


    XsMenuItem {
        mytext: qsTr("Python/C++ API Documentation")
        onTriggered: openDoc(studio.apiDocsUrl())
    }

    XsMenuSeparator { }

    XsMenuItem {
        mytext: qsTr("View Release Notes")
        onTriggered: openDialog("qrc:/dialogs/XsFunctionalFeaturesDialog.qml")
    }

    XsMenuItem {
        mytext: qsTr("Submit Feedback ...")
        onTriggered: openDialog("qrc:/dialogs/XsFeedbackDialog.qml")
    }

    XsMenuSeparator { }

    XsMenuItem {
        mytext: qsTr("About")
        shortcut: 'Ctrl+/'
        onTriggered: openDialog("qrc:/dialogs/XsAboutDialog.qml")
    }

    property var thedialog: undefined
    function openDialog(qml_path) {
		var component = Qt.createComponent(qml_path);
		if (component.status == Component.Ready) {
            thedialog = component.createObject(app_window);
            thedialog.visible = true
		} else {
			// Error Handling
			console.log("Error loading component:", component.errorString());
		}
	}
}
