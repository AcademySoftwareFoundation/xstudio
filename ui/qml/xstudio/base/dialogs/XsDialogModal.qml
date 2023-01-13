// SPDX-License-Identifier: Apache-2.0
import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import xStudio 1.0

XsDialog {
    id: control
	modality: Qt.WindowModal

    Connections {
        target: control
        function onVisibleChanged() {
        	app_window.dimmer = control.visible
        }
    }
}