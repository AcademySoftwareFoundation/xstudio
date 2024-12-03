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

        ShotHistoryTextRow{ id: authorDiv
            Layout.fillWidth: true
            Layout.fillHeight: true

            textDiv.leftPadding: panelPadding
            textDiv.horizontalAlignment: Text.AlignLeft

            text: authorRole
            textColor: palette.text
        }
        ShotHistoryTextRow{ id: dateDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight

            textDiv.leftPadding: panelPadding
            textDiv.horizontalAlignment: Text.AlignLeft

            property var dateFormatted: createdDateRole.toLocaleString().split(" ")

            text: typeof dateFormatted !== 'undefined'? dateFormatted[1].substr(0,3)+" "+dateFormatted[2]+" "+dateFormatted[3] : ""
        }
        ShotHistoryTextRow{ id: frameRangeDiv
            Layout.fillWidth: true
            Layout.minimumHeight: XsStyleSheet.widgetStdHeight

            textDiv.leftPadding: panelPadding
            textDiv.horizontalAlignment: Text.AlignLeft

            text: frameRangeRole
        }
    }
}