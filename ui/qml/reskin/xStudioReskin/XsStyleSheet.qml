// SPDX-License-Identifier: Apache-2.0
pragma Singleton
import QtQuick 2.12

QtObject {
    
    property color accentColor: "#D17000"

    property real fontSize: 12
    property string fontFamily: "Inter" //"Regular" //"Courier"

    property color primaryTextColor: "#F1F1F1"
    property color secondaryTextColor: "#C1C1C1"
    property color hintColor: "#959595"
    property color errorColor: "#EB3941"

    property color baseColor: "#414141"
    property color panelTitleBarColor: "#666666"
    property color panelTabColor: "#1F1F1F"
    property color panelBgColor: "#333333"
    property real panelPadding: 4

    property color widgetBgNormalColor: "#1AFFFFFF"
    property real widgetStdHeight: 24
    property real widgetBorderWidth: 1

    property real primaryButtonStdWidth: 40
    property real secondaryButtonStdWidth: 16

    property real menuStdWidth: 140
    property real menuHeight: widgetStdHeight
    property color menuBarColor: "#474747"
    property color menuLabelColor: primaryTextColor
    property color menuHotKeyColor: secondaryTextColor
    property color menuDividerColor: "#858585"
    property real menuDividerHeight: 1
    property real menuPadding: 4
    property real menuLabelPaddingLR: 16
    property real menuIndicatorSize: 16

}
