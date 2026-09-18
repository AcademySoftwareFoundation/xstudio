// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

import "."
import ".."

Item {

    id: delegate
    height: XsStyleSheet.widgetStdHeight

    property var modelIndex
    property bool isSelected: selectedItems.includes(modelIndex)
    property bool isHovered: underMouseIndex === modelIndex

    Loader {
        id: dataLoader
        anchors.fill: parent
        sourceComponent: dataVis
    }

    Component {
        id: dataVis
        Item {

            Rectangle {
                anchors.fill: parent
                color: (isHovered ? XsStyleSheet.panelBgGradTopColor : (modelIndex.row % 2 == 0 ? XsStyleSheet.panelBgColor : Qt.lighter(XsStyleSheet.panelBgColor, 1.1)))
            }

            Rectangle {
                anchors.fill: parent
                color: isSelected ? XsStyleSheet.accentColor : "transparent"
                opacity: 0.5
            }
                
            property var indent: depth * 20 + 44 + (is_folder ? 20 : 0)

            // Cells
            component Cell: XsText {
                property int index: 0
                x: columnPositions[index] + (index === 0 ? indent : 0)
                width: columnWidths[index]
                property int elideMode: Text.ElideRight
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: index ? Text.AlignHCenter : Text.AlignLeft
                elide: elideMode
                color: isSelected ? XsStyleSheet.primaryTextColor : XsStyleSheet.secondaryTextColor
            }

            Loader {
                x: indent-24
                width: 20
                height: 20
                anchors.verticalCenter: parent.verticalCenter
                sourceComponent: is_folder ? undefined : isHovered ? playButton : undefined
                Component {
                    id: playButton
                    XsIcon {
                        id: playIcon
                        source: "qrc:/icons/play_circle.svg"
                        visible: isHovered
                        opacity: playHighlighted ? 1.0 : 0.6
                        property bool playHighlighted: false

                        Connections {
                            target: mouseArea
                            enabled: isHovered
                            function onPositionChanged(mouse) {
                                var pt = mouseArea.mapToItem(playIcon, mouse.x, mouse.y)
                                playHighlighted = (pt.x >= 0 && pt.x <= 20 && pt.y >= 0 && pt.y <= 20)
                            }
                            function onPressed() {
                                mouseArea.startDrag(delegate)
                                if (playHighlighted) {
                                    sendCommand({"action": "preview_file", "path": scanResultsModel.get(modelIndex, "path")})                                    
                                }
                            }
                        }
                    }
                }
            }
                                    
            // Expander
            Loader {  
                x: indent - 44
                width: 40
                height: 20
                sourceComponent: is_folder ? folder_icon : undefined
                Component {
                    id: folder_icon
                    Item {
                        anchors.fill: parent
                        property bool expanderHovered: false
                        XsIcon {
                            id: expander
                            x: 2
                            y: 2
                            width: 16
                            height: 16
                            source: "qrc:/icons/chevron_right.svg"
                            rotation: is_expanded ? 90 : 0
                            Behavior on rotation {NumberAnimation{duration: 150 }}
                        }

                        Rectangle {
                            anchors.fill: expander
                            color: "transparent"
                            border.color: isHovered && expanderHovered ? XsStyleSheet.accentColor : "transparent"
                            border.width: 1
                        }

                        XsIcon {
                            x: 20
                            width: 20
                            height: 20
                            source: is_expanded ? "qrc:/icons/folder_open.svg" : "qrc:/icons/folder.svg"
                        }
                        Connections {
                            target: mouseArea
                            enabled: isHovered
                            function onPositionChanged(mouse) {
                                var pt = mouseArea.mapToItem(expander, mouse.x, mouse.y)
                                expanderHovered = (pt.x >= 0 && pt.x <= 16 && pt.y >= 0 && pt.y <= 16)
                            }
                            function onPressed() {
                                mouseArea.startDrag(delegate)
                                if (isHovered && expanderHovered) {
                                    if (is_expanded) { is_expanded = false }
                                    else { 
                                        is_expanded = true 
                                        if (!is_scanned) {
                                            sendCommand({"action": "scan_single", "path": scanResultsModel.get(underMouseIndex, "path")})
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

            }

            Cell { index: 0; text: name_and_count; elideMode: Text.ElideMiddle }
            Cell { index: 1; text: version ? "v"+ version : "" }
            Cell { index: 2; text: frames }
            Cell { index: 3; text: owner }
            Cell { index: 4; text: date_string}
            Cell { index: 5; text: size_str}

        }
    }
    
}