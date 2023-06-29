// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.1
import QtQuick.Layouts 1.12

import xStudio 1.0

RowLayout {

    property int pad: 0
    
    XsTrayButton {

        Layout.fillHeight: true
        text: "Present"
        icon.source: "qrc:/icons/presentation_mode.png"
        buttonPadding: pad
        toggled_on: sessionWidget.presentation_layout.active
    
        // checked: mediaDialog.visible
        tooltip: "Enter presentation mode"
        
        onClicked: {        
            sessionWidget.layout_name = "presentation_layout"
        }
    }

    /*XsTrayButton {

        Layout.fillHeight: true
        text: "Browse"
        icon.source: "qrc:/icons/layout_browse.png"
        buttonPadding: pad
        toggled_on: sessionWidget.browse_layout.active            
        tooltip: "Enter browse layout"
        
        onClicked: {        
            sessionWidget.layout_name = "browse_layout"
        }
    }*/

    XsTrayButton {

        Layout.fillHeight: true
        text: "Review"
        icon.source: "qrc:/icons/layout_review.png"
        buttonPadding: pad
        toggled_on: sessionWidget.review_layout.active
        tooltip: "Enter review layout"
        
        onClicked: {        
            sessionWidget.layout_name = "review_layout"
        }
    }

    XsTrayButton {

        Layout.fillHeight: true
        text: "Edit"
        icon.source: "qrc:/icons/layout_edit.png"
        buttonPadding: pad
        toggled_on: sessionWidget.edit_layout.active
        tooltip: "Enter edit layout"
        
        onClicked: {        
            sessionWidget.layout_name = "edit_layout"
        }
    }

    Rectangle {
        width: 2
        radius: 2
        Layout.fillHeight: true
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        color: XsStyle.controlBackground
    }

    XsTrayButton {

        Layout.fillHeight: true
        text: "Pop Out"
        icon.source: "qrc:/icons/popout_viewer.png"
        tooltip: "Open the pop-out interface."
        buttonPadding: pad
        visible: sessionWidget.window_name == "main_window"
        onClicked: {
            app_window.togglePopoutViewer()
        }
        toggled_on: app_window ? app_window.popout_window ? app_window.popout_window.visible : false : false

    }

    XsTrayButton {

        Layout.fillHeight: true
        text: "Full Screen"
        icon.source: "qrc:/icons/enter_fullscreen.png"
        tooltip: "Toggle Full-Screen mode."
        buttonPadding: pad
        onClicked: {
            sessionWidget.playerWidget.toggleFullscreen()
        }
        toggled_on: sessionWidget.playerWidget.is_full_screen

    }


}