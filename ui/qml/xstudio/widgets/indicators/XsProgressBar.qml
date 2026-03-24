// SPDX-License-Identifier: Apache-2.0
import QtQuick
import xStudio 1.0

ProgressBar{
    indeterminate: false

    background: Rectangle{
        implicitWidth: 100
        implicitHeight: 1
        color: XsStyleSheet.accentColor
    }
}