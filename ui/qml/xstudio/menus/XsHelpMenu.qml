// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Dialogs 1.1

import xStudio 1.0

XsMenu {
    title: qsTr("Help")
    XsMenuItem {
        mytext: qsTr("User Documentation")
        shortcut: "F1"
        onTriggered: {
            var url = session.userDocsUrl()
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
    }
    XsMenuItem {
        mytext: qsTr("Hot Keys")
        onTriggered: {
            var url = session.hotKeyDocsUrl()
            if (url == "") {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.title = "Error"
                dialog.text = "Unable to resolve location of docs."
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
    }
    XsMenuItem {
        mytext: qsTr("Developer Documentation")
        enabled:false
    }
    XsMenuItem {
        mytext: qsTr("Python/C++ API Documentation")
        onTriggered: {
            var url = session.apiDocsUrl()
            if (url == "") {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.title = "Error"
                dialog.text = "Unable to resolve location of docs."
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
    }
    XsMenuSeparator { }
    XsMenuItem {
        mytext: qsTr("View Release Notes")
        onTriggered: openDialog("qrc:/dialogs/XsFunctionalFeaturesDialog.qml")
    }
    //    XsMenuItem {
    //        mytext: qsTr("Support...")
    //        enabled:false
    //    }
    //    XsMenuSeparator { }
    XsMenuItem {
        mytext: qsTr("Submit Feedback ...")
        onTriggered: openDialog("qrc:/dialogs/XsFeedbackDialog.qml")
        // onTriggered: {
        //     var recip = "xstudio-support@dneg.com";
        //     var subj = "[" + Qt.application.name + "] Feedback";
        //     var body = "Thank you using "+ Qt.application.name + " and for taking time to give us your feedback or suggestions! This e-mail goes directly to the developers and we appeciate any comments you give us.\n\n\Kind regards,\nThe "+ Qt.application.name + " Development Team"
        //     Qt.openUrlExternally("mailto:"+recip+"?subject="+subj+"&body="+body);
        // }
    }
    // XsMenuItem {
    //     mytext: qsTr("Report Bug (via e-mail)")
    //     onTriggered: {
    //         var recip = "xstudio-support@dneg.com";
    //         var subj = "[" + Qt.application.name + "] Bug Report";
    //         var body = "We are sorry you ran in to an issue using " + Qt.application.name + ", but thank you for taking the time to let us know about it. To help remedy the situation quickly, we have some guidelines for reporting problems. Kindly let us know:\n\n1. What you did, with reproducible steps we can follow?\n\n2. What you expected to happen?\n\n3. What actually happened?\n\nWith this information we are able to diagnose and implement a solution. Feel free to attach a screen shot if you think that would help.\n\nThank you again,\nThe "+ Qt.application.name + " Development Team"
    //         Qt.openUrlExternally("mailto:"+recip+"?subject="+subj+"&body="+body);
    //     }
    // }
    // XsMenuSeparator { }

    XsMenuItem {
        mytext: qsTr("Check For Updates...")
        enabled:false
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
