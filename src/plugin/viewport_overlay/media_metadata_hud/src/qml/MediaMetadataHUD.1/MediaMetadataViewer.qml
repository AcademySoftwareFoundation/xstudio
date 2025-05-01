// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.qmlmodels

import xStudio 1.0
import xstudio.qml.models 1.0
import "."
// This is an adapted version of XsAttributesPanel, allowing us to add a dynamic
// list of masks that the user can enable/disable

Item {

    id: dialog
    property bool select_mode: false

    XsModuleData {
        id: metadata_attrs
        modelDataName: "metadata_hud_attrs"
    }

    XsAttributeValue {
        id: __full_metadata_set
        attributeTitle: "Full Onscreen Image Metadata"
        model: metadata_attrs
    }
    property alias full_metadata_set: __full_metadata_set.value


    XsAttributeValue {
        id: __profile_field_values
        attributeTitle: "Profile Metadata Values"
        model: metadata_attrs
    }
    property alias profile_field_values: __profile_field_values.value


    XsAttributeValue {
        id: __profile_data
        attributeTitle: "Profile Fields Data"
        model: metadata_attrs
    }
    property alias profile_data: __profile_data.value


    XsAttributeValue {
        id: __profile_field_paths
        attributeTitle: "Profile Fields"
        model: metadata_attrs
    }
    property alias profile_field_paths: __profile_field_paths.value


    XsAttributeValue {
        id: __search_string
        attributeTitle: "Search String"
        model: metadata_attrs
    }
    property alias search_string: __search_string.value

    XsAttributeValue {
        id: __action
        attributeTitle: "Action"
        model: metadata_attrs
    }
    property alias actionAttr: __action.value

    // when this viewer is shown, we set the 'Action' attribute to the 
    // name of the viewport that this metadata viewer was launched from.
    // The backend plugin will then fetch the metadata for the on-screen
    // image and the media that it came from.
    property var parentVis: windowVisible
    onParentVisChanged: {
        if (parentVis) {
            actionAttr = ["SHOWING METADATA VIEWER", view.name]
        } else {
            actionAttr = ["HIDING METADATA VIEWER", ""]
        }
    }

    property var things: select_mode ? ["Pipeline Metadata", "Media Metadata", "Frame Metadata", "Layout"] : ["Pipeline Metadata", "Media Metadata", "Frame Metadata"]

    XsToolTip {
        id: toolTip
        maxWidth: parent.width*0.8
    }

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        TabBar {
            id: tabBar

            Layout.fillWidth: true
            background: Rectangle {
                color: palette.base
            }

            Repeater {
                model: things

                TabButton {

                    width: implicitWidth

                    contentItem: XsText {
                        text: things[index]
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: tabBar.currentIndex == index
                    }

                    background: Rectangle {
                        border.color: hovered? palette.highlight : "transparent"
                        color: tabBar.currentIndex == index ? XsStyleSheet.panelTitleBarColor : Qt.darker(XsStyleSheet.panelTitleBarColor, 1.5)
                    }
                }

            }

        }

        /*Item {
            Layout.preferredHeight: 10
        }*/

        ColumnLayout {

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8

            XsSearchButton {
                id: searchBtn
                Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
                Layout.preferredHeight: XsStyleSheet.widgetStdHieght + 4
                visible: tabBar.currentIndex != 3
                isExpanded: false
                hint: "Search metadata fields ..."
                // awkward two-way binding again
                onTextChanged: {
                    if (backendV != text) {
                        search_string = text
                    }
                }
                property var backendV: search_string
                onBackendVChanged: {
                    if (backendV != text) {
                        text = backendV
                    }
                }
            }        

            Flickable {
                id: flickable
                clip: true

                Layout.fillWidth: true
                Layout.fillHeight: true
                
                contentWidth: loader.item ? loader.item.width : 0
                contentHeight: loader.item ? loader.item.height : 0

                property var tabIdx: tabBar.currentIndex
                onTabIdxChanged: {
                    loader.sourceComponent = undefined
                    loader.sourceComponent = tabBar.currentIndex == 3 ? selected_metadata : metadata_group
                }
            
                Component {
                    id: metadata_group
                    MetadataGroup {
                        metadataSet: full_metadata_set
                        setKey: things[tabBar.currentIndex]
                        width: flickable.width
                    }
                }

                Component {
                    id: selected_metadata
                    SelectedMetadata {
                        width: flickable.width
                    }
                }

                Loader {
                    id: loader
                }

                ScrollBar.vertical: XsScrollBar {
                    // policy: ScrollBar.AlwaysOn
                    visible: flickable.height < flickable.contentHeight
                    policy: ScrollBar.AlwaysOn
                    width: 8
                }

            }

        }

    }
    
}