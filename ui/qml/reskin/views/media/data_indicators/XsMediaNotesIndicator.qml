// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudioReskin 1.0
import QtQuick.Layouts 1.15

Item{ 
    
    id: notesDiv
    clip: true

    RowLayout{ id: attachmentIconsRow
        anchors.left: parent.left
        anchors.leftMargin: itemPadding*2
        anchors.verticalCenter: parent.verticalCenter
        spacing: itemPadding/2

        // TODO: add notes/annotation/colour/transform status data to
        // MediaItem in the main data model to drive these buttons ...
        
        Repeater{

            model: 4

            XsSecondaryButton{ id: attachmentIcon

                enabled: false

                Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth
                Layout.maximumWidth: XsStyleSheet.secondaryButtonStdWidth
                Layout.minimumHeight: XsStyleSheet.secondaryButtonStdWidth
                Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth
                imgSrc: 
                    index==0 ? "qrc:/icons/sticky_note.svg" :
                    index==1 ? "qrc:/icons/brush.svg" : 
                    index==2 ? "qrc:/icons/tune.svg" : 
                    index==3 ? "qrc:/icons/open_with.svg" : ""

            }
        }
    }
    Rectangle{
        width: headerThumbWidth; 
        height: parent.height
        anchors.right: parent.right
        color: bgColorPressed
    }

}