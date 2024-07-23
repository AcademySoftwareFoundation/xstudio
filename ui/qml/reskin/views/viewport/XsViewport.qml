// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.15

import xStudioReskin 1.0
import xstudio.qml.viewport 1.0

//Viewport {
Rectangle{ color: "transparent"

    id: viewport
    anchors.fill: parent
    property color gradient_colour_top: "#5C5C5C"
    property color gradient_colour_bottom: "#474747"

    property alias view: view
    focus: true

    Item {
        anchors.fill: parent
        // Keys.forwardTo: viewport //#TODO: To check with Ted
        focus: true
        Keys.forwardTo: view
    }

    property real panelPadding: XsStyleSheet.panelPadding

    Rectangle{
        id: r
        gradient: Gradient {
            GradientStop { position: r.alpha; color: gradient_colour_top }
            GradientStop { position: r.beta; color: gradient_colour_bottom }
        }
        anchors.fill: actionBar
        property real alpha: -y/height
        property real beta: parent.height/height + alpha

    }

    XsViewportActionBar{
        id: actionBar
        anchors.top: parent.top
        actionbar_model_data_name: view.name + "_actionbar"
    }


    Rectangle{
        id: r2
        gradient: Gradient {
            GradientStop { position: r2.alpha; color: gradient_colour_top }
            GradientStop { position: r2.beta; color: gradient_colour_bottom }
        }
        anchors.top: infoBar.top
        anchors.bottom: infoBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        property real alpha: -y/height
        property real beta: parent.height/height + alpha

    }

    XsViewportInfoBar{
        id: infoBar
        anchors.top: actionBar.bottom
    }
    
    property color gradient_dark: "black"
    property color gradient_light: "white"

    Viewport {
        id: view
        x: panelPadding
        y: (actionBar.height + infoBar.height)
        width: parent.width-(x*2)
        height: parent.height-(toolBar.height + transportBar.height) - (y)

        onPointerEntered: {
            focus = true;
            forceActiveFocus()
        }

    }

    Rectangle{
        // couple of pixels down the left of the viewport
        id: left_side
        gradient: Gradient {
            GradientStop { position: left_side.alpha; color: gradient_colour_top }
            GradientStop { position: left_side.beta; color: gradient_colour_bottom }
        }
        anchors.left: parent.left
        anchors.right: view.left
        anchors.top: view.top
        anchors.bottom: view.bottom
        property real alpha: -y/height
        property real beta: parent.height/height + alpha

    }

    Rectangle{
        // couple of pixels down the right of the viewport
        id: right_side
        gradient: Gradient {
            GradientStop { position: right_side.alpha; color: gradient_colour_top }
            GradientStop { position: right_side.beta; color: gradient_colour_bottom }
        }
        anchors.left: view.right
        anchors.right: parent.right
        anchors.top: view.top
        anchors.bottom: view.bottom
        property real alpha: -y/height
        property real beta: parent.height/height + alpha

    }

    Rectangle{
        id: r3
        gradient: Gradient {
            GradientStop { position: r3.alpha; color: gradient_colour_top }
            GradientStop { position: r3.beta; color: gradient_colour_bottom }
        }
        anchors.fill: toolBar
        property real alpha: -y/height
        property real beta: parent.height/height + alpha

    }

    XsViewportToolBar{
        id: toolBar
        anchors.bottom: transportBar.top
        toolbar_model_data_name: view.name + "_toolbar"
    }

    Rectangle{
        id: r4
        gradient: Gradient {
            GradientStop { position: r4.alpha; color: gradient_colour_top }
            GradientStop { position: r4.beta; color: gradient_colour_bottom }
        }
        anchors.fill: transportBar
        property real alpha: -y/height
        property real beta: parent.height/height + alpha

    }
    XsViewportTransportBar{ 
        id: transportBar
        anchors.bottom: parent.bottom
    }


}