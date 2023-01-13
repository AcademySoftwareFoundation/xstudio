// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5

import xStudio 1.0

XsMenu {
    title: qsTr("Fit")
    id: fitMenu
    hasCheckable: true

    property var viewportFitMode: viewport.fitMode
    
    Instantiator {
        model: viewport.fitModeOptions
        delegate: XsMenuItem {
			mytext: modelData["text"]
			mycheckable: true
            mychecked: modelData["text"] === viewportFitMode
            onTriggered: {
                viewport.fitMode = modelData["text"]
            }
        }
    	onObjectAdded: fitMenu.insertItem(index, object)
    	onObjectRemoved: fitMenu.removeItem(object)
    }

}





