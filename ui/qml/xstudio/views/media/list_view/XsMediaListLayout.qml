// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import "."

ColumnLayout {

    spacing: 0

    XsMediaHeader{

        id: titleBar
        Layout.fillWidth: true
        height: XsStyleSheet.widgetStdHeight
    }

    XsMediaList {

        id: mediaList
        Layout.fillWidth: true
        Layout.fillHeight: true
        itemRowHeight: rowHeight

    }

}
