// SPDX-License-Identifier: Apache-2.0
pragma Singleton
import QtQuick 2.15
import xStudio 1.0

QtObject {

    // Backgrounds
    property color backgroundColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelBgColor !== undefined) ? XsStyleSheet.panelBgColor : "#333333"
    property color panelBgColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelBgColor !== undefined) ? XsStyleSheet.panelBgColor : "#333333"
    property color headerBgColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelTitleBarColor !== undefined) ? XsStyleSheet.panelTitleBarColor : "#474747"
    property color alternateBgColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelBgColor !== undefined) ? Qt.darker(XsStyleSheet.panelBgColor, 1.1) : "#2d2d2d"

    // Text Colors
    property color textColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.primaryTextColor !== undefined) ? XsStyleSheet.primaryTextColor : "#F1F1F1"
    property color secondaryTextColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.secondaryTextColor !== undefined) ? XsStyleSheet.secondaryTextColor : "#C1C1C1"
    property color hintColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.hintColor !== undefined) ? XsStyleSheet.hintColor : "#959595"
    property color accentColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.accentColor !== undefined) ? XsStyleSheet.accentColor : "#D17000"

    // Interaction Colors
    property color selectionColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelBgFlatColor !== undefined) ? Qt.lighter(XsStyleSheet.panelBgFlatColor, 1.35) : "#7a7a7a"
    property color hoverColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelBgColor !== undefined) ? Qt.lighter(XsStyleSheet.panelBgColor, 1.15) : "#3D3D3D"
    property color pressedColor: (typeof XsStyleSheet !== 'undefined' && XsStyleSheet.panelBgColor !== undefined) ? Qt.darker(XsStyleSheet.panelBgColor, 1.1) : "#2A2A2A"

    // Borders / Dividers
    property color borderColor: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.menuBorderColor : "#858585"
    property color dividerColor: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.menuDividerColor : "#858585"

    // Fonts
    property string fontFamily: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.fontFamily : "Inter"
    property real fontSize: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.fontSize : 12
    property string fixedWidthFontFamily: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.fixedWidthFontFamily : "Monospace"

    // Dimensions
    property real rowHeight: 28
    property real headerHeight: 30
    property real padding: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.panelPadding : 4
    property real widgetHeight: (typeof XsStyleSheet !== 'undefined') ? XsStyleSheet.widgetStdHeight : 24
}
