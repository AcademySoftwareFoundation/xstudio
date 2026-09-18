// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic

import xstudio.qml.models 1.0
import xStudio 1.0

XsTextCopyable {
    
    id: label
    text: root.text
    // if value.visible == false, it's a 'header' entry for our table
    font.weight: value.visible ? Font.Medium : Font.Bold

}