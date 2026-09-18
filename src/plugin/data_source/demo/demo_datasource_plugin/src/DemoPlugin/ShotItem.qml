// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0

// this import allows us to use the DemoPluginDatamodel
import demoplugin.qml 1.0

TreeBaseItem {

    id: shot_item
    text: shot ? shot : ""
    expandable: false
    
}