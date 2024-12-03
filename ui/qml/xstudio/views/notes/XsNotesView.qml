// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.viewport 1.0

Item{
    id: panel
    anchors.fill: parent

    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding
    property color panelColor: XsStyleSheet.panelBgColor
    property bool showDetails: false

    XsGradientRectangle{ id: backgroundDiv
        anchors.fill: parent
    }

    XsPreference {
        id: note_category_id
        index: globalStoreModel.searchRecursive("/core/bookmark/note_category", "pathRole")
    }
    property alias note_category: note_category_id.value

    XsPreference {
        id: note_colour_id
        index: globalStoreModel.searchRecursive("/core/bookmark/note_colour", "pathRole")
    }
    property alias note_colour: note_colour_id.value

    Item{

        id: titleBar
        width: parent.width;
        height: btnHeight+(panelPadding*2)
        clip: true

        RowLayout{

            x: panelPadding
            spacing: 1
            width: parent.width-(x*2)
            height: btnHeight
            anchors.verticalCenter: parent.verticalCenter

            XsPrimaryButton{

                id: addBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/add.svg"

                toolTip: "Add Note"
                hotkeyNameForTooltip: "Create new note"

                onClicked: {
                    addNote()
                }
            }
            Item{
                Layout.preferredWidth: 1
                Layout.preferredHeight: parent.height
            }

            XsPrimaryButton {
                id: previousBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/chevron_right.svg"
                imageDiv.rotation: 180
                enabled: list.count && currentOnScreenMediaData.values.actorUuidRole ? bookmarkFilterModel.previousBookmark(currentPlayhead.mediaFrame, currentOnScreenMediaData.values.actorUuidRole).valid : false
                onClicked: {
                    let ind = bookmarkFilterModel.previousBookmark(currentPlayhead.mediaFrame, currentOnScreenMediaData.values.actorUuidRole)
                    if(ind.valid) {
                        let sind = bookmarkFilterModel.mapToSource(ind)
                        let owner = sind.model.get(sind, "ownerRole")
                        let start = sind.model.get(sind, "startFrameRole")
                        let frameFrom = sind.model.get(sind, "frameFromTimecodeRole")
                        jumpToNote(owner, start, frameFrom)
                    }
                }
                toolTip: "Jump to Previous Note"
                hotkeyNameForTooltip: "Move backwards to previous note"


            }
            XsPrimaryButton {
                id: nextBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/chevron_right.svg"
                enabled: list.count && currentOnScreenMediaData.values.actorUuidRole ? bookmarkFilterModel.nextBookmark(currentPlayhead.mediaFrame, currentOnScreenMediaData.values.actorUuidRole).valid : false
                onClicked: {
                    let ind = bookmarkFilterModel.nextBookmark(currentPlayhead.mediaFrame, currentOnScreenMediaData.values.actorUuidRole)
                    if(ind.valid) {
                        let sind = bookmarkFilterModel.mapToSource(ind)
                        let owner = sind.model.get(sind, "ownerRole")
                        let start = sind.model.get(sind, "startFrameRole")
                        let frameFrom = sind.model.get(sind, "frameFromTimecodeRole")
                        jumpToNote(owner, start, frameFrom)
                    }
                }
                toolTip: "Jump to Next Note"
                hotkeyNameForTooltip: "Move forwards to next note"

            }
            XsText{
                id: theTitle
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: parent.height
                text: {
                    if (dropdownNoteScope.currentIndex == 0) {
                        return currentOnScreenMediaData.values.nameRole? currentOnScreenMediaData.values.nameRole: ""
                    } else if (dropdownNoteScope.currentIndex == 1) {
                        return viewedMediaSetProperties.values.nameRole? viewedMediaSetProperties.values.nameRole: ""
                    } else {
                        return "All Session Notes"
                    }
                }
                font.bold: true
                elide: Text.ElideMiddle
            }
            Rectangle{
                Layout.preferredWidth: 1
                Layout.preferredHeight: parent.height
                color: XsStyleSheet.menuBarColor
            }
            XsText{
                Layout.minimumWidth: 0
                Layout.preferredWidth: 50
                Layout.preferredHeight: parent.height
                text: "Scope:"
            }
            XsComboBox { id: dropdownNoteScope
                model: ["Media", "Playlist", "Session"]
                Layout.minimumWidth: 0
                Layout.preferredWidth: btnWidth*2.2
                Layout.preferredHeight: parent.height
            }
            // XsPrimaryButton{ id: moreBtn
            //     Layout.minimumWidth: 0
            //     Layout.preferredWidth: btnWidth
            //     Layout.preferredHeight: parent.height
            //     imgSrc: "qrc:/icons/more_vert.svg"
            // }
        }

    }

    function jumpToNote(owner, frame, frameFromTimeCode) {
        var media_idx = theSessionData.searchRecursive(owner, "actorUuidRole", viewportCurrentMediaContainerIndex)

        if(!media_idx.valid) {
            // media for note isn't in current playhead.
            // find it...
            media_idx = theSessionData.searchRecursive(owner, "actorUuidRole")
            if(media_idx.valid) {
                var playist = theSessionData.getPlaylistIndex(media_idx)
                viewportCurrentMediaContainerIndex = playist
            }
        }

        // user has clicked on the 'Go to frame'.
        if (theSessionData.get(viewportCurrentMediaContainerIndex, "typeRole") == "Timeline") {
            // if we are seeking to a note within a timeline, need some different
            // logic here.
            if (media_idx.valid) {

                let frames = theSessionData.mediaFrameToTimelineFrames(
                    viewportCurrentMediaContainerIndex,
                    media_idx,
                    frameFromTimeCode,
                    true // skip disabled clips
                    )

                if(frames.length) {

                    // set the frame to the first timeline frame that shows
                    // this note
                    currentPlayhead.logicalFrame = frames[0]

                    // now, if we can, we want to select the corresponding
                    // clip too
                    let c = theSessionData.getTimelineClipIndexes(viewportCurrentMediaContainerIndex, media_idx);
                    if (c.length) {
                        theSessionData.makeTimelineSelection(viewportCurrentMediaContainerIndex, [c[0]]);
                    }

                    return
                }
            }

        }

        theSessionData.putMediaOnScreen(owner)

        // wait 100ms before setting playhead frame, because it might take
        // a moment for the 'currentPlayheadData' to connect to a playhead
        // if the playhead has changed (if we're jumping to a new playlist)
        callbackTimer.setTimeout(function(frame) { return function() {
            currentPlayhead.logicalFrame = frame
            }}(frame), 100);
    }

    Rectangle{

        id: notesDiv
        x: panelPadding
        y: titleBar.height
        width: panel.width-(x*2)
        height: panel.height-y-panelPadding
        color: panelColor

        XsListView {
            id: list
            anchors.fill: parent

            property real listItemSpacing: panelPadding
            property real listItemWidth: width - rightSpacing
            property real listItemHeight: 20 * 6 //6*notesDelegate.itemHeight
            property int rightSpacing: list.height < list.contentHeight ? 12 : 0
            Behavior on rightSpacing {NumberAnimation {duration: 150}}

            model: DelegateModel {
                id: chooserModel
                model: bookmarkFilterModel
                delegate: XsNotesItemDelegate{

                    id: notesDelegate
                    width: list.listItemWidth
                    height: list.listItemHeight
                    listItemSpacing: list.listItemSpacing
                    isActive: currentPlayhead.mediaFrame >= startFrameRole && currentPlayhead.mediaFrame <= endFrameRole && currentOnScreenMediaData.values.actorUuidRole == ownerRole
                }
            }
        }

        XsLabel {
            anchors.fill: parent
            visible: !list.count
            text: 'Click the "+" button to add a Note'
            color: XsStyleSheet.hintColor
            font.pixelSize: XsStyleSheet.fontSize*1.2
            font.weight: Font.Medium
        }
    }

    property var mediaOrder: updateMediaOrder()

    property var playlistFollower: viewportCurrentMediaContainerIndex
    onPlaylistFollowerChanged: {
        theTitle
    }
    function updateTitle() {
        theTitle
    }

    function updateMediaOrder() {

        // here we make a map of media index (from the current playlist)
        // with the media uuid as the index. This is used by XsBookmarkFilterModel
        // to order the notes according to the media order in the playlist
        let result = {}
        if(viewportCurrentMediaContainerIndex.valid) {

            let model = viewportCurrentMediaContainerIndex.model

            // from playlist, to get to the media list within we go to first row/column
            let mediaind = viewportCurrentMediaContainerIndex.model.index(0, 0, viewportCurrentMediaContainerIndex)

            let count = model.rowCount(mediaind)
            for(let i=0;i<count;i++) {
                result[model.get(model.index(i,0,mediaind), "actorUuidRole")] = i
            }

        }
        return result
    }

    function addNote() {
        if(bookmarkModel.insertRows(bookmarkModel.rowCount(), 1)) {

            var nm = currentOnScreenMediaData.values.nameRole
            if (nm == "New Media") {
                nm = helpers.fileFromURL(currentOnScreenMediaSourceData.values.pathRole)
            }

            // There are 705600000 flicks in one second
            var media_position_flicks = Math.round(currentPlayhead.mediaFrame*705600000.0/currentOnScreenMediaSourceData.values.rateFPSRole)

            let ind = bookmarkModel.index(bookmarkModel.rowCount()-1, 0)
            bookmarkModel.set(ind, currentOnScreenMediaData.values.actorUuidRole, "ownerRole")
            bookmarkModel.set(ind, media_position_flicks, "startRole")
            bookmarkModel.set(ind, nm, "subjectRole")
            bookmarkModel.set(ind, 0, "durationRole")
            bookmarkModel.set(ind, note_category, "categoryRole")  //#TODO: preferences error
            bookmarkModel.set(ind, note_colour, "colourRole")
        }
    }

    function deleteNote(uuid) {
        var idx = bookmarkModel.searchRecursive(uuid, "uuidRole")
        if(idx.valid) {
            bookmarkModel.removeRows(idx.row, 1, idx.parent)
        }
    }

    XsBookmarkFilterModel {
        id: bookmarkFilterModel
        sourceModel: bookmarkModel
        currentMedia: currentPlayhead.mediaUuid // this property is made visible by XsSessionWindow
        showHidden: true
        depth: dropdownNoteScope.currentIndex
        mediaOrder: panel.mediaOrder
    }

}