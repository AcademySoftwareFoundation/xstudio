// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 1.4

import xstudio.qml.models 1.0

import xStudioReskin 1.0

Tab {
    id: widget
    title: ""
    property var viewSource
    property var currentViewSource
    property var the_panel

    onTitleChanged: {
        viewSource = views_model.view_qml_source(title)
    }

    // this model lists the 'Views' (e.g. Playlists, Media List, Timeline plus
    // and 'views' registered by an xstudio plugin)
    XsViewsModel {
        id: views_model 
    }

    Connections {
        target: views_model
        function onJsonChanged() {
            viewSource = views_model.view_qml_source(title)
        }
    }

    onViewSourceChanged: {
        loadPanel()
    }

    function loadPanel() {

        if (viewSource == currentViewSource) return;

        let component = Qt.createComponent(viewSource)
        currentViewSource = viewSource

        let tab_bg_visible = viewSource != "Viewport"

        if (component.status == Component.Ready) {

            if (the_panel != undefined) the_panel.destroy()
            the_panel = component.createObject(
                widget,
                {
                })
        } else {
            console.log("Error loading panel:", component, component.errorString())
        }
    }
}