// SPDX-License-Identifier: Apache-2.0
pragma Singleton
import QtQuick 2.12

QtObject {

    property var darkerFactor: 1.0
    property var textDarkerFactor: 1.0

    property color accentColor: "#D17000"

    property color accentColorBlue: "#307bf6"
    property color accentColorPurple: "#9b56a3"
    property color accentColorPink: "#e65d9c"
    property color accentColorRed: "#ed5f5d"
    property color accentColorOrange: "#e9883a"
    property color accentColorYellow: "#f3ba4b"
    property color accentColorGreen: "#77b756"
    property color accentColorGrey: "#999999"

    property real fontSize: 12
    property real playlistPanelFontSize: fontSize + 1
    property string fontFamily: "Inter" //"Regular" //"Overpass"
    property string fixedWidthFontFamily: "Courier" //"VeraMono" //"Regular"

    property color primaryTextColor: Qt.darker("#F1F1F1", textDarkerFactor)
    property color secondaryTextColor: Qt.darker("#C1C1C1", textDarkerFactor)
    property color hintColor: "#959595"
    property color errorColor: "#EB3941"

    property color baseColor: Qt.darker("#414141", darkerFactor)
    property color panelTitleBarColor: Qt.darker("#666666", darkerFactor)
    property color panelTabColor: Qt.darker("#1F1F1F", darkerFactor)
    property color panelBgColor: Qt.darker("#333333", darkerFactor)
    property color panelBgFlatColor: Qt.darker("#555555", darkerFactor)
    property color panelBgGradTopColor: Qt.darker("#5C5C5C", darkerFactor)
    property color panelBgGradBottomColor: menuBarColor
    property real panelPadding: 4
    property real dividerSize: 1

    property color widgetBgNormalColor: Qt.darker("#1AFFFFFF", darkerFactor)
    property real widgetStdHeight: 24
    property real widgetBorderWidth: 1

    property real primaryButtonStdWidth: 40
    property real primaryButtonStdHeight: widgetStdHeight + 4
    property real secondaryButtonStdWidth: 16

    // control colour = button colour etc?
    property color controlColour: Qt.darker("#33FFFFFF", darkerFactor)

    property real menuStdWidth: 140
    property real menuHeight: widgetStdHeight
    property color menuBarColor: Qt.darker("#474747", darkerFactor)
    property color menuLabelColor: primaryTextColor
    property color menuHotKeyColor: secondaryTextColor
    property color menuDividerColor: Qt.darker("#858585", darkerFactor)
    property color menuBorderColor: Qt.darker("#858585", darkerFactor)
    property real menuDividerHeight: 1
    property real menuPadding: 4
    property real menuLabelPaddingLR: 16
    property real menuIndicatorSize: 16
    property real menuCheckboxSize: 16

    property color scrollbarBaseColor: Qt.darker("#A1A1A1", darkerFactor)
    property color scrollbarActiveColor: Qt.darker("#C1C1C1", darkerFactor)

}
