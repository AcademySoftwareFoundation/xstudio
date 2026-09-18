// SPDX-License-Identifier: Apache-2.0
pragma Singleton
import QtQuick

QtObject {

    // dynamic UI brightness
    property real luminance: 1.0
    property real textLuminance: 1.0

    property bool useFlatTheme: false

    // fonts
    readonly property int fontSize: 12
    readonly property string fontFamily: "Inter"

    readonly property string fixedWidthFontFamily: systemFixedWidthFontFamily // A context property set-up on C++ side
    readonly property string altFixedWidthFontFamily: "Bitstream Vera Sans Mono"

    // colours
    property color accentColor: "#D17000"

    // shades of gray
    readonly property color gray100: "#333333"
    readonly property color gray200: "#414141"
    readonly property color gray300: "#5C5C5C"
    readonly property color gray400: "#666666"
    readonly property color gray500: "#858585"
    readonly property color gray600: "#959595"
    readonly property color gray700: "#A1A1A1"
    readonly property color gray800: "#C1C1C1"
    readonly property color gray900: "#F1F1F1"

    readonly property color panelBgColor:           Qt.darker(gray100, luminance)
    readonly property color baseColor:              Qt.darker(gray200, luminance)
    readonly property color panelBgGradTopColor:    Qt.darker(gray300, luminance)
    readonly property color panelTitleBarColor:     Qt.darker(gray400, luminance)
    readonly property color menuBorderColor:        Qt.darker(gray500, luminance)
    readonly property color hintColor:              gray600
    readonly property color scrollbarBaseColor:     Qt.darker(gray700, luminance)
    readonly property color secondaryTextColor:     Qt.darker(gray800, textLuminance)
    readonly property color scrollbarActiveColor:   Qt.darker(gray800, luminance)
    readonly property color primaryTextColor:       Qt.darker(gray900, textLuminance)

    // readonly property color widgetBgNormalColor:    Qt.darker(gray300, luminance)
    // readonly property color gray450: "#7D7D7D"
    // readonly property color controlColour:          Qt.darker(gray450, luminance)

    property color widgetBgNormalColor:             Qt.darker("#1AFFFFFF", luminance)
    property color controlColour:                   Qt.darker("#33FFFFFF", luminance)

    // sizes
    readonly property int panelPadding: 4
    readonly property int dividerSize: 1

    readonly property int widgetStdHeight: 24
    readonly property int widgetBorderWidth: 1

    readonly property int primaryButtonStdWidth: 40
    readonly property int primaryButtonStdHeight: widgetStdHeight + 4
    readonly property int secondaryButtonStdWidth: 16

    readonly property int menuStdWidth: 140
    readonly property int menuHeight: widgetStdHeight
    readonly property int menuDividerHeight: 1
    readonly property int menuPadding: 4
    readonly property int menuLabelPaddingLR: 16
    readonly property int menuIndicatorSize: 16
    readonly property int menuCheckboxSize: 16
}
