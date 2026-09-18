// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

ListView {
    clip: true
    interactive: true
    spacing: 0
    orientation: ListView.Vertical
    snapMode: ListView.NoSnap
    flickDeceleration: 100000

    add: Transition{
        NumberAnimation{ property:"opacity"; duration: 100; from: 0}
    }
    addDisplaced: Transition{
        NumberAnimation{ property:"y"; duration: 100}
    }

    remove: Transition{
        NumberAnimation{ property:"opacity"; duration: 100; to: 0}
    }
    removeDisplaced: Transition{
        NumberAnimation{ property:"y"; duration: 100}
    }
}