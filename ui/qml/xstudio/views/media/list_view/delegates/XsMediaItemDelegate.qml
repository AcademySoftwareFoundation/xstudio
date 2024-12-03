// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "../data_indicators"
import "../../common_delegates"
import "../../functions"

Rectangle {

    id: listItem
    width: parent.width
    implicitHeight: itemRowHeight + drag_target_indicator.height
    color: "transparent"

    property color hintColor: XsStyleSheet.hintColor
    property color errorColor: XsStyleSheet.errorColor

    property bool isSelected: false
    property bool isDragTarget: false


    // these are referenced by XsMediaThumbnailImage and XsMediaListMouseArea
    property real mouseX
    property real mouseY
    property bool hovered: false



    property bool playOnClick: false
    property bool isOnScreen: actorUuidRole == currentPlayhead.mediaUuid

    property var actorUuidRole__: actorUuidRole
    onActorUuidRole__Changed: setSelectionIndex( modelIndex() )

    Connections {
        target: mediaSelectionModel
        function onSelectedIndexesChanged() {
            setSelectionIndex( modelIndex() )
        }
    }

    Connections {
        target: mediaListModelData
        function onSearchStringChanged() {
            setSelectionIndex( modelIndex() )
        }
    }


    XsMediaItemFunctions {
        id: functions
    }

    property var setSelectionIndex: functions.setSelectionIndex
    property var toggleSelection: functions.toggleSelection
    property var exclusiveSelect: functions.exclusiveSelect
    property var inclusiveSelect: functions.inclusiveSelect

    property int selectionIndex: 0

    function modelIndex() {
        return helpers.makePersistent(mediaListModelData.rowToSourceIndex(index))
    }

    property color highlightColor: palette.highlight
    property color bgColorPressed: Qt.darker(palette.highlight, 3) //XsStyleSheet.widgetBgNormalColor
    property color bgColorNormal: "transparent" //XsStyleSheet.widgetBgNormalColor
    //background:
    Rectangle {
        id: itemBg
        anchors.fill: parent
        color: isSelected ? bgColorPressed : bgColorNormal
    }

    Rectangle {
        id: drag_target_indicator
        width: parent.width
        height: visible ? 4 : 0
        visible: isDragTarget
        color: palette.highlight
        Behavior on height { NumberAnimation{duration: 250} }
    }
    Rectangle {
        id: drag_item_bg
        anchors.fill: parent
        visible: dragTargetIndex != undefined && isSelected
        opacity: 0.5
        color: "white"
        z: 100
    }

    property var mediaSourceMetadataFields: mediaDisplayInfoRole

    property bool fieldsReady: typeof mediaSourceMetadataFields == "object"

    property bool isActive: isOnScreen
    property real panelPadding: XsStyleSheet.panelPadding
    property real itemPadding: XsStyleSheet.panelPadding/2

    property real headerThumbWidth: 1
    property color headerThumbColor: XsStyleSheet.widgetBgNormalColor


    // Note: DelegateChooser has a flaw .. if the 'role' value that drives
    // the choice changes AFTER completion, it does not trigger a switch of
    // the DelegateChoice so I've rolled my own
    Component {

        id: chooser
        Item {
            property var what: data_type
            width: loader.width
            height: loader.height
            onWhatChanged: {
                if (what == "flag") {
                    loader.sourceComponent = flag_indicator
                } else if (what == "thumbnail") {
                    loader.sourceComponent = thumbnail
                } else if (what == "index") {
                    loader.sourceComponent = selection_index
                } else if (what == "notes") {
                    loader.sourceComponent = notes_indicator
                } else {
                    loader.sourceComponent = metadata_value
                }
            }
            Loader {
                id: loader
            }
            Component {
                id: flag_indicator
                XsMediaFlagIndicator{
                    width: size
                    height: itemRowHeight
                }
            }
            Component {
                id: metadata_value
                XsMediaTextItem {
                    text: fieldsReady ? index < mediaSourceMetadataFields.length ? mediaSourceMetadataFields[index] : "" : ""
                    width: size
                    height: itemRowHeight
                }
            }
            Component {
                id: selection_index
                XsMediaTextItem {
                    text: selectionIndex ? selectionIndex : ""
                    width: size
                    height: itemRowHeight
                }
            }
            Component {
                id: notes_indicator
                XsMediaNotesIndicator{
                    width: size
                    height: itemRowHeight
                }
            }
            Component {
                id: thumbnail
                XsMediaThumbnailImage {
                    width: size
                    height: itemRowHeight
                    showBorder: isOnScreen
                }
            }
        }
    }

    DelegateModel {

        id: media_columns_model
        model: columns_root_model
        delegate: chooser

    }

    Item {

        width: parent.width
        height: itemRowHeight
        y: drag_target_indicator.height

        Rectangle{
            id: rowDividerLine
            width: parent.width;
            height: headerThumbWidth
            color: XsStyleSheet.widgetBgNormalColor
            anchors.bottom: parent.bottom
            z: 100 // on-top of thumbnails etc.
        }

        ListView {
            anchors.fill: parent
            model: media_columns_model
            orientation: ListView.Horizontal
            interactive: false
        }
    }

    Rectangle {
        id: itemBgBorder
        anchors.fill: parent
        border.color: hovered ? highlightColor : mediaStatusRole && mediaStatusRole != "Online" ? "red" : "transparent"
        border.width: 1
        color: "transparent"
    }
}