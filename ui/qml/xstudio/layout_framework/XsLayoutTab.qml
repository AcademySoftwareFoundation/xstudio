// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 1.4

import xstudio.qml.models 1.0

import xStudio 1.0

Item {

    // The Tab dynamically loads the item that is its contents. The loading of the
    // item is driven by the 'title' - this is used as an identifier for what
    // is to be loaded. We ask the global 'viewsModel' object to return us
    // a piece of qml code or the path to a .qml script for the given title.
    // The code/script and title are 'registered' via a function on the viewsModel
    // either directly in the qml source (see XsSessionWindow.qml) or in the
    // backend via the Module class (see Module::register_ui_panel_qml)
    //
    // This way UI panels can be added to xSTUDIO either in qml source or
    // in C++ plugin code.

    id: widget

    // we use this to traverse the qml context heriarchy to get to the 'panel'
    // qml item from some child^n (usually a menu item) of the panel see.
    // Helpers cpp class
    objectName: "XsPanelParent"

    title: ""
    clip: true
    property var viewSource
    property var currentViewSource
    property var the_panel
    property bool is_current_tab: false
    property var title

    onIs_current_tabChanged: {
        loadPanel()
        if (the_panel)
            the_panel.visible = is_current_tab
    }

    onThe_panelChanged: {
        if (the_panel)
            the_panel.visible = is_current_tab
    }

    onTitleChanged: {
        viewSource = viewsModel.view_qml_source(title)
    }

    property var vmLength: viewsModel.length
    onVmLengthChanged: {
        viewSource = viewsModel.view_qml_source(title)
    }

    property var panels_layout_model_length: panels_layout_model.length
    onPanels_layout_model_lengthChanged: {
        viewSource = viewsModel.view_qml_source(title)
    }

    onViewSourceChanged: {
        loadPanel()
    }

    function loadPanel() {

        if (!is_current_tab) return

        if (viewSource == currentViewSource || viewSource === undefined) return;

        if (viewSource.endsWith(".qml")) {

            let component = Qt.createComponent(viewSource)
            currentViewSource = viewSource

            if (component.status == Component.Ready) {

                if (the_panel != undefined) the_panel.destroy()
                the_panel = component.createObject(
                    widget,
                    {
                        visible: is_current_tab,
                    })

                // identify as an XStudioPanel - this is used when menu items
                // are actioned to identify the 'panel' context of the parent
                // menu
            } else {
                console.log("Error loading panel:", component, component.errorString())
            }

        } else {

            currentViewSource = viewSource
            the_panel = Qt.createQmlObject(viewSource, widget)
            the_panel.visible = is_current_tab
            // see note above

        }

    }

}