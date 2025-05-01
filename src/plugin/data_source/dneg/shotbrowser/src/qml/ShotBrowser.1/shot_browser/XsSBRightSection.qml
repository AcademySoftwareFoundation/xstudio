// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0

ColumnLayout {
    spacing: panelPadding

    XsSBR1Tools{
        Layout.fillWidth: true
    }

    Rectangle{
        color: panelColor
        Layout.fillWidth: true
        Layout.fillHeight: true

        XsSBR2Views{
            anchors.fill: parent
            anchors.margins: 4
        }
    }

    XsSBR3Actions{
        Layout.fillWidth: true
        Layout.minimumHeight: XsStyleSheet.widgetStdHeight
        Layout.maximumHeight: XsStyleSheet.widgetStdHeight
    }
}
