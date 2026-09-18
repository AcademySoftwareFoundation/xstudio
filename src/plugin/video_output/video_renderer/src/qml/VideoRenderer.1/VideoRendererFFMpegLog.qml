// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic

import xstudio.qml.models 1.0
import xStudio 1.0

XsWindow {

	id: dialog
	width: 450
	height: 300
    title: "Configure FFMPEG Video Encoding Presets"
    property string attr_name
    property var job_id: ""

    // access 'attribute' data exposed by our C++ plugin
    // access 'attribute' data exposed by our C++ plugin
    XsModuleData {
        id: video_render_status_attrs
        modelDataName: "video_render_status_attrs"
    }
    property alias video_render_status_attrs: video_render_status_attrs

    XsAttributeValue {
        id: __ffmpeg_log
        attributeTitle: job_id
        role: "user_data"
        model: video_render_status_attrs
    }
    property alias ffmpeg_log: __ffmpeg_log.value

    property int current_job_idx: 0

    ColumnLayout {
        anchors.fill: parent
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20

            color: XsStyleSheet.panelBgColor
            border.color: "black"

            Flickable {

                anchors.fill: parent
                anchors.margins: 4
                clip: true
                contentWidth: log.width
                contentHeight: log.height
                id: flickable

                XsTextEdit {
                    id: log
                    text: ffmpeg_log ? ffmpeg_log : ""
                    horizontalAlignment: Qt.AlignLeft
                    readOnly: true
                    selectByMouse: true
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

        XsSimpleButton {
            id: btnCancel
            Layout.alignment: Qt.AlignRight
            Layout.margins: 10
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: dialog.close()
        }
    }


}