// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.helpers 1.0

XsIntegerAttrControl {

    Layout.fillWidth: horizontal ? false : true
    Layout.preferredHeight: visible ? horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
    Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
    Layout.fillHeight: horizontal ? true : false
}
