// SPDX-License-Identifier: Apache-2.0
pragma Singleton
import QtQuick 2.12

QtObject {

    property string theme: "dark"

    property color primaryColor: "#414141"
    property color backgroundColor
    property color foregroundDisabledColor
    property color foregroundEnabledColor
    property color borderColor

    property color highlightColorDefault: "#bb7700"
    property color highlightColor: highlightColorDefault
    property color accentColor: "white"
    property color errorCodeColor: "red"

    property string primaryFont: "Overpass"
    property string errorCodeFont: "VeraMono"


    // Window
    property color mainBackground: backgroundColor
    property color mainColor: borderColor
    property color indevColor: foregroundDisabledColor
    property color hoverColor: accentColor
    property int colorDuration: 400  // for accent color change.
    property string fontFamily: primaryFont


    // Menu Related
    property int menuBarHeight: 26
    property int menuItemHeight: 22
    property color menuBackground: primaryColor
    property color menuBorderColor: borderColor
    property int menuBorderWidth: 1
    property int menuRadius: 5
    property int menuFontSize: 12
    property string menuFontFamily: fontFamily

    // Tab bar
    property int lrMargin: 2
    property int tabBarHeight: menuBarHeight
    property color currentTabColor: '#ff363636'
    property color notcurrentTabColor: '#ff202020'
    property color tabSeparatorColor: primaryColor
    property int tabSeparatorWidth: 1

    // Viewer title bar
    property int viewerTitleBarHeight: 40
    property int viewerTitleBarButtonsMargin: 8
    property int viewerTitleBarFontSize: 16

    // Media info bar
    property color mediaInfoBarBackground: primaryColor
    property int mediaInfoBarFontSize: 10
    property int mediaInfoBarMargins: 5
    property color mediaInfoBarOffsetBgColor: '#616161'
    property color mediaInfoBarOffsetEdgeColor: '#515151'
    property color mediaInfoBarOffsetBgColorDisabled: mediaInfoBarOffsetEdgeColor
    property color mediaInfoBarOffsetEdgeColorDisabled: primaryColor
    property string pixValuesFontFamily: 'VeraMono'
    property real pixValuesFontSize: 10
    property color mediaInfoBarBorderColor: backgroundColor

    // Trays
    property string fixWidthFontFamily: pixValuesFontFamily //'VeraMono'

    // Controls
    property color controlBackground: primaryColor
    property color controlColor: accentColor
    property color controlColorDisabled: foregroundEnabledColor
    property int controlHeight: 29
    property int toolsButtonIconSize: 17
    property string controlContentFontFamily: fontFamily
    property color controlTitleColor: controlColorDisabled
    property color controlTitleColorDisabled: foregroundDisabledColor
    property string controlTitleFontFamily: fontFamily
    property int timeFontSize: 13
    property int blankViewportMessageSize: 20
    property color blankViewportColor: "#666"
    property int frameErrorFontSize: 16
    property color frameErrorColor: errorCodeColor
    property string frameErrorFontFamily: fixWidthFontFamily
    property int frameErrorFontWeight: Font.Light
    property color outsideLoopTimelineColor

    // Popups
    property color popupBackground: primaryColor
    property int popupControlFontSize: 12

    // Panels
    property int panelHeaderHeight: 25

    // sessionbar
    property int sessionBarFontSize: 14
    property int sessionBarHeight: 50

    // session window dividers
    property int outerBorderWidth: 4
    property int dividerWidth: 4
    property color dividerColor: backgroundColor
    property int presentationModeAnimLength: 100

    // Transport
    property color transportBorderColor: primaryColor
    property color transportBackground: "#000"
    property int transportHeight: 31

    // Status Bar
    property int statusBarFontSize: 10
    property int statusBarHeight: 19
    property color statusBarBackground: backgroundColor

    // Media List Panel
    property int mediaListMaxScrollSpeed: 200 // pixels per second, only affects mouse wheel scrolling
    property color mediaListItemBG1: backgroundColor
    property color mediaListItemBG2: notcurrentTabColor

    // Viewport
    property int outOfRangeFontSize: 72
    property string outOfRangeFontFamily: fontFamily
    property var outOfRangeFontWeight: Font.DemiBold
    property color outOfRangeColor: "#44ffffff"

    // Layouts bar
    property int layoutsBarFontSize: 16


    function setFont(primaryFont)
    {
        if(primaryFont == "Overpass")
        {
            fontFamily = primaryFont
            menuFontSize = 12
            pixValuesFontSize = 10
        }
        else if(primaryFont == "VeraMono")
        {
            fontFamily = primaryFont
            menuFontSize = 12
            pixValuesFontSize = 10
        }
        else
        {
            fontFamily = primaryFont
            menuFontSize = 12
            pixValuesFontSize = 10
        }
    }


    function setTheme(currentTheme)
    {
        if(currentTheme == "light")
        {
            theme = currentTheme
            accentColor = "black"
            highlightColor = "tomato"
            primaryColor = "lightcyan"
            setPrimaryShades(primaryColor)

            hoverColor = accentColor
        }
        else //if(currentTheme == "dark")
        {
            theme = currentTheme
            accentColor = "white"
            highlightColor = highlightColorDefault
            primaryColor = "#414141"
            setPrimaryShades(primaryColor)

            hoverColor = accentColor
        }
        // console.debug("Theme set with primary-color: "+primaryColor+" ("+theme+")")
    }

    function setPrimaryShades(primaryColor)
    {
        var factor;
        var colors = [];

        if(theme == "light")
        {
            factor = 2.0; colors.push( Qt.darker( primaryColor, factor) )
            factor = 0.0; colors.push( Qt.lighter( primaryColor, factor) )
            factor = 1.4; colors.push( Qt.darker( primaryColor, factor) )
            factor = 2.0; colors.push( Qt.darker( primaryColor, factor) )
            factor = 2.4; colors.push( Qt.darker( primaryColor, factor) )
            factor = 1.2; colors.push( Qt.darker( primaryColor, factor) )
        }
        else //if(theme=="dark")
        {
            factor = 2.0; colors.push( Qt.darker( primaryColor, factor) )
            factor = 0.0; colors.push( Qt.lighter( primaryColor, factor) )
            factor = 1.4; colors.push( Qt.lighter( primaryColor, factor) )
            factor = 2.0; colors.push( Qt.lighter( primaryColor, factor) )
            factor = 2.4; colors.push( Qt.lighter( primaryColor, factor) )
            factor = 1.2; colors.push( Qt.darker( primaryColor, factor) )
        }

        // console.debug("colors: "+colors)

        backgroundColor = colors[0]            // mainBackground
        primaryColor = colors[1]               // controlBackground
        foregroundDisabledColor = colors[2]
        borderColor = colors[3]
        foregroundEnabledColor = colors[4]
        outsideLoopTimelineColor = colors[5]

        controlBackground = colors[1]
    }

 


    Component.onCompleted: {
        setTheme("dark")
    }
}
