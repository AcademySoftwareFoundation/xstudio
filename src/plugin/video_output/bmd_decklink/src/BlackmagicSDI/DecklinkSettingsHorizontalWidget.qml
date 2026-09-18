// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

	id: bmd_settings_dialog
    property var dockWidgetSize: XsStyleSheet.primaryButtonStdHeight + 4

    // this is REQUIRED to ensure correct scaling
    anchors.fill: parent
    anchors.rightMargin: 90 // makes space for 'position' button

    XsGradientRectangle{
        anchors.fill: parent
    }

    XsModuleData {
        id: decklink_settings
        modelDataName: "Decklink Settings"
    }
    property alias decklink_settings: decklink_settings

    XsModuleData {
        id: decklink_viewport_attributes
        modelDataName: "decklink viewport attrs"
    }
    property alias decklink_viewport_attributes: decklink_viewport_attributes

    XsModuleData {
        id: decklink_ocio_attributes
        modelDataName: "decklink ocio"
    }
    property alias decklink_ocio_attributes: decklink_ocio_attributes

    XsAttributeValue {
        id: __decklinkEnabled
        attributeTitle: "Enabled"
        model: decklink_settings
    }
    property alias is_running: __decklinkEnabled.value

    XsAttributeValue {
        id: __startStop
        attributeTitle: "Start Stop"
        model: decklink_settings
    }
    property alias startStop: __startStop.value

    XsAttributeValue {
        id: __outputResolution
        attributeTitle: "Output Resolution"
        model: decklink_settings
    }
    property alias outputResolution: __outputResolution.value

    XsAttributeValue {
        id: __pixelFormat
        attributeTitle: "Pixel Format"
        model: decklink_settings
    }
    property alias pixelFormat: __pixelFormat.value

    XsAttributeValue {
        id: __frameRate
        attributeTitle: "Refresh Rate"
        model: decklink_settings
    }
    property alias frameRate: __frameRate.value

    property real button_radius: 3

    RowLayout {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 12
        spacing: 0
                
        XsText {
            text: "Decklink SDI Output"
            font.pixelSize: 12
            font.weight: Font.Bold
        }

        Item {
            Layout.preferredWidth: 20
        }

        DecklinkOutputButton {

            Layout.fillHeight: true
            Layout.preferredWidth: 80
            Layout.margins: 4

        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.margins: 8
            color: XsStyleSheet.menuBorderColor
        }

        XsSecondaryButton{

            id: settingsBtn
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Layout.margins: 4
            imgSrc: "qrc:/icons/settings.svg"
            imageSrcSize: width
            onClicked: {
                loader.sourceComponent = settingsDlg
                loader.item.show()
            }

            Loader {
                id: loader
            }

            Component {
                id: settingsDlg
                DecklinkSettingsDialog {

                }
            }
        }        

        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.margins: 8
            color: XsStyleSheet.menuBorderColor
        }

        XsText {
            text: "Output : "
            font.pixelSize: 12
        }

        XsText {
            text: outputResolution + " / " + pixelFormat + " / " + frameRate + " fps"
            font.weight: Font.Bold
            font.pixelSize: 12
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.margins: 8
            color: XsStyleSheet.menuBorderColor
        }

        Repeater {
            model: decklink_ocio_attributes
            XsViewerMenuButton
            {
                Layout.preferredWidth: 140
                Layout.fillHeight: true
                Layout.margins: 4
                text: title
                shortText: abbr_title
                valueText: value
            }
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.margins: 8
            color: XsStyleSheet.menuBorderColor
        }

        DecklinkStatusIndicator {
            Layout.alignment: Qt.AlignLeft
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
        }

    }

}