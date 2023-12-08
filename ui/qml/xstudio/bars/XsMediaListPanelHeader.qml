// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12
import QtQml.Models 2.15
import xstudio.qml.helpers 1.0

import xStudio 1.0

Rectangle {

    id: media_list_header
    color: "transparent"
    property bool expanded: true
    anchors.fill: parent

    Label {
        anchors.leftMargin: 7
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: "Media"
        color: XsStyle.controlColor
        font.pixelSize: XsStyle.sessionBarFontSize
        font.family: XsStyle.controlTitleFontFamily
        font.hintingPreference: Font.PreferNoHinting
        font.weight: Font.DemiBold
        verticalAlignment: Qt.AlignVCenter
    }

    Label {
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        text: app_window.currentSource.fullName
        color: XsStyle.controlColor
        font.pixelSize: XsStyle.sessionBarFontSize
        font.family: XsStyle.controlTitleFontFamily
        font.hintingPreference: Font.PreferNoHinting
        font.weight: Font.DemiBold
        verticalAlignment: Qt.AlignVCenter
    }

    Label {
        anchors.rightMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        text: app_window.currentSource.values.mediaCountRole ? app_window.currentSource.values.mediaCountRole : 0
        color: XsStyle.controlColor
        font.pixelSize: XsStyle.sessionBarFontSize
        font.family: XsStyle.controlTitleFontFamily
        font.hintingPreference: Font.PreferNoHinting
        font.weight: Font.DemiBold
        verticalAlignment: Qt.AlignVCenter
    }

}
