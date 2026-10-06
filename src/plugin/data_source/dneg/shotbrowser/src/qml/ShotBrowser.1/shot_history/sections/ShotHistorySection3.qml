// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0

Rectangle{
    color: "transparent"
    id: control

    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing

        RowLayout {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight

            spacing: itemSpacing

            ShotHistoryTextRow{ id: stepDiv
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: pipelineStepRole
                textColor: XsStyleSheet.primaryTextColor
                textDiv.width: width
            }
            ShotHistoryTextRow{ id: statusDiv
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: pipelineStatusFullRole
                textDiv.width: width
            }
        }

        ShotHistoryTextRow{ id: prodDiv
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            text: productionStatusFullRole
            textColor: XsStyleSheet.primaryTextColor
            textDiv.width: width
        }

        RowLayout { id: siteDiv
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight-1
            Layout.maximumHeight: XsStyleSheet.widgetStdHeight-1

            readonly property var panelColorLight: Qt.lighter(panelColor, 1.5)

            spacing: itemSpacing

            Repeater{
                model: ListModel {
                        id: siteModel
                        ListElement{siteName:"chn"; siteColour:"#508f00"}
                        ListElement{siteName:"lon"; siteColour:"#2b7ffc"}
                        ListElement{siteName:"mtl"; siteColour:"#979700"}
                        ListElement{siteName:"mum"; siteColour:"#ef9526"}
                        ListElement{siteName:"syd"; siteColour:"#008a46"}
                    }

                Rectangle{
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Layout.minimumHeight: siteDiv.height - 1
                    // Layout.maximumHeight: siteDiv.height - 1
                    // Layout.minimumWidth: (control.width - itemSpacing * 1.5 * (siteModel.count-1)) / siteModel.count
                    // Layout.maximumWidth: (control.width - itemSpacing * 1.5 * (siteModel.count-1)) / siteModel.count

                    property int onDisk: {
                        if(index==0) onSiteChn
                        else if(index==1) onSiteLon
                        else if(index==2) onSiteMtl
                        else if(index==3) onSiteMum
                        else if(index==4) onSiteSyd
                        else false
                    }

                    opacity: 0.5

                    gradient: Gradient {
                        GradientStop { position: 0.4;
                            color: !onDisk ? siteDiv.panelColorLight : onDisk == 1 ? siteDiv.panelColorLight : siteColour
                        }
                        GradientStop { position: 0.8;
                            color: !onDisk ? siteDiv.panelColorLight : Qt.darker(siteColour, 1)
                        }
                        GradientStop { position: 1.0;
                            color: !onDisk ? siteDiv.panelColorLight : Qt.darker(siteColour, 1)
                        }
                    }

                    XsLabel {
                        anchors.fill: parent
                        text: siteName
                        font.pixelSize: textSize/1.4
                        font.weight: Font.Medium
                    }
                }
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}