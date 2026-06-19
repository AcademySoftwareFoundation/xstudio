// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import xStudio 1.0

import ".."
import "."

Item {

    id: flatDelegate

    property real rooty: 0 

    // this is true if we're either in or close to being in the flicaable viewport. We only load the actual widget content
    // if this is true to aid performance when there are many thumbnails in the list.
    property bool visibleInFlickableFuzzy: smallResultSet || ((yInWindow) > (thumbFlickable.windowBottomMore) && (yInWindow) < (thumbFlickable.windowTopMore))
    property var yInWindow: y + rooty
    property alias thumbLoader: thumbLoader
    Loader {
        id: thumbLoader
        anchors.fill: parent
        sourceComponent: visibleInFlickableFuzzy ? theThumb : undefined
    }

    Rectangle {
        id: bgrect
        anchors.fill: parent; anchors.margins: 5
        color: XsStyleSheet.widgetBgNormalColor
        radius: 4
    }
    
    Component {
        id: theThumb
        Item {
            // ── Thumbnail cell ──────────────────────────────────
            property bool isSelected: selectedItems.includes(modelIndex)
            property bool isHovered: modelIndex === underMouseIndex
            property bool infoHighlighted: false

            // this is true if we're exactly in the flicaable viewport. We only load the thumbnail (which is expensive) if we are visible.
            property bool visibleInFlickable: (yInWindow) > (thumbFlickable.windowBottom) && (yInWindow) < (thumbFlickable.windowTop)

            property var modelIndex: scanResultsModel.index(index, 0, thumbsModelIndex)
            
            Rectangle {
                radius: 4
                anchors.fill: parent; anchors.margins: 5
                color: isSelected ? XsStyleSheet.accentColor : isHovered ? XsStyleSheet.hintColor : "transparent"
                opacity: 0.5
                border.color: "#777777"
                border.width: 1
            }

            ColumnLayout {

                anchors.fill: parent; anchors.margins: 10
                spacing: 4
                //visible: type === "file"

                Item {

                    Layout.fillWidth: true; Layout.fillHeight: true

                    // This adds a big computation load to the QML scene graph if there
                    // are many thumbnails
                    /*BusyIndicator {
                        anchors.centerIn: parent; width: 30; height: 30
                        running: !thumbnailSource && type === "file"
                        visible: running
                    }*/

                    Image {
                        anchors.fill: parent
                        property var thumbSource: "image://thumbnail/file://" + thumbnailFrame
                        source: (visibleInFlickable || loaded) ? thumbSource : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        Component.onDestruction: source = ""
                        property bool loaded: false
                        onStatusChanged: {
                            if (status === Image.Ready) {
                                loaded = true
                            }
                        }
                    }

                }

                Item {
                    Layout.fillWidth: true; height: 32; clip: true

                    XsText {
                        anchors.top: parent.top
                        anchors.left: parent.left; anchors.right: parent.right
                        text: stem; color: "#e0e0e0"; font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle
                    }
                    XsText {
                        anchors.bottom: parent.bottom; anchors.left: parent.left
                        text: extension; color: "#888888"; font.pixelSize: 10
                        visible: parent.extension !== ""
                    }
                    XsText {
                        anchors.bottom: parent.bottom; anchors.right: parent.right
                        text: frames; color: "#888888"; font.pixelSize: 10
                    }
                }
            }

            Loader {
                id: overlays
                anchors.fill: parent
                sourceComponent: is_folder ? undefined : isHovered ? playButton : undefined
                Component {
                    id: playButton
                    Item {

                        XsIcon {
                            id: playIcon
                            anchors.top: parent.top; anchors.left: parent.left
                            anchors.margins: 14
                            width: 24; height: 24
                            source: "qrc:/icons/play_circle.svg"
                            visible: isHovered
                            opacity: playHighlighted ? 1.0 : 0.6
                            property bool playHighlighted: false

                        }

                        XsIcon {
                            id: infoIcon
                            anchors.top: parent.top; anchors.right: parent.right
                            anchors.margins: 14
                            width: 24; height: 24
                            source: "qrc:/icons/info.svg"
                            visible: isHovered
                            opacity: infoHighlighted ? 1.0 : 0.6
                            property bool infoHighlighted: false
                            onInfoHighlightedChanged: {
                                if (infoHighlighted) {
                                    var pt = mapToItem(root, 10, 10)
                                    thumbToolTip.x = pt.x
                                    thumbToolTip.y = pt.y
                                    thumbToolTip.text = tooltipText()
                                    thumbToolTip.visible = true
                                } else {
                                    thumbToolTip.visible = false
                                }    
                            }

                        }

                        Connections {
                            target: mouseArea
                            enabled: isHovered
                            function onPositionChanged(mouse) {
                                var pt = mouseArea.mapToItem(playIcon, mouse.x, mouse.y)
                                if (pt.x >= 0 && pt.x <= playIcon.width && pt.y >= 0 && pt.y <= playIcon.height) {
                                    playIcon.playHighlighted = true
                                } else {
                                    playIcon.playHighlighted = false
                                    pt = mouseArea.mapToItem(infoIcon, mouse.x, mouse.y)
                                    if (pt.x >= 0 && pt.x <= infoIcon.width && pt.y >= 0 && pt.y <= infoIcon.height) {
                                        infoIcon.infoHighlighted = true
                                    } else {
                                        infoIcon.infoHighlighted = false
                                    }
                                }
                            }
                            function onPressed() {

                                mouseArea.startDrag(flatDelegate)
                                if (playIcon.playHighlighted) {
                                    sendCommand({"action": "preview_file", "path": scanResultsModel.get(modelIndex, "path")})                                    
                                }
                            }
                        }
                    }
                }
            }

            function tooltipText() {
                // parent directory path only
                var txt = path
                var sl = txt.lastIndexOf("/")
                if (sl >= 0) txt = txt.substring(0, sl)
                
                txt += "\n" + (name || "")
                if (frames) txt += "\nFrames: " + frames
                if (data && date) txt += "\nModified: " + formatDate(date)
                if (data && size_str) txt += "\nSize: " + size_str
                return txt
            }

        }
    }


} // delegate