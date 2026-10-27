// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

import xStudio 1.0

XsGradientRectangle {

    id: panel
    //anchors.fill: parent
    property color bgColorPressed: XsStyleSheet.accentColor
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property real textSize: XsStyleSheet.fontSize
    property color textColorNormal: XsStyleSheet.primaryTextColor
    property color hintColor: XsStyleSheet.hintColor

    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    property int inHandle: 0
    property int outHandle: 0
    property int cutRangeIn: 0
    property int cutRangeOut: 0
    property int editBoxWidth: 34
    property int editBoxHeight: 15

    property bool haveCurrentClip: false
    property var currentClipIndex: null

    //#TODO: test
    property bool showIcons: false

    property alias theTimeline: theTimeline
    property alias timelineProperties: timelineProperties

    property bool hideMarkers: false
    property string timeMode: "timecode"
    property real verticalScale: 1.0

    property bool buttonsShowText: width > 1450
    property real iconTextBtnWidth: buttonsShowText ? btnWidth*2.2 : btnWidth

    Behavior on iconTextBtnWidth {NumberAnimation {duration: 150}}

    // persist these properties between sessions
    XsStoredPanelProperties {
        propertyNames: ["hideMarkers", "verticalScale", "timeMode"]
    }

    /* This gives us direct access to the properties of the current (active)
    playlist - for example viewedMediaSetProperties.values.nameRole gives us
    the playlist name */
    XsModelPropertyMap {
        id: timelineProperties
        index: timelineIndex.parent
    }

    XsModelPropertyMap {
        id: parentPlaylistProperties
        index: timelineIndex.parent.parent.parent
    }
    property var timelineName: timelineProperties.values.nameRole ? parentPlaylistProperties.values.nameRole + " / " + timelineProperties.values.nameRole : ""

    XsModelPropertyMap {
        id: currentClipProperties
        index: currentClipIndex || theSessionData.index(-1,-1)
        onContentChanged: updateCurrentClipDetail()
        onIndexChanged: updateCurrentClipDetail()
    }

    function nTimer() {
        return Qt.createQmlObject("import QtQuick; Timer {}", appWindow);
    }

    function delay(delayTime, cb) {
         let timer = new nTimer();
         timer.interval = delayTime;
         timer.repeat = false;
         timer.triggered.connect(cb);
         timer.start();
    }

    function updateCurrentClipDetail() {
        if(currentClipProperties.index && currentClipProperties.index.valid) {
            let model = currentClipProperties.index.model
            let tindex = model.getPlaylistIndex(currentClipProperties.index)
            let mlist = model.index(0, 0, tindex)
            let mediaIndex = model.search(currentClipProperties.values.clipMediaUuidRole, "actorUuidRole", mlist)
            if(model.canFetchMore(mediaIndex)) {
                model.fetchMore(mediaIndex)
                delay(250, function() {panel.updateCurrentClipDetail()})
            } else {
                let mediaSourceIndex = model.search(
                    model.get(mediaIndex, "imageActorUuidRole"),
                    "actorUuidRole", mediaIndex
                )
                let taf = model.get(mediaSourceIndex, "timecodeAsFramesRole")
                if(taf == undefined) {
                    delay(250, function() {panel.updateCurrentClipDetail()} )
                } else {
                    // let name = currentClipProperties.values.nameRole
                    let start = currentClipProperties.values.trimmedStartRole
                    let astart = currentClipProperties.values.availableStartRole
                    let duration = currentClipProperties.values.trimmedDurationRole
                    let head = start - astart
                    let tail = currentClipProperties.values.availableDurationRole - head - duration

                    start = start - astart + taf
                    let end = start + duration - 1

                    cutRangeIn = start
                    cutRangeOut = end
                    inHandle = head
                    outHandle = tail
                    haveCurrentClip = true
                }
            }
        } else {
            haveCurrentClip = false
        }
    }

    function setCutRange(text, what) {
        let v = parseInt(text)

        if(currentClipProperties.index && currentClipProperties.index.valid) {
            let model = currentClipProperties.index.model
            let tindex = model.getPlaylistIndex(currentClipProperties.index)
            let mlist = model.index(0, 0, tindex)
            let mediaIndex = model.search(currentClipProperties.values.clipMediaUuidRole, "actorUuidRole", mlist)
            let mediaSourceIndex = model.search(
                model.get(mediaIndex, "imageActorUuidRole"),
                "actorUuidRole", mediaIndex
            )
            let taf = model.get(mediaSourceIndex, "timecodeAsFramesRole")
            if(taf == undefined) {
                return
            } else {
                // let name = currentClipProperties.values.nameRole
                let start = currentClipProperties.values.trimmedStartRole
                let astart = currentClipProperties.values.availableStartRole
                let duration = currentClipProperties.values.trimmedDurationRole
                let head = start - astart
                let tail = currentClipProperties.values.availableDurationRole - head - duration

                start = start - astart + taf
                let end = start + duration - 1

                cutRangeIn = start
                cutRangeOut = end
                inHandle = head
                outHandle = tail
                haveCurrentClip = true

                if (what == 0) { // cutIn
                    let d = v-cutRangeIn
                    currentClipProperties.values.activeStartRole = currentClipProperties.values.activeStartRole + d
                    currentClipProperties.values.activeDurationRole = currentClipProperties.values.activeDurationRole - d
                } else if (what == 1) { // cutOut
                    let d = cutRangeOut-v
                    currentClipProperties.values.activeDurationRole = currentClipProperties.values.activeDurationRole - d
                } else if (what == 2) { // inHandle
                    let d = v-inHandle
                    currentClipProperties.values.activeStartRole = currentClipProperties.values.activeStartRole + d
                    currentClipProperties.values.activeDurationRole = currentClipProperties.values.activeDurationRole - d
                } else if (what == 3) { // outHandle
                    let d = v-outHandle
                    currentClipProperties.values.activeDurationRole = currentClipProperties.values.activeDurationRole - d
                }

                updateCurrentClipDetail()
            }
        }        
    }

    /*XsPlayhead {
        id: timelinePlayhead
        Component.onCompleted: {
            connectToModel(0)
        }

        function connectToModel(retry) {

            // connect to the timeline playhead ...
            let playhead_idx = theSessionData.searchRecursive(
                "Playhead",
                "typeRole",
                theTimeline.timelineModel.rootIndex.parent
                )

            if (playhead_idx.valid) {
                let playhead_uuid = theSessionData.get(playhead_idx, "actorUuidRole")
                if (playhead_uuid == undefined) {
                    // uh-oh - remember the session model is populated asynchronously
                    // we might need to wait few milliseconds until "actorUuidRole" for
                    // the playhead has been filled in.
                    if (retry < 3) {
                        callbackTimer.setTimeout(function() { return function() {
                            connectToModel(retry+1)
                        }}(), 200);
                    }
                } else {
                    timelinePlayhead.uuid = playhead_uuid
                }
            } else if (retry < 3) {
                callbackTimer.setTimeout(function() { return function() {
                    connectToModel(retry+1)
                }}(), 200);
                return;
            }

            let sind = theSessionData.searchRecursive("PlayheadSelection", "typeRole", theTimeline.timelineModel.rootIndex.parent)
            if (sind.valid) {
                timelinePlayheadSelectionIndex = helpers.makePersistent(sind)
            }
        }
    }

    Connections {

        target: theTimeline.timelineModel

        function onRootIndexChanged() {

            if (!theTimeline.timelineModel.rootIndex.valid) {
                timelinePlayhead.uuid = undefined
            } else {
                timelinePlayhead.connectToModel(0)
            }
        }
    }*/

    Connections {
        target: timelinePlayhead
        function onMediaUuidChanged() {
            updateClipIndex()
        }
    }

    function updateClipIndex() {
        currentClipIndex = helpers.makePersistent(theSessionData.getTimelineClipIndex(theTimeline.timelineModel.rootIndex, timelinePlayhead.logicalFrame))
    }

    XsHotkeyArea {
        id: hotkey_area
        anchors.fill: parent
        context: hotkey_area_id
        focus: true
    }

    XsFocusRemover {
        target: hotkey_area
    }

    property var hotkey_area_id: "timeline" + hotkey_area

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if(!isPlayheadActive) {

                if (!multiTimelineMode) {
                    viewportCurrentMediaContainerIndex = theTimeline.timelineModel.rootIndex.parent
                }

                // we ensure the timeline playhead is back in 'pinned' mode. This
                // means, regardless of the media selection, the playhead source
                // is pinned to the timeline itslef and isn't going to use the
                // selected media as it's source (which is usual behaviour)
                timelinePlayhead.pinnedSourceMode = true

            }
        }
        onExited: {
            if(theTimeline.scrollingModeActive || theTimeline.scalingModeActive)
                helpers.restoreOverrideCursor()
        }
        onEntered: {
            if(theTimeline.scrollingModeActive)
                helpers.setOverrideCursor("Qt.OpenHandCursor")
            else if(theTimeline.scalingModeActive)
                helpers.setOverrideCursor("://cursors/magnifier_cursor.svg")
        }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4

        Rectangle{
            Layout.maximumHeight: btnHeight
            Layout.minimumHeight: btnHeight
            Layout.fillWidth: true
            Layout.leftMargin: btnWidth + 4
            color: bgColorNormal

            enabled: theTimeline.have_timeline
            opacity: isPlayheadActive ? 1.0 : (enabled ? 0.4 : 1.0)
            Behavior on opacity {NumberAnimation {duration: 150}}

            RowLayout{
                spacing: 2
                anchors.fill: parent

                XsPrimaryButton{ 
                    Layout.preferredWidth: btnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/content_copy.svg"
                    text: "Copy selected tracks/clips to clipboard" + copy_key.seqDisplay
                    onClicked: {
                        clipboard.text = theSessionData.copyTimelineItemsToClipboard(theTimeline.timelineSelection.selectedIndexes, timelineIndex)
                    }
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                    XsHotkeyReference {
                        id: copy_key
                        hotkeyName: "Timeline Copy Selected Items to Clipboard"
                        property var seqDisplay: sequence != "" ? "  (" + sequence + ")": ""
                    }
                }

                XsPrimaryButton{ 
                    Layout.preferredWidth: btnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/content_paste.svg"
                    text: "Paste tracks/clips from clipboard" + paste_key.seqDisplay
                    onClicked: theSessionData.pasteFromClipboard(clipboard.text, timelineIndex)
                    enabled: clipboard.text.startsWith("COPIED_CLIPS")
                    XsHotkeyReference {
                        id: paste_key
                        hotkeyName: "Timeline Paste Items from Clipboard"
                        property var seqDisplay: sequence != "" ? "  (" + sequence + ")": ""
                    }

                }

                XsPrimaryButton{ id: deleteBtn
                    Layout.preferredWidth: btnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/delete.svg"
                    text: "Delete"
                    onClicked: theTimeline.deleteItems(theTimeline.timelineSelection.selectedIndexes)
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                }
                XsPrimaryButton{
                    Layout.preferredWidth: btnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/undo.svg"
                    text: "Undo"
                    onClicked: theTimeline.undo(viewedMediaSetProperties.index)
                }
                XsPrimaryButton{
                    Layout.preferredWidth: btnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/redo.svg"
                    text: "Redo"
                    onClicked:  theTimeline.redo(viewedMediaSetProperties.index)
                }
                XsSearchButton{ id: searchBtn
                    Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
                    Layout.preferredHeight: parent.height
                    isExpanded: false
                    hint: "Search..."
                    onTextChanged: {
                        if(text.length)
                            theTimeline.timelineSelection.select(
                                helpers.createItemSelection(
                                    theSessionData.getIndexesByName(
                                        theTimeline.timelineModel.rootIndex, text, "Clip"
                                    )
                                ),
                                ItemSelectionModel.ClearAndSelect
                            )

                    }
                    onEditingCompleted: {
                        // jump to first clip
                        if(theTimeline.timelineSelection.selectedIndexes.length) {
                            let frame = theSessionData.startFrameInParent(theTimeline.timelineSelection.selectedIndexes[0])
                            timelinePlayhead.logicalFrame = frame
                        }
                        forceActiveFocus(panel)
                    }
                }

                Item{
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 12
                }

                XsPrimaryButton{
                    Layout.leftMargin: 16
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/waves.svg"
                    text: "Ripple"
                    toolTip: "Ripple"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    isActive: theTimeline.rippleMode
                    onClicked: {
                        theTimeline.rippleMode = !theTimeline.rippleMode
                        theTimeline.overwriteMode = false
                    }
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    visible: true
                    imgSrc: "qrc:/icons/filter_none.svg"
                    text: "Overwrite"
                    toolTip: "Overwrite"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    isActive: theTimeline.overwriteMode
                    onClicked: {
                        theTimeline.overwriteMode = !theTimeline.overwriteMode
                        theTimeline.rippleMode = false
                    }
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/vertical_align_center.svg"
                    imageDiv.rotation: 90
                    text: "Snap"
                    toolTip: "Snap"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    isActive: theTimeline.snapMode
                    onClicked: theTimeline.snapMode = !theTimeline.snapMode
                }

                Item{
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 12
                }

                XsPrimaryButton{
                    Layout.leftMargin: 16
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/crop_free.svg"
                    text: "Fit All"
                    toolTip: "Fit All"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    font.family: XsStyleSheet.fontFamily
                    onClicked:  theTimeline.fitItems()
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/fit_screen.svg"
                    text: "Selected"
                    toolTip: "Fit Selected"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                    onClicked:  theTimeline.fitItems(theTimeline.timelineSelection.selectedIndexes)
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/laps.svg"
                    text: "Loop"
                    toolTip: "Loop Selection"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    onClicked: theTimeline.loopSelection = !theTimeline.loopSelection
                    isActive: theTimeline.loopSelection
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/center_focus_weak.svg"
                    text: "Focus"
                    toolTip: "Focus Selection"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    onClicked: theTimeline.focusSelection = !theTimeline.focusSelection
                    isActive: theTimeline.focusSelection
                }
                Item{
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 12
                }

                XsPrimaryButton{
                    Layout.leftMargin: 16
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/stacks.svg"
                    text: "Flatten"
                    toolTip: "Flatten Selected Tracks"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                    onClicked: {
                        theSessionData.bakeTimelineItems(theTimeline.timelineSelection.selectedIndexes)
                        theTimeline.deleteItems(theTimeline.timelineSelection.selectedIndexes)
                    }
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/splitscreen_add.svg"
                    text: "Insert"
                    toolTip: "Insert Track Above"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                    onClicked:  theTimeline.insertTrackAbove(theTimeline.timelineSelection.selectedIndexes)
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/library_add.svg"
                    text: "Duplicate"
                    toolTip: "Duplicate Selected"
                    showBoth: buttonsShowText
                    font.pixelSize: XsStyleSheet.fontSize
                    onClicked: theTimeline.duplicate(theTimeline.timelineSelection.selectedIndexes)
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                }

                Item{
                    Layout.fillWidth: true
                }


                ColumnLayout {

                    spacing: 2

                    XsText{
                        Layout.alignment: Qt.AlignHCenter
                        text: "Cut Range"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2
                        XsTextField {
                            Layout.preferredWidth: editBoxWidth
                            Layout.preferredHeight: editBoxHeight
                            text: cutRangeIn
                            font.pixelSize: 10
                            horizontalAlignment: TextInput.AlignHCenter
                            onEditingFinished: setCutRange(text, 0)
                            
                        }
                        XsText {
                            text: "-"
                            font.pixelSize: 10
                        }
                        XsTextField {
                            Layout.preferredWidth: editBoxWidth
                            Layout.preferredHeight: editBoxHeight
                            text: cutRangeOut
                            font.pixelSize: 10
                            horizontalAlignment: TextInput.AlignHCenter
                            onEditingFinished: setCutRange(text, 1)

                        }
                    }
                }

                ColumnLayout {

                    Layout.leftMargin: 10
                    spacing: 2
                    XsText {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Handles"
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 10
                    }

                    RowLayout {

                        spacing: 2
                        XsTextField {
                            Layout.preferredWidth: editBoxWidth
                            Layout.preferredHeight: editBoxHeight
                            text: inHandle
                            font.pixelSize: 10
                            horizontalAlignment: TextInput.AlignHCenter
                            onEditingFinished: setCutRange(text, 2)
                        }
                        XsText {
                            text: "/"
                            font.pixelSize: 10
                        }
                        XsTextField {
                            Layout.preferredWidth: editBoxWidth
                            Layout.preferredHeight: editBoxHeight
                            text: outHandle
                            font.pixelSize: 10
                            horizontalAlignment: TextInput.AlignHCenter
                            onEditingFinished: setCutRange(text, 3)
                        }
                    }                

                }

                Item {
                    Layout.preferredWidth: 8
                }

            }
        }

        Rectangle{
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: theTimeline.trackBackground

            enabled: theTimeline.have_timeline
            opacity: isPlayheadActive ? 1.0 : (enabled ? 0.4 : 1.0)
            Behavior on opacity {NumberAnimation {duration: 150}}

            RowLayout {
                anchors.fill: parent
                Rectangle{
                    Layout.fillHeight: true
                    Layout.minimumWidth: btnWidth
                    Layout.maximumWidth: btnWidth
                    color: theTimeline.trackBackground

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 2

                        XsPrimaryButton{
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Select"
                            isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/arrow_selector_tool.svg"
                            isActive: theTimeline.editMode == text
                            onClicked: theTimeline.editMode = text
                        }

                        XsPrimaryButton{
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Move"
                            isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/open_with.svg"
                            isActive: theTimeline.editMode == text
                            onClicked: theTimeline.editMode = text
                        }

                        XsPrimaryButton{
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Trim"
                            isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/horizontal_align_center.svg"
                            isActive: theTimeline.editMode == text
                            onClicked: theTimeline.editMode = text
                        }

                        XsPrimaryButton{
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Roll"
                            isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/expand.svg"
                            onClicked: theTimeline.editMode = text
                            isActive: theTimeline.editMode == text
                            imageDiv.rotation: 90
                        }

                        XsPrimaryButton{
                            // Layout.topMargin: 8
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Cut"
                            isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/content_cut.svg"
                            isActive: theTimeline.editMode == text
                            onClicked: {
                                theTimeline.editMode = text
                                theTimeline.snapCacheKey = helpers.makeQUuid()
                            }
                        }

                        XsPrimaryButton{
                            // Layout.topMargin: 8
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Zoom"
                            // isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/zoom_in.svg"
                            onClicked: theTimeline.scalingModeActive = !theTimeline.scalingModeActive
                            isActive: theTimeline.scalingModeActive
                            hotkeyNameForTooltip: "Timeline Zoom"
                        }

                        XsPrimaryButton{
                            // Layout.topMargin: 8
                            Layout.minimumHeight: btnHeight
                            Layout.maximumHeight: btnHeight
                            Layout.fillWidth: true
                            text: "Pan"
                            // isActiveIndicatorAtLeft: true
                            imgSrc: "qrc:/icons/pan.svg"
                            onClicked: theTimeline.scrollingModeActive = !theTimeline.scrollingModeActive
                            isActive: theTimeline.scrollingModeActive
                            hotkeyNameForTooltip: "Timeline Scroll"
                        }

                        Item {
                            Layout.fillHeight: true
                        }


                        // XsPrimaryButton{
                        //     text: "Reorder"
                        //     isActiveIndicatorAtLeft: true
                        //     imgSrc: "qrc:/icons/repartition.svg"
                        //     onClicked: theTimeline.editMode = text
                        // }


                    }
                }

                XsTimeline {
                    id: theTimeline
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }
}
}
