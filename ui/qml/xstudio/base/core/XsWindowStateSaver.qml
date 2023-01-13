// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.10
import QtQuick.Window 2.10
import QtQuick.Controls 2.3
import Qt.labs.settings 1.0

import xStudio 1.0

import xstudio.qml.properties 1.0

Item
{
    property Window windowObj
    property string windowName: ""
    property var override_pref_path: undefined

    property alias window_settings: window_settings

    XsPreferenceSet {

        id: window_settings
        preferencePath: override_pref_path ? override_pref_path : "/ui/qml/" + windowName + "_settings"
    }

    Component.onCompleted:
    {
        try  {
            windowObj.x = window_settings.properties.x
            windowObj.y = window_settings.properties.y
            windowObj.width = window_settings.properties.width
            windowObj.height = window_settings.properties.height
        } catch(err) {}
        //windowObj.visibility = window_settings.properties.visibility
    }

    Connections
    {
        target: windowObj
        function onXChanged(x) { saveSettingsTimer.restart() }
        function onYChanged(y) { saveSettingsTimer.restart() }
        function onWidthChanged(width) { saveSettingsTimer.restart() }
        function onHeightChanged(height) { saveSettingsTimer.restart() }
        function onVisibilityChanged(v) { saveSettingsTimer.restart() }
    }

    Timer
    {
        id: saveSettingsTimer
        interval: 1000
        repeat: false
        onTriggered: saveSettings()
    }

    function saveSettings()
    {
        switch(windowObj.visibility)
        {
            case ApplicationWindow.Windowed:
                window_settings.properties.x = windowObj.x;
                window_settings.properties.y = windowObj.y;
                window_settings.properties.width = windowObj.width;
                window_settings.properties.height = windowObj.height;
                window_settings.properties.visibility = windowObj.visibility;
                break;
            case ApplicationWindow.FullScreen:
                window_settings.properties.visibility = windowObj.visibility;
                break;
            case ApplicationWindow.Maximized:
                window_settings.properties.visibility = windowObj.visibility;
                break;
            case ApplicationWindow.Hidden:
                window_settings.properties.visibility = windowObj.visibility;
                break
        }
    }
}
