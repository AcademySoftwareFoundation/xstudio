// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import ShotBrowser 1.0

Rectangle{
    color: XsStyleSheet.widgetBgNormalColor

     RowLayout {
        anchors.fill: parent
        spacing: itemSpacing

        Rectangle{ id: indicators
            Layout.preferredWidth: 20
            Layout.fillHeight: true
            color: "transparent"

            Column{
                y: spacing
                anchors.fill: parent
                spacing: itemSpacing

                XsPrimaryButton{ id: notesIndicatorDisplay
                    property bool hasNotes: noteCountRole == 0 ? false : true
                    text: "N"
                    width: 20
                    height: parent.height/3 - itemSpacing
                    font.pixelSize: textSize*1.2
                    font.weight: hasNotes? Font.Bold:Font.Medium
                    isUnClickable: true
                    isActiveViaIndicator: false
                    textDiv.color: hasNotes? palette.text : XsStyleSheet.hintColor
                    enabled: false
                    bgDiv.opacity: enabled? 1.0 : 0.5
                    isActive: hasNotes
                }
                XsPrimaryButton{ id: dailiesIndicatorDisplay
                    property bool hasDailies: submittedToDailiesRole === undefined ? false :true
                    text: "D" //dalies
                    width: 20
                    height: parent.height/3 - itemSpacing
                    font.pixelSize: textSize*1.2
                    font.weight: hasDailies? Font.Bold:Font.Medium
                    isUnClickable: true
                    isActiveViaIndicator: false
                    textDiv.color: hasDailies? palette.text : XsStyleSheet.hintColor
                    enabled: false
                    bgDiv.opacity: enabled? 1.0 : 0.5
                    isActive: hasDailies
                }
                XsPrimaryButton{ id: clientIndicatorDisplay
                    property bool hasClient: dateSubmittedToClientRole === undefined ? false : true
                    text: "C" //client
                    width: 20
                    height: parent.height/3 - itemSpacing
                    font.pixelSize: textSize*1.2
                    font.weight: hasClient? Font.Bold:Font.Medium
                    isUnClickable: true
                    isActiveViaIndicator: false
                    textDiv.color: hasClient? palette.text : XsStyleSheet.hintColor
                    enabled: false
                    bgDiv.opacity: enabled? 1.0 : 0.5
                    isActive: hasClient
                }

            }

        }


        Item{ id: thumbImage
            Layout.fillWidth: true
            Layout.fillHeight: true

            property bool failed: thumbRole == undefined

            Rectangle{
                width: parent.width
                height: Math.min(width / (16/9), thumbImage.height) - 10
                color: Qt.lighter(panelColor, 1.1)
                visible: parent.failed
                anchors.verticalCenter: parent.verticalCenter
            }
            XsText{ id: noThumbnailDisplay
                text: parent.failed? "No ShotGrid\nThumbnail":"Loading..."
                width: thumbnail.width
                height: thumbnail.height
                anchors.centerIn: parent
                wrapMode: Text.WordWrap
                font.pixelSize: textSize*1.2
                font.weight: Font.Black
                opacity: parent.failed? 0.4 : 0.9
            }

            XsImage{ id: thumbnail
                visible: !parent.failed
                width: parent.width
                height: Math.min(width / (16/9), thumbImage.height)
                sourceSize.height: height
                sourceSize.width:  width
                source: parent.failed || thumbRole==undefined? "": thumbRole
                fillMode: Image.PreserveAspectFit
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                cache: true
                asynchronous: true
                imgOverlayColor: "transparent"

                opacity: 0
                Behavior on opacity {NumberAnimation {duration: 150}}

                onStatusChanged: {
                    if (status == Image.Null) { parent.failed=true; opacity=0 }
                    else if (status == Image.Error) { parent.failed=true; opacity=0; noThumbnailDisplay.text= "Thumbnail\nError" }
                    else if (status == Image.Ready) opacity=1
                    else opacity=0
                }
            }
        }
    }
}