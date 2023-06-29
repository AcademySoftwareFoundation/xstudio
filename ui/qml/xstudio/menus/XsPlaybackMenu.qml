// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {
    title: qsTr("Playback")
    property var playhead: sessionWidget.viewport.playhead
    id: playback_menu

    XsMenuItem {
        mytext: qsTr("Play Backwards")
        shortcut: "J"
        onTriggered: {
            if (!playhead.playing) {
                playhead.velocityMultiplier = 1
                playhead.playing = true
                playhead.forward = false
            } else if (playhead.forward) {
                playhead.forward = false
                playhead.velocityMultiplier = 1;
            } else if (playhead.velocityMultiplier < 16) {
                playhead.velocityMultiplier = playhead.velocityMultiplier*2;
            }
        }
    }
    XsMenuItem {
        mytext: qsTr("Pause")
        shortcut: "K"
        onTriggered: {
            playhead.playing = false
        }
    }
    XsMenuItem {
        mytext: qsTr("Play Forwards")
        shortcut: "L"
        onTriggered: {
            onActivated: {
                if (!playhead.playing) {
                    playhead.velocityMultiplier = 1
                    playhead.playing = true
                    playhead.forward = true
                } else if (!playhead.forward) {
                    playhead.forward = true
                    playhead.velocityMultiplier = 1;
                } else if (playhead.velocityMultiplier < 16) {
                    playhead.velocityMultiplier = playhead.velocityMultiplier*2;
                }
            }
        }
    }
    XsMenuItem {
        mytext: qsTr("Play/Pause")
        onTriggered: {
            playhead.playing = !playhead.playing
        }
    }

    XsMenuSeparator {}

    XsMenuItem {
        mytext: qsTr("Next Frame")
        shortcut: "Right"
        onTriggered: {
            playhead.step(1)
        }
    }
    XsMenuItem {
        mytext: qsTr("Previous Frame")
        shortcut: "Left"
        onTriggered: {
            playhead.step(-1)
        }
    }
    XsMenuSeparator {}
    // XsMenuItem {
    //     mytext: qsTr("Add Bookmark")
    //     shortcut: "["
    //     onTriggered: {
    //         session.add_note(playhead.media, playhead.mediaSecond)
    //     }
    // }
    // XsMenuItem {
    //     mytext: qsTr("Add Bookmark And Show")
    //     shortcut: "{"
    //     onTriggered: {
    //         let uuid = session.add_note(playhead.media, playhead.mediaSecond)
    //         sessionWidget.toggleNotes(true)
    //     }
    // }
    // XsMenuItem {
    //     mytext: qsTr("Set Bookmark Out")
    //     shortcut: "]"
    //     onTriggered: session.bookmarks.setNearestOutBookmark(playhead.mediaUuid, playhead.second)
    // }
    // XsMenuItem {
    //     mytext: qsTr("Loop Bookmark")
    //     shortcut: "}"
    //     onTriggered: {
    //         let pos = session.bookmarks.getNearestBookmark(playhead.mediaUuid, playhead.second)
    //         if(pos.length) {
    //             playhead.loopStart = pos[0]
    //             playhead.loopEnd = pos[1]
    //             playhead.useLoopRange = true
    //         }
    //     }
    // }
    XsMenuItem {
        mytext: qsTr("Next Note")
        shortcut: "."
        onTriggered: playhead.frame = playhead.nextBookmark(playhead.frame)
    }
    XsMenuItem {
        mytext: qsTr("Previous Note")
        shortcut: ","
        onTriggered: playhead.frame = playhead.previousBookmark(playhead.frame)
    }
    XsMenuSeparator {
        id: loop_sep
    }

    XsModuleMenuBuilder {
        parent_menu: playback_menu
        root_menu_name: "playback_menu"
        insert_after: bod
    }

    XsMenuItem {
        mytext: qsTr("Set Loop In")
        shortcut: "I"
        onTriggered: {
            playhead.setLoopStart(playhead.frame)
            playhead.useLoopRange = true
        }
    }
    XsMenuItem {
        mytext: qsTr("Set Loop Out")
        id: bod
        shortcut: "O"
        onTriggered: {
            playhead.setLoopEnd(playhead.frame)
            playhead.useLoopRange = true
        }
    }

    XsMenuItem {
        mytext: qsTr("Enable Loop")
        shortcut: "P"
        mycheckable: true
        onTriggered: {
            playhead.useLoopRange = !playhead.useLoopRange
        }
        checked: playhead ? playhead.useLoopRange : 0
    }

    XsRepeatMenu {}

}
