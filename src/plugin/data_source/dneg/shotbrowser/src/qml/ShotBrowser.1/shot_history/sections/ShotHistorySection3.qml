// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import ShotBrowser 1.0

Rectangle{
    color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing

        ShotHistoryTextRow{ id: stepDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: pipelineStepRole
            textColor: palette.text
            textDiv.width: width
        }
        ShotHistoryTextRow{ id: statusDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            text: pipelineStatusFullRole
            textDiv.width: width
        }

        Rectangle{ id: siteDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight
            color: XsStyleSheet.widgetBgNormalColor

            Grid{ id: siteGrid
                width: parent.width - itemSpacing*(siteModel.count-1)
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                rows: 1
                columns: siteModel.count
                spacing: itemSpacing
                flow: Grid.LeftToRight

                Repeater{
                    model: siteModel

                    XsPrimaryButton{

                        property int onDisk: {
                            if(index==0) onSiteChn
                            else if(index==1) onSiteLon
                            else if(index==2) onSiteMtl
                            else if(index==3) onSiteMum
                            else if(index==4) onSiteSyd
                            else if(index==5) onSiteVan
                            else false
                        }

                        property real desiredWidth: siteGrid.width/siteModel.count > 40? 40 : siteGrid.width/siteModel.count

                        width: desiredWidth
                        height: siteGrid.height

                        isUnClickable: true
                        enabled: false
                        text: siteName
                        textDiv.color: onDisk? palette.text : XsStyleSheet.hintColor
                        font.pixelSize: textSize/1.4
                        font.weight: Font.Medium

                        bgDiv.opacity: enabled? 1.0 : 0.5
                        forcedBgColorNormal: onDisk ? onDisk == 1? "transparent"
                            : Qt.darker(siteColour, 1)
                            : Qt.lighter(panelColor, 1.1)

                        Rectangle{
                            width: parent.width
                            height: parent.height
                            anchors.bottom: parent.bottom
                            visible: parent.onDisk && parent.onDisk == 1
                            opacity: parent.enabled? 1.0 : 0.5
                            z:-1
                            // border.width: itemSpacing
                            // border.color: siteColour
                            gradient: Gradient {
                                GradientStop { position: 0.4; color: Qt.lighter(panelColor, 1.1) }
                                GradientStop { position: 0.8; color: Qt.darker(siteColour, 1)   }
                                GradientStop { position: 1.0; color: Qt.darker(siteColour, 1)   }
                            }
                        }

                    }
                }

                ListModel{
                    id: siteModel
                    ListElement{siteName:"chn"; siteColour:"#508f00"}
                    ListElement{siteName:"lon"; siteColour:"#2b7ffc"}
                    ListElement{siteName:"mtl"; siteColour:"#979700"}
                    ListElement{siteName:"mum"; siteColour:"#ef9526"}
                    ListElement{siteName:"syd"; siteColour:"#008a46"}
                    ListElement{siteName:"van"; siteColour:"#7a1a39"}
                }

            }
        }
    }
}