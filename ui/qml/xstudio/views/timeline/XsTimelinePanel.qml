// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15
import QtQml 2.14
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

import xStudio 1.0

XsGradientRectangle{

    id: panel
    anchors.fill: parent

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property real textSize: XsStyleSheet.fontSize
    property var textFont: XsStyleSheet.fontFamily
    property color textColorNormal: palette.text
    property color hintColor: XsStyleSheet.hintColor

    property real iconTextBtnWidth: btnWidth*2.2
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    property var currentClipRange: []
    property var currentClipHandles: []
    property var currentClipIndex: null
    property var timelinePlayheadSelectionIndex: null

    //#TODO: test
    property bool showIcons: false

    property alias theTimeline: theTimeline

    property bool hideMarkers: false
    property string timeMode: "timecode"
    property real verticalScale: 1.0

    property bool isPlayheadActive: timelinePlayhead.pinnedSourceMode ? currentPlayhead.uuid == timelinePlayhead.uuid : false

    // persist these properties between sessions
    XsStoredPanelProperties {
        propertyNames: ["hideMarkers", "verticalScale", "timeMode"]
    }

    XsModelPropertyMap {
        id: currentClipProperties
        index: currentClipIndex || theSessionData.index(-1,-1)
        onContentChanged: updateCurrentClipDetail()
        onIndexChanged: updateCurrentClipDetail()
    }

    function nTimer() {
         return Qt.createQmlObject("import QtQuick 2.0; Timer {}", appWindow);
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
                delay(250, function() {updateCurrentClipDetail()})
            } else {
                let mediaSourceIndex = model.search(
                    model.get(mediaIndex, "imageActorUuidRole"),
                    "actorUuidRole", mediaIndex
                )
                let taf = model.get(mediaSourceIndex, "timecodeAsFramesRole")
                if(taf == undefined) {
                    delay(250, function() {updateCurrentClipDetail()} )
                } else {
                    // let name = currentClipProperties.values.nameRole
                    let start = currentClipProperties.values.trimmedStartRole
                    let astart = currentClipProperties.values.availableStartRole
                    let duration = currentClipProperties.values.trimmedDurationRole
                    let head = start - astart
                    let tail = currentClipProperties.values.availableDurationRole - head - duration

                    start = start - astart + taf
                    let end = start + duration - 1

                    currentClipRange = [start, end]
                    currentClipHandles = [head, tail]
                }
            }
        } else {
            currentClipRange = []
            currentClipHandles = []
        }
    }

    XsPlayhead {
        id: timelinePlayhead
        Component.onCompleted: {
            connectToModel()
        }
        function connectToModel() {

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
                    callbackTimer.setTimeout(function(index) { return function() {
                        timelinePlayhead.uuid = theSessionData.get(playhead_idx, "actorUuidRole")
                    }}( playhead_idx ), 200);
                } else {
                    timelinePlayhead.uuid = playhead_uuid
                }
            }

            let sind = theSessionData.searchRecursive("PlayheadSelection", "typeRole", theTimeline.timelineModel.rootIndex.parent)

            if (sind.valid) {
                timelinePlayheadSelectionIndex = helpers.makePersistent(sind)
            }
        }
    }

    property alias timelinePlayhead: timelinePlayhead

    Connections {

        target: theTimeline.timelineModel

        function onRootIndexChanged() {

            timelinePlayhead.connectToModel()
        }
    }

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
        context: "timeline"
        focus: true
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if(!isPlayheadActive) {
                viewportCurrentMediaContainerIndex = theTimeline.timelineModel.rootIndex.parent

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

                XsPrimaryButton{
                    Layout.leftMargin: 16
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/waves.svg"
                    text: "Ripple"
                    toolTip: "Ripple"
                    showBoth: true
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
                    showBoth: true
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
                    showBoth: true
                    font.pixelSize: XsStyleSheet.fontSize
                    isActive: theTimeline.snapMode
                    onClicked: theTimeline.snapMode = !theTimeline.snapMode
                }

                Item{
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 24
                }

                XsPrimaryButton{
                    Layout.leftMargin: 16
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/crop_free.svg"
                    text: "Fit All"
                    toolTip: "Fit All"
                    showBoth: true
                    font.pixelSize: XsStyleSheet.fontSize
                    onClicked:  theTimeline.fitItems()
                }
                XsPrimaryButton{
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/fit_screen.svg"
                    text: "Selected"
                    toolTip: "Fit Selected"
                    showBoth: true
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
                    showBoth: true
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
                    showBoth: true
                    font.pixelSize: XsStyleSheet.fontSize
                    onClicked: theTimeline.focusSelection = !theTimeline.focusSelection
                    isActive: theTimeline.focusSelection
                }
                Item{
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 24
                }

                XsPrimaryButton{
                    Layout.leftMargin: 16
                    Layout.preferredWidth: iconTextBtnWidth
                    Layout.preferredHeight: parent.height
                    imgSrc: "qrc:/icons/stacks.svg"
                    text: "Flatten"
                    toolTip: "Flatten Selected Tracks"
                    showBoth: true
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
                    showBoth: true
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
                    showBoth: true
                    font.pixelSize: XsStyleSheet.fontSize
                    onClicked: theTimeline.duplicate(theTimeline.timelineSelection.selectedIndexes)
                    enabled: theTimeline.timelineSelection.selectedIndexes.length
                }

                Item{
                    Layout.fillWidth: true
                }


                RowLayout {
                    Layout.preferredWidth: btnWidth*5
                    Layout.maximumWidth: btnWidth*5
                    Layout.fillHeight: true

                    ColumnLayout {
                        Layout.preferredWidth: btnWidth*1.8
                        Layout.maximumWidth: btnWidth*1.8
                        Layout.fillHeight: true

                        XsText{
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            elide: Text.ElideMiddle
                            text: "Cut Range:"
                            horizontalAlignment: Text.AlignRight
                        }

                        XsText{
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            elide: Text.ElideMiddle
                            text: "Handles:"
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        XsText{
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            font.bold: true
                            font.family: XsStyleSheet.fixedWidthFontFamily
                            elide: Text.ElideMiddle
                            text: currentClipRange.length ? currentClipRange[0] : ""
                            horizontalAlignment: Text.AlignRight
                        }

                        XsText{
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            font.bold: true
                            font.family: XsStyleSheet.fixedWidthFontFamily
                            elide: Text.ElideMiddle
                            text: currentClipHandles.length ? (Math.abs(currentClipHandles[0]) + (currentClipHandles[0] < 0 ? " -" : "")) : ""
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        XsText{
                            Layout.fillHeight: true
                            text: " - "
                        }

                        XsText{
                            Layout.fillHeight: true
                            text: " / "
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        XsText{
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            font.bold: true
                            elide: Text.ElideMiddle
                            text: currentClipRange.length ? currentClipRange[1] : ""
                            font.family: XsStyleSheet.fixedWidthFontFamily
                            horizontalAlignment: Text.AlignLeft
                        }

                        XsText{
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            font.bold: true
                            elide: Text.ElideMiddle
                            font.family: XsStyleSheet.fixedWidthFontFamily
                            text: currentClipHandles.length ? (Math.abs(currentClipHandles[1]) + (currentClipHandles[1] < 0 ? " -" : "")): ""
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
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
