// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudio 1.0


Rectangle { id: widget

    property var filteredCount: "-"  //resultsModel.count
    property var totalCount: "-" //resultsModel.sourceModel.count + (resultsModel.sourceModel.truncated ? "+":"")

    property color bgColor: XsStyleSheet.panelBgColor
    property color borderColor: XsStyleSheet.menuDividerColor //"transparent"

    implicitWidth: 100
    implicitHeight: 28
    border.color: borderColor
    border.width: 1
    color: bgColor

    XsText{
        text: filteredCount + " / " +  totalCount
        width: parent.width
        anchors.centerIn: parent
    }
}