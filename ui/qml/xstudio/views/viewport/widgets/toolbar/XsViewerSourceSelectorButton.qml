// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

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

}
