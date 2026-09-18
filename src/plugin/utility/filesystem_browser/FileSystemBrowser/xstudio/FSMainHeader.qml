// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0

Rectangle {

    color: XsStyleSheet.panelBgGradTopColor
    id: root
    
    // Filter Row
    RowLayout {

        anchors.fill: parent
        anchors.margins: 0
        spacing: 1

        Repeater {
            model: filterTermAttrs
            XsViewerMenuButton
            {

                Layout.preferredWidth: 120
                Layout.fillHeight: true

                text: title
                shortText: abbr_title
                valueText: value
            }
        }
        
        // Text Filter
        XsSearchButton
        {
            Layout.preferredWidth: implicitWidth
            Layout.fillHeight: true
            isExpanded: false
            onTextChanged: {
                scanResultsModel.searchString = text
            }
            hint: "Filter String ..."
        }

        XsPrimaryButton {
            Layout.fillHeight: true
            visible: searching_attr.value === true
            text: "Stop Scan"
            onClicked: sendCommand({"action": "stop_scan"})
        }

        // Progress Bar (Left - fills remaining space)
        ProgressBar {

            id: scanProgress
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 2

            // Only visible when scanning
            visible: searching_attr.value === true
            
            from: 0
            to: 100
            value: progress_attr.value
            indeterminate: true 
            
            background: Rectangle {
                color: "#444444"
                radius: 3
            }
            contentItem: Item {
                Rectangle {
                    width: scanProgress.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: "#17a81a"
                }
            }
        }
        
        // If not scanning, we need a spacer to push buttons to right
        Item { 
            Layout.fillWidth: true 
            visible: !scanProgress.visible 
        }

        // Preview Indicator
        XsPrimaryButton {
            Layout.preferredWidth: 80
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            text: "Previewing"
            enabled: previewing
            onClicked: {
                sendCommand({"action": "stop_preview"})
            }
            isActive: previewing
        }
            
        // Divider (Vertical line)
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 14
            color: "#444444"
            Layout.alignment: Qt.AlignVCenter
        }

        // View Mode Selector (Right)
        RowLayout {
            spacing: 1
            Layout.fillHeight: true
            
            Repeater {
                model: ["qrc:/icons/list.svg", "qrc:/icons/list_group.svg", "qrc:/icons/ad_group.svg", "qrc:/icons/image.svg"]
                delegate: XsPrimaryButton {

                    Layout.preferredWidth: 32
                    Layout.fillHeight: true
                    visible: index != 2 // hiding 'group' view for now

                    isActive: viewMode === index
                    imgSrc: modelData
                    onClicked: viewMode = index
                    toolTip: root.tooltips[index]
                }
            }
        }
        
    }
    property var tooltips: ["List View", "Tree View", "Group View", "Thumbnail View"]

}