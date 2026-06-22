// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

Item {
    id: widget
    property string prefix: ""

    //
    // Here is the logic to remember the scrolled position per container.
    // This assumes that somewhere in the parent hierarchy is a dictionary
    // property named scrollKeeper into which the scroll data is kept.
    //

    // machanism to stash the contentY before it gets erased
    // when a new container is selected
    property real storeContentY: 0
    Connections {
        target: mediaListModelData
        enabled: visible
        function onModelAboutToBeReset() {
            storeContentY = contentY
        }
    }

    // machanism to store and restore the scroll position
    // whenever the current container changes
    Connections {
        target: sessionSelectionModel
        enabled: visible
        function onCurrentChanged(cur_idx, prev_idx) {
            if (prev_idx.valid && storeContentY) {
                let key = prefix + theSessionData.get(prev_idx, "idRole")
                scrollKeeper[key] = storeContentY
                storeContentY = 0
            }

            let key = prefix + inspectedMediaSetProperties.values.idRole
            if (typeof scrollKeeper == "object" && scrollKeeper.hasOwnProperty(key)) {
                contentY = scrollKeeper[key]
            }
        }
    }

    // machanisms to store and restore the scroll position
    // whenever the mode is changed between grid and list
    Component.onDestruction: {
        let id_ = inspectedMediaSetProperties.values.idRole
        if (id_ && typeof scrollKeeper == "object" ) {
            let key = prefix + id_
            scrollKeeper[key] = contentY
        }
    }
    Component.onCompleted: {
        let id_ = inspectedMediaSetProperties.values.idRole
        if (id_) {
            let key = prefix + id_
            if (typeof scrollKeeper == "object" && scrollKeeper.hasOwnProperty(key))
                contentY = scrollKeeper[key]
        }
    }
}
