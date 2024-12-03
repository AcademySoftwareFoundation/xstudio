// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import QuickFuture 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

Rectangle{
    color: XsStyleSheet.widgetBgNormalColor

    property bool isHovered: thumbMArea.containsMouse ||
            dateDiv.toolTipMArea.containsMouse ||
            artistDiv.toolTipMArea.containsMouse ||
            timeDiv.toolTipMArea.containsMouse ||
            typeDiv.toolTipMArea.containsMouse ||
            fromDiv.toolTipMArea.containsMouse ||
            toDiv.toolTipMArea.containsMouse ||
            noThumbnailDisplayMArea.containsMouse

    MouseArea { id: thumbMArea
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        propagateComposedEvents: true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins:
        {
            top: 1
            bottom: 0
            left: 2
            right: 4
        }
        spacing: 0

        Item{ id: thumbnailDiv
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Layout.preferredHeight: itemHeight
            // Layout.rowSpan: 2
            // Layout.columnSpan: 2

            property bool failed: thumbRole == undefined

            Rectangle{
                width: parent.width
                height: width / (16/9) - 10
                color: Qt.lighter(panelColor, 1.1)
                visible: parent.failed
            }
            XsText{ id: noThumbnailDisplay
                text: parent.failed? "No ShotGrid\nThumbnail":"Loading..."
                width: thumbnail.width
                height: thumbnail.height
                anchors.centerIn: parent
                wrapMode: Text.WordWrap
                font.pixelSize: XsStyleSheet.fontSize
                font.weight: Font.Black
                opacity: parent.failed? 0.4 : 0.9
                color: XsStyleSheet.secondaryTextColor

                MouseArea { id: noThumbnailDisplayMArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    enabled: thumbRole

                    XsToolTip{
                        text: thumbRole==undefined? "" : thumbRole
                        visible: parent.containsMouse && thumbnailDiv.failed
                        x: 0
                    }
                }
            }
            XsImage{ id: thumbnail
                visible: !parent.failed
                width: parent.width
                height: width / (16/9)
                source: parent.failed || thumbRole==undefined? "" : thumbRole
                fillMode: Image.PreserveAspectFit
                anchors.centerIn: parent
                cache: true
                asynchronous: true
                sourceSize.height: height
                sourceSize.width:  width
                imgOverlayColor: "transparent"

                opacity: 0
                Behavior on opacity {NumberAnimation {duration: 150}}

                onStatusChanged: {
                    if (status == Image.Null) { parent.failed=true; opacity=0 }
                    else if (status == Image.Error) { parent.failed=true; opacity=0; noThumbnailDisplay.text= "Thumbnail Error" }
                    else if (status == Image.Ready) opacity=1
                    else opacity=0
                }
            }



            XsPrimaryButton {
                imgSrc: "qrc:///shotbrowser_icons/attach_file.svg"
                height: 20
                width: 20
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 6
                background: Item{}
                visible: attachmentsRole && attachmentsRole.length
                imgOverlayColor: isHovered ? (pressed ? palette.highlight : palette.text) : XsStyleSheet.hintColor
                onClicked: {
                    attachmentsRole.forEach(function (item, index) {
                        Future.promise(
                                helpers.openURLFuture("http://shotgun.dneg.com/thumbnail/full/"+item.type+"/"+item.id)
                        ).then(function(result) {
                        })
                    })
                }

                layer.enabled: true
                layer.effect: DropShadow{
                    verticalOffset: 1
                    horizontalOffset: 1
                    color: "#010101"
                    radius: 1
                    samples: 3
                    spread: 0.5
                }

            }
        }
        Item{ id: emptyDiv
            Layout.fillWidth: true
            Layout.preferredHeight: 1
        }

        NotesHistoryDetailRow{ id: dateDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight/1.5
            titleText: "Date :"
            // valueText: "Nov 30 2023"

            property var dateFormatted: createdDateRole.toLocaleString().split(" ")
            valueText: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""

            toolTip: dateFormatted[0].substr(0,dateFormatted[0].length-1)
        }

        NotesHistoryDetailRow{ id: timeDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight/1.5
            titleText: "Time :"
            // valueText: "10:01 AM IST"

            property var dateFormatted: createdDateRole.toLocaleString().split(" ")
            property var timeFormatted: dateFormatted[4].split(":")
            valueText: typeof timeFormatted !== 'undefined'?
                typeof dateFormatted[6] !== 'undefined'?
                    timeFormatted[0]+":"+timeFormatted[1]+" "+dateFormatted[5]+" "+dateFormatted[6] :
                    timeFormatted[0]+":"+timeFormatted[1]+" "+dateFormatted[5] : ""

        }

        NotesHistoryDetailRow{ id: typeDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight/1.5
            titleText: "Type :"
            valueText: noteTypeRole ? noteTypeRole : ""
            textColor: palette.text
        }

        NotesHistoryDetailRow{ id: artistDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight/1.5
            titleText: "Artist :"
            valueText: artistRole ? artistRole : ""
        }

        NotesHistoryDetailRow{ id: fromDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight/1.5
            titleText: "From :"
            valueText: createdByRole ? createdByRole : ""
        }

        NotesHistoryDetailRow{ id: toDiv
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight/1.5
            titleText: "To :"
            valueText: addressingRole ? addressingRole.join("\n") : ""
        }
    }
}