// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic

import xstudio.qml.models 1.0
import xStudio 1.0
import "."

XsWindow {

	id: dialog
	width: 1024
	height: 300
    title: "Configure FFMPEG Video Encoding Presets"
    property string attr_name

    minimumWidth: 1080 
    minimumHeight: 260

    // access 'attribute' data exposed by our C++ plugin
    // access 'attribute' data exposed by our C++ plugin
    XsModuleData {
        id: video_render_status_attrs
        modelDataName: "video_render_status_attrs"
    }
    property alias video_render_status_attrs: video_render_status_attrs


    Loader {
        id: loader        
    }

    Component {
        id: ffmpeg_out_dlg
        VideoRendererFFMpegLog {
            title: "Video Encode Log"
        }
    }

    property var rowHeight: 30

    ColumnLayout {

        anchors.fill: parent

        Item {

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 30

            Rectangle {

                anchors.fill: parent

                color: XsStyleSheet.panelBgColor
                border.color: "black"

                Flickable {

                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    contentWidth: grid_layout.width
                    contentHeight: grid_layout.height
                    id: flickable

                    GridLayout {

                        id: grid_layout
                        rows: video_render_status_attrs.length
                        flow: GridLayout.TopToBottom
                        rowSpacing: 0
                        columnSpacing: 5
                        width: flickable.width

                        Repeater {
                            model: video_render_status_attrs
                            JobTextEntry {
                                id: input                                
                                text: value.render_item
                                Layout.preferredHeight: rowHeight
                                Layout.fillWidth: true
                                Layout.minimumWidth: 150
                                Layout.rightMargin: 15
                            }
                        }
                        Repeater {
                            model: video_render_status_attrs
                            JobTextEntry {
                                id: input
                                text: value.output_file
                                Layout.preferredHeight: rowHeight
                                Layout.fillWidth: true
                                Layout.minimumWidth: 150
                                Layout.rightMargin: 15
                            }
                        }
                        Repeater {
                            model: video_render_status_attrs
                            JobTextEntry {
                                id: input
                                text: value.video_preset
                                Layout.preferredHeight: rowHeight
                                Layout.rightMargin: 15
                            }
                        }
                        Repeater {
                            model: video_render_status_attrs
                            
                            JobTextEntry {
                                id: input
                                text: value.resolution
                                Layout.preferredHeight: rowHeight
                                Layout.rightMargin: 15
                            }
                        }
                        Repeater {
                            model: video_render_status_attrs

                            Item {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: rowHeight
                                JobTextEntry {
                                    id: input
                                    text: value.status
                                    anchors.fill: parent
                                    XsToolTip {
                                        text: value.status
                                        visible: ma.containsMouse && parent.truncated
                                        x: 0 //#TODO: flex/pointer
                                        y_reposition: -1.0
                                    }
                                    MouseArea {
                                        id: ma
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                    
                                }
                                Rectangle {
                                    height: parent.height
                                    width: {
                                        return parent.width*(value.percent_complete)*0.01
                                    }
                                    color: palette.highlight
                                    opacity: 0.4
                                }
                            }
                        }

                        Repeater {

                            model: video_render_status_attrs
                            Item {
                                Layout.preferredHeight: rowHeight
                                Layout.preferredWidth: rowHeight
                                property var icon_color: value.status_colour ? value.status_colour : "grey"
                                property bool ready_to_play: icon_color == "green"

                                Rectangle {
                                    anchors.fill: parent
                                    border.color: palette.highlight
                                    color: ready_to_play && ma.pressed ? palette.highlight : "transparent"
                                    visible: ready_to_play && ma.containsMouse
                                }

                                XsIcon { 
                                    anchors.fill: parent
                                    source: value.status_icon ? value.status_icon : ""
                                    imgOverlayColor: icon_color
                                    visible: value.visible
                                }
                                MouseArea {
                                    id: ma
                                    anchors.fill: parent                                    
                                    hoverEnabled: true
                                    cursorShape: ready_to_play ? Qt.PointingHandCursor : undefined
                                    onPressed: {
                                        xstudio_callback(["quickview_output", value.output_file, true])
                                    }
                                }
                            }
                        }

                        Repeater {
                            model: video_render_status_attrs

                            Item {
                                Layout.preferredHeight: rowHeight
                                Layout.preferredWidth: rowHeight
                                Layout.margins: 2
                                XsPrimaryButton { 
                            
                                    anchors.fill: parent
                                    imgSrc: "qrc:/video_renderer_icons/monitor_heart.svg"
                                    visible: value.visible
                                    onClicked: {
                                        loader.sourceComponent = ffmpeg_out_dlg
                                        loader.item.job_id = value.job_id
                                        loader.item.visible = true
                                    }
                                }
                            }
                        }

                        Repeater {
                            
                            model: video_render_status_attrs
                            Item {
                                Layout.preferredHeight: rowHeight
                                Layout.preferredWidth: rowHeight
                                Layout.margins: 2

                                XsPrimaryButton { 
                            
                                    anchors.fill: parent
                                    visible: value.visible
                                    imgSrc: "qrc:/icons/delete.svg" 
                                    onClicked: {
                                        xstudio_callback(["remove_job", value.job_id])
                                    }
                                }
                            }
                        }

                    }
                }

                XsScrollBar {
                    
                    id: verticalScroll
                    width: 8
                    anchors.bottom: parent.bottom
                    anchors.top: parent.top
                    anchors.left: parent.right
                    anchors.leftMargin: 3
                    orientation: Qt.Vertical
                    visible: target.height < target.contentHeight
                    property var target: flickable

                    property var yPos: target.visibleArea.yPosition
                    onYPosChanged: {
                        if (!pressed) {
                            position = yPos
                        }
                    }
                    
                    property var heightRatio: target.visibleArea.heightRatio
                    onHeightRatioChanged: {
                        verticalScroll.size = heightRatio
                    }

                    onPositionChanged: {
                        if (pressed) {
                            target.contentY = (position * target.contentHeight) + target.originY
                        }
                    }                

                }    

                XsScrollBar {
                    
                    height: 8
                    anchors.top: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 3
                    orientation: Qt.Horizontal
                    visible: target.width < target.contentWidth
                    property var target: flickable

                    property var xPos: target.visibleArea.xPosition
                    onXPosChanged: {
                        if (!pressed) {
                            position = xPos
                        }
                    }
                    
                    property var widthRatio: target.visibleArea.widthRatio
                    onWidthRatioChanged: {
                        size = widthRatio
                    }

                    onPositionChanged: {
                        if (pressed) {
                            target.contentX = (position * target.contentWidth) + target.originX
                        }
                    }                

                }                

            }
        }

        RowLayout {

            spacing: 5
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            Layout.margins: 10

            XsPrimaryButton {
                id: btnCancel
                text: qsTr("Close")
                width: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: dialog.visible = false
            }

        }

    }

}