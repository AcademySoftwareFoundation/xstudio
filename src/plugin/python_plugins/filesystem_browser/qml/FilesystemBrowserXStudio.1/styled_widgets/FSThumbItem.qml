// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import xStudio 1.0

import ".."
import "."

Item {

    id: flatDelegate

    property bool visibleInFlickable: y > thumbFlickable.windowBottom && y < thumbFlickable.windowTop

    // ── Folder path header (spans full row) ────────────
    Rectangle {
        anchors.fill: parent
        visible: modelData.type === "header"
        color: "#1a1a1a"
        border.color: isHovered ? XsStyleSheet.accentColor : "transparent"
        border.width: 1
        XsIcon {
            id: folderIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left; anchors.leftMargin: 10
            width: 24; height: 24
            source: "qrc:/icons/folder.svg"
        }
        XsText {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: folderIcon.right; anchors.leftMargin: 10
            text: modelData.type === "header" ? modelData.path : ""
            font.bold: true
            elide: Text.ElideLeft
            font.pixelSize: 14
        }
    }

    // ── Thumbnail cell ──────────────────────────────────
    property bool isSelected: selectedItems.includes(index)
    property bool isHovered: underMouseIndex === index
    property bool infoHighlighted: false
    
    Rectangle {
        id: bgrect
        anchors.fill: parent; anchors.margins: 5
        visible: modelData.type === "file"
        color: XsStyleSheet.widgetBgNormalColor
        radius: 4
    }

    Rectangle {
        radius: 4
        anchors.fill: bgrect
        visible: isSelected ? modelData.type === "file" : 0
        color: XsStyleSheet.accentColor
        opacity: 0.5
        border.color: "#777777"
        border.width: 2

    }

    Rectangle {
        radius: 4
        anchors.fill: bgrect
        visible: modelData.type === "file"
        color: isHovered ? XsStyleSheet.hintColor : "transparent"
        opacity: 0.5
        border.color: "#777777"
        border.width: 1
    }

    function mouseMove(x, y) {
        var pt = mapToItem(infoIcon, x, y)
        if (pt.x >= 0 && pt.x <= infoIcon.width && pt.y >= 0 && pt.y <= infoIcon.height) {
            infoIcon.infoHighlighted = true
        } else {
            infoIcon.infoHighlighted = false
        }
    }


    ColumnLayout {

        anchors.fill: parent; anchors.margins: 10
        spacing: 4
        visible: modelData.type === "file"

        Item {

            Layout.fillWidth: true; Layout.fillHeight: true

            // This adds a big computation load to the QML scene graph if there
            // are many thumbnails
            /*BusyIndicator {
                anchors.centerIn: parent; width: 30; height: 30
                running: !modelData.data.thumbnailSource && modelData.type === "file"
                visible: running
            }*/

            Image {
                anchors.fill: parent
                property var thumbSource: "image://thumbnail/file://" + modelData.data.thumbnailFrame
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

            XsIcon {
                id: infoIcon
                anchors.top: parent.top; anchors.right: parent.right
                anchors.margins: 4
                width: 20; height: 20
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
        }

        Item {
            Layout.fillWidth: true; height: 32; clip: true
            property string rawName: modelData.name || ""
            property string ext: {
                var d = rawName.lastIndexOf(".")
                return d >= 0 ? rawName.slice(d + 1) : ""
            }
            property string stem: {
                var d = rawName.lastIndexOf(".")
                return d >= 0 ? rawName.slice(0, d) : rawName
            }
            property string baseName: stem.replace(/[#@%]+$/, "").replace(/\.$/, "")
            property string frameRange: modelData.frames || ""

            XsText {
                anchors.top: parent.top
                anchors.left: parent.left; anchors.right: parent.right
                text: parent.baseName; color: "#e0e0e0"; font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle
            }
            XsText {
                anchors.bottom: parent.bottom; anchors.left: parent.left
                text: parent.ext; color: "#888888"; font.pixelSize: 10
                visible: parent.ext !== ""
            }
            XsText {
                anchors.bottom: parent.bottom; anchors.right: parent.right
                text: parent.frameRange; color: "#888888"; font.pixelSize: 10
                visible: parent.frameRange !== ""
            }
        }
    }

    function tooltipText() {
        if (modelData.type !== "file") return ""
        // parent directory path only
        var txt = modelData.path
        var sl = txt.lastIndexOf("/")
        if (sl >= 0) txt = txt.substring(0, sl)
        
        txt += "\n" + (modelData.name || "")
        if (modelData.frames) txt += "\nFrames: " + modelData.frames
        if (modelData.data && modelData.data.date) txt += "\nModified: " + formatDate(modelData.data.date)
        if (modelData.data && modelData.data.size_str) txt += "\nSize: " + modelData.data.size_str
        return txt
    }

} // delegate