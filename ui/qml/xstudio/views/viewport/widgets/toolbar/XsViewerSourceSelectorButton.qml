// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

// 'XsViewerAnyMenuButton' can be used as a base for a button that appears
// in the viewport toolbar and that shows a pop-up menu when it is clicked
// on.
XsViewerAnyMenuButton  {

    id: source_button

    // here we choose which menu model is used to build the pop-up that appears
    // when you click on the source button in the toolbar. We want the menu
    // to show the sources of the current playhead, which are defined by the
    // Source and Audio Source attributes in the Playhead class.
    // see PlayheadActor::make_source_menu_model in the C++ source.
    menuModelName: currentPlayhead.uuid + "source_menu"

    text: "Source"
    shortText: "Src"
    valueText: viewportPlayhead.imageSourceName ? viewportPlayhead.imageSourceName : ""

    XsMenuModelItem {
        id: foo
        text: "Select ..."
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 2.75
        menuModelName: source_button.menuModelName
        onActivated: {
            // awkward! We have an instance of this XsMenuModelItem for each viewport
            // toolbar instance. When the user clicks on "Select ..." button in the
            // menu, every XsMenuModelItem instance here will get onActivated signal.
            // This means we will end up running the code here multiple times and
            // would get multiple XsViewerSourceSelectorPopupWindow showing.
            // However, the onActivated signal has a 'menuContext' argument which is the
            // high level XsViewportPanel object. 
            // Thus we filter on that here to ensure we don't have multiple source 
            // selector windows pop-up.
            if (menuContext == viewportWidget) {
                loader.sourceComponent = dialog
                loader.item.visible = true
            }
        }
    }

    Loader {
        id: loader
    }

    Component {
        id: dialog

        XsFloatingViewWindow {

            minimumWidth: 250
            minimumHeight:220
            defaultWidth: 250
            defaultHeight: 300

            name: "Source Selector"
            content_qml: "qrc:/views/viewport/widgets/toolbar/XsViewerSourceSelectorPopupWindow.qml"
        }

    }
}
