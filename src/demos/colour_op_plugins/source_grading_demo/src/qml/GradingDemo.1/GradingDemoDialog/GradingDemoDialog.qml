// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.0

XsWindow {

    id: drawDialog
    title: "Basic Grading Tool Demo"

    width: minimumWidth
    minimumWidth: 215
    maximumWidth: minimumWidth

    height: minimumHeight
    minimumHeight: minimumWidth
    maximumHeight: minimumHeight

    XsModuleAttributesModel {
        id: grading_demo_controls
        attributesGroupNames: "grading_demo_controls"
    }

    Component {

        id: gradeSlider

        Item {

            id: slider_group

            // Note that here 'value' is propagated by the Repeater from the 
            // grading_demo_controls model, which maps to the attributes in 
            // the backen GradingDemoColourOp c++ class. Attributes hold
            // multiple bits of data - each bit of data has a 'role' id (such as
            // Value or Title or FloatScrubMin: see attribute.hpp). 
            // Each role enum has a corresponding role name string (such as 
            // 'value' or 'title' or 'float_scrub_min' - again these can be
            // seen in attribute.hpp). The XsAttributesModel has one element
            // per attribute that has been added to the 'group'. The role data
            // for each attribute is then visible within the model element as
            // a qml property whose name matches the corresponding role name.
            //
            // Note that this mechanism only works with certain QML classes that
            // instance items in the 'model-view'delegate' design pattern that
            // Qt/QML promotes. Such classes are 'Repeater', 'ListView' and so-on.
            //
            // So in this case 'value' is the attribute Value data, 'title' is its
            // Title data etc.
            property var backend_value : value 
            Layout.fillWidth: true
            Layout.fillHeight: true

            function set_value(v) {
                value = v
            }
            
            Slider {
                id: slider
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: label.top
                from: float_scrub_min
                to: float_scrub_max
                orientation: Qt.Vertical
                
                handle: Rectangle {
                    id: sliderHandle
                    x: (slider.width - width) / 2
                    y: slider.visualPosition * (slider.height - height)
                    width:15
                    height:15
                    radius:15
                    color: attr_colour
                    border.color: "white"
                }

                // To avoid nasty feedback loop, where frontend changes backend
                // value and thenm shortly later, we get an updated backed value
                // coming back (where our frontend value might have changed again)
                // we need to ensure that the backend only updates the front end
                // when the user isn't interacting
                property var backend_value: slider_group.backend_value
                onBackend_valueChanged: {
                    if (!pressed) value = backend_value
                }
                onValueChanged: {
                    if (pressed) {
                        // testing if pressed is crucial to prevent
                        // front-end/backend-end update sync problems, as an
                        // 
                        slider_group.set_value(value)
                    }
                }
                onPressedChanged: {
                    if (!pressed) value = backend_value
                }
            }
            Text {
                id: label
                height: 20
                text: title + ": " + backend_value.toFixed(2)
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: XsStyle.controlColor
                font.family: XsStyle.fixWidthFontFamily
                font.pixelSize: XsStyle.popupControlFontSize
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter    
            }
        }

    }

    ColumnLayout {

        anchors.fill: parent

        RowLayout {

            id: row_layout
            Layout.fillWidth: true
            Layout.fillHeight: true
    
            Repeater {
                model: grading_demo_controls
                delegate: gradeSlider
            }
        }
    
        Button {
            id: btnOK
            text: qsTr("Hide")
            width: 100
            height: 36
            Layout.alignment: Qt.AlignRight
            Layout.margins: 5
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            background: Rectangle {
    //                color: XsStyle.highlightColor//mouseArea.containsMouse?:XsStyle.controlBackground
                color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                anchors.fill: parent
            }
            contentItem: Text {
                text: btnOK.text
                color: XsStyle.hoverColor//:XsStyle.mainColor
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
    
            MouseArea {
                id: mouseArea
                hoverEnabled: true
                anchors.fill: btnOK
                cursorShape: Qt.PointingHandCursor
                onClicked: close()
            }
        }
    
    }

}