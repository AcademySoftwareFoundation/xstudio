// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14

import xStudio 1.1

XsWindow {
    id: dialog
    title: "Notes"
    width: 700
    height: 600
    minimumWidth: 400
    minimumHeight: actionbar.height + 125

    centerOnOpen: true

    property var playerWidget: sessionWidget.playerWidget
    property var playhead: sessionWidget.viewport.playhead
    property var currentMediaUuid: playhead ? playhead.mediaUuid : null

    XsWindowStateSaver
    {
        windowObj: dialog
        windowName: "note_window"
    }

    // onPlayheadChanged: bookmarks_model.update()

    property var mediaOrder: updateMediaOrder()

    function updateMediaOrder() {
        let result = {}

        if(app_window.screenSource.index.valid) {
            let model = app_window.screenSource.index.model

            // playlist, force media order up a level if needed.

            let mediaind = model.index(0, 0, app_window.screenSource.index)

            if(filterDepth.currentIndex == 2) {
                if(model.get(app_window.screenSource.index, "typeRole") != "Playlist") {
                    mediaind = model.index(0, 0, app_window.screenSource.index.parent.parent)
                }
            }

            let count = model.rowCount(mediaind)
            for(let i=0;i<count;i++) {
                result[model.get(model.index(i,0,mediaind), "actorUuidRole")] = i
            }

        }

        return result
    }

    function containsMedia(index, media_uuid) {
        let model = index.model
        return model.search_recursive(media_uuid, "actorUuidRole", index).valid
    }

    function getMediaIndex(media_uuid) {
        let model = app_window.screenSource.index.model
        let mediaind = model.index(0, 0, app_window.screenSource.index)
        return model.search_recursive(media_uuid, "actorUuidRole", mediaind)
    }

    XsBookmarkFilterModel {
        id: filter
        sourceModel: app_window.bookmarkModel
        mediaOrder: dialog.mediaOrder
        currentMedia: dialog.currentMediaUuid
        depth: filterDepth.currentIndex
    }

    DelegateModel {
        id: bookmarks_model

        property var srcModel: filter
        onSrcModelChanged: model = srcModel

        delegate: Rectangle {
            id: delegate_rect
            property int pad_height: 6
            property int pad_width: 16

            property var tmpCategoryRole: categoryRole

            property var hasFocus: dialog.currentMediaUuid && dialog.currentMediaUuid == ownerRole && playhead.mediaFrame >= startFrameRole && playhead.mediaFrame <= endFrameRole
            color: hasFocus ? XsStyle.highlightColor : XsStyle.mainBackground

            // this triggers a QML bug..
            onHasFocusChanged: {
                if(hasFocus && DelegateModel.ivisibleIndex !== undefined) {
                    view.lastFocus = DelegateModel.ivisibleIndex
                }
            }

            border.color: XsStyle.controlBackground
            border.width : 1
            height: 120
            width: view.width

            XsMenu {
                id: contextMenu
                title: qsTr("Pop")

                XsMenuItem {
                    mytext: qsTr("Remove Note")
                    onTriggered: bookmarks_model.model.removeRows(index, 1, bookmarks_model.parentModelIndex())
                }

                XsMenuItem {
                    mytext: qsTr("Remove All Notes")
                    onTriggered: bookmarks_model.model.removeRows(0, bookmarks_model.count, bookmarks_model.parentModelIndex())
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton | Qt.LeftButton
                onPressed: {
                    if(mouse.button == Qt.RightButton) {
                        focus = true
                        contextMenu.popup()
                    }
                    if(mouse.button == Qt.LeftButton) {
                        focus = true
                    }
                }
            }

            RowLayout{
                id: item
                anchors.fill: parent
                anchors.margins: 2
                spacing: 1

                // one
                Rectangle {
                    color: XsStyle.mainBackground
                    Layout.fillHeight: true
                    Layout.preferredWidth:150


                    Timer {
                        id: delayJump
                        interval: 1000
                        running: false
                        repeat: false
                        onTriggered: {playhead.jumpToSource(ownerRole);playhead.frame = (playhead.frame - playhead.mediaFrame) + startFrameRole}
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        onClicked: {
                            if(currentMediaUuid == ownerRole ) {
                                playhead.frame = (playhead.frame - playhead.mediaFrame) + startFrameRole
                            } else if(containsMedia(app_window.screenSource.index, ownerRole)) {

                                let mind = getMediaIndex(ownerRole)
                                // console.log(ownerRole, mind, app_window.mediaSelectionModel.isSelected(mind))

                                if(app_window.mediaSelectionModel.isSelected(mind)){
                                } else {
                                    app_window.mediaSelectionModel.select(mind, ItemSelectionModel.ClearAndSelect)
                                }
                                delayJump.start()
                            } else {
                                // not implemented
                            }
                        }
                        hoverEnabled: true
                        onHoveredChanged: gotoframe.visible = containsMouse
                    }

                    ColumnLayout{
                        anchors.fill: parent
                        spacing: 2
                        Image {
                            id: icon
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.maximumWidth:150
                            Layout.maximumHeight:150
                            Layout.preferredWidth:150
                            Layout.preferredHeight:150
                            asynchronous: true
                            fillMode: Image.PreserveAspectFit
                            cache: true
                            source: thumbnailRole != undefined ? thumbnailRole : ""
                            smooth: true
                            transformOrigin: Item.Center

                            XsLabel {
                                z:1
                                id: gotoframe
                                anchors.fill: icon
                                style: Text.Outline
                                styleColor: "black"
                                text: "Go To Frame"
                                visible: false
                                color: XsStyle.highlightColor
                                opacity: 0.80
                                font.pixelSize: XsStyle.popupControlFontSize * 1.5
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }

                        XsLabel {
                            text: frameFromTimecodeRole != undefined ? "Frame " + String(frameFromTimecodeRole).padStart(4, '0') : ""
                            Layout.fillWidth: true
                            Layout.margins: 2
                            horizontalAlignment: TextEdit.AlignHCenter
                            verticalAlignment: TextEdit.AlignVCenter
                        }
                    }
                }
                // Two
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 1

                    Rectangle {
                        color: XsStyle.controlBackground
                        Layout.fillWidth: true//lab.implicitWidth
                        Layout.minimumHeight: subject_label.implicitHeight+pad_height
                        // Layout.minimumWidth: children[0].implicitWidth+pad_width

                        RowLayout {
                            anchors.margins: 4
                            anchors.fill: parent

                            // decorators
                            Repeater {
                                model: (
                                    app_window.sessionModel.tags[uuidRole] ? (
                                        app_window.sessionModel.tags[uuidRole]["Decorator"] ? app_window.sessionModel.tags[uuidRole]["Decorator"].tags : null
                                    )
                                    : null
                                )

                                XsDecoratorWidget {
                                    Layout.topMargin: 2
                                    Layout.bottomMargin: 2
                                    Layout.leftMargin: 3
                                    Layout.preferredWidth: subject_label.height
                                    Layout.preferredHeight: subject_label.height
                                    Layout.alignment: Qt.AlignVCenter|Qt.AlignHCenter
                                    widgetString: modelData.data
                                }
                            }

                            XsLabel {
                                id: subject_label
                                font.italic: true
                                color: XsStyle.mainColor
                                opacity: 0.8
                                text: "Subject: "
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                            XsTextInput {
                                clip: true
                                Layout.fillWidth: true//lab.implicitWidth
                                horizontalAlignment: TextEdit.AlignLeft
                                verticalAlignment: TextEdit.AlignVCenter
                                color: XsStyle.highlightColor
                                placeholderText: "Enter subject here..."

                                text: subjectRole != undefined ? subjectRole : ""
                                onEditingFinished: subjectRole = text
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: XsStyle.controlBackground

                        Flickable {
                            id: flick
                            anchors.fill: parent
                            anchors.margins: 4
                            contentWidth: edit.paintedWidth
                            contentHeight: edit.paintedHeight
                            clip: true
                            boundsMovement: Flickable.StopAtBounds

                            function ensureVisible(r) {
                                if (contentX >= r.x)
                                    contentX = r.x;
                                else if (contentX+width <= r.x+r.width)
                                    contentX = r.x+r.width-width;
                                if (contentY >= r.y)
                                    contentY = r.y;
                                else if (contentY+height <= r.y+r.height)
                                    contentY = r.y+r.height-height;
                            }
                            ScrollBar.vertical: ScrollBar {
                                // width: 40
                                // anchors.left: parent.right // adjust the anchor as suggested by derM
                                // policy: ScrollBar.AlwaysOn
                            }
                            XsTextEdit {
                                Keys.onPressed: {
                                    if ((event.key == Qt.Key_Return) && (event.modifiers == Qt.NoModifier)) {
                                        focus = false
                                        event.accepted = true;
                                    }
                                }
                                id: edit
                                placeholderText: "Enter note here..."
                                width: flick.width
                                height: lineCount<=6? flick.height : flick.height*lineCount
                                selectByMouse: true
                                onCursorRectangleChanged: flick.ensureVisible(cursorRectangle)
                                verticalAlignment: Qt.AlignTop
                                horizontalAlignment: Qt.AlignLeft
                                wrapMode: TextEdit.Wrap
                                text: noteRole != undefined ? noteRole : ""
                                onEditingFinished: {
                                    app_window.requestActivate()
                                    noteRole = text
                                }
                            }
                        }
                    }
                }
                // Three
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth:200
                    color: XsStyle.controlBackground

                    GridLayout{
                        anchors.fill: parent
                        columnSpacing: 1
                        rowSpacing: 1
                        columns: 3

                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.fillWidth: true
                            Layout.minimumHeight: date_rect.height
                            Layout.minimumWidth: children[0].implicitWidth + pad_width

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: "transparent"
                            }
                        }

                        Rectangle {
                            id: date_rect
                            color: XsStyle.mainBackground
                            // Layout.columnSpan: 3
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight + pad_height
                            Layout.minimumWidth: children[0].implicitWidth + pad_width

                            XsTextInput {
                                color: XsStyle.highlightColor
                                anchors.fill: parent
                                anchors.margins: 1
                                text:  createdRole != undefined ? createdRole :""
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                                onEditingFinished: createdRole = text
                            }

                        }

                        Rectangle {
                            color: XsStyle.mainBackground
                            // Layout.fillWidth: true//lab.implicitWidth
                            Layout.fillWidth: true
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            // Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsButtonOld {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "X"
                                palette.button: "transparent"
                                onClicked: bookmarks_model.model.removeRows(index, 1, bookmarks_model.parentModelIndex())
                                tooltipTitle:"Notes"
                                tooltip:"Remove Note."
                                // horizontalAlignment: TextEdit.AlignHCenter
                                // verticalAlignment: TextEdit.AlignVCenter
                            }
                        }

                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.columnSpan: 3
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width
                            id: author
                            XsTextInput {
                                anchors.fill: parent
                                anchors.margins: 1
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                                color: XsStyle.highlightColor

                                text: authorRole != undefined ? authorRole : ""
                                onEditingFinished: authorRole = text
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            // Layout.fillWidth: true//lab.implicitWidth
                            Layout.fillWidth: true
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width
                            XsLabel {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "In"
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width
                            XsLabel {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: startTimecodeRole != undefined ? startTimecodeRole : ""
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            // Layout.fillWidth: true//lab.implicitWidth
                            Layout.fillWidth: true
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsButtonOld {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "Set"
                                palette.button: "transparent"
                                onClicked: startFrameRole = playhead.mediaFrame
                                tooltipTitle:"Notes"
                                tooltip:"Set In Point."
                                // horizontalAlignment: TextEdit.AlignHCenter
                                // verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            // Layout.fillWidth: true//lab.implicitWidth
                            Layout.fillWidth: true
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsLabel {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "Out"
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsLabel {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: endTimecodeRole != undefined ? endTimecodeRole : ""
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsButtonOld {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "Set"
                                palette.button: "transparent"
                                onClicked: endFrameRole = playhead.mediaFrame
                                tooltipTitle:"Notes"
                                tooltip:"Set Out Point."
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            // Layout.fillWidth: true//lab.implicitWidth
                            Layout.fillWidth: true
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsLabel {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "Dur"
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width
                            XsLabel {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: durationTimecodeRole != undefined ? durationTimecodeRole: ""
                                horizontalAlignment: TextEdit.AlignHCenter
                                verticalAlignment: TextEdit.AlignVCenter
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: children[0].implicitHeight+pad_height
                            Layout.minimumWidth: children[0].implicitWidth+pad_width

                            XsButtonOld {
                                anchors.fill: parent
                                anchors.margins: 1
                                text: "Loop"
                                palette.button: "transparent"
                                tooltipTitle:"Notes"
                                tooltip:"Set Loop Range."
                                onClicked: {
                                    playhead.loopStart = startFrameRole + playhead.sourceOffsetFrames
                                    playhead.loopEnd = endFrameRole + playhead.sourceOffsetFrames
                                    playhead.useLoopRange = true
                                }
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.columnSpan: 3
                            Layout.fillWidth: true//lab.implicitWidth
                            Layout.minimumHeight: author.height
                            Layout.minimumWidth: author.width
                            Layout.preferredHeight: author.height

                            XsComboBox {
                                id: type_combo
                                anchors.fill: parent
                                anchors.margins: 1
                                bgColorNormal: popupOptions.opened ? palette.base : colourRole ? Qt.tint(XsStyle.mainBackground, "#20" + colourRole.substring(3)) :"transparent"
                                // color: XsStyle.highlightColor
                                model: app_window.bookmark_categories
                                onModelChanged: {
                                    init = true
                                }
                                textRole: "textRole"
                                displayText: currentText==""? "Note Type":currentText
                                textColorNormal: popupOptions.opened?"light grey": currentText==""?"grey":"light grey"
                                font.family: XsStyle.controlTitleFontFamily
                                font.hintingPreference: Font.PreferNoHinting

                                property bool init: false

                                function updateFromModel() {
                                    let index = model.search(categoryRole, "valueRole")
                                    if(index.valid && currentIndex != index.row) {
                                        init = true
                                        currentIndex = index.row
                                    }
                                }
                                Component.onCompleted: updateFromModel()

                                Connections {
                                    target: delegate_rect
                                    function onTmpCategoryRoleChanged() {
                                        type_combo.updateFromModel()
                                    }
                                }

                                onCurrentIndexChanged: {
                                    if(init) {
                                        init = false
                                    } else if(currentIndex !== -1) {
                                        console.log(currentIndex)
                                        let ind = model.index(currentIndex, 0)
                                        let col = model.get(ind, "colourRole")
                                        let cat = model.get(ind, "valueRole")
                                        if(col == undefined)
                                            col = ""

                                        if(ind.valid && cat !== categoryRole) {

                                            categoryRole = cat
                                            colourRole = col

                                            preferences.note_category.value = cat
                                            preferences.note_colour.value = col
                                        }
                                    }
                                }
                            }
                        }
                        Rectangle {
                            color: XsStyle.mainBackground
                            Layout.columnSpan: 3
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    onVisibleChanged: {
        if(!visible) {
            // force update for edit widgets on close.
            root.forceActiveFocus()
        }
    }

    Rectangle {
        id: root
        anchors.top: parent.top//titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        color: "transparent"
        focus: true
        // border.color : "grey"
        // border.width : 2

        ColumnLayout{
            anchors.fill: parent
            anchors.margins: 4

            Rectangle {
                id: actionbar
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: XsStyle.controlBackground
                border.color : XsStyle.mainColor
                border.width : 1

                RowLayout {
                    anchors.fill: parent
                    property string filename: app_window.mediaImageSource.fileName ? app_window.mediaImageSource.fileName : "No Selection"

                    XsRoundButton {
                        id: note
                        text: "Add Note"
                        // Layout.fillHeight: true
                        // Layout.margins: 4
                        // Layout.preferredWidth: implicitWidth+8
                        Layout.preferredWidth: filterDepth.width
                        Layout.preferredHeight: implicitHeight+4
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        onClicked: {
                            app_window.sessionFunction.addNote()
                            root.forceActiveFocus()
                        }
                    }
                    XsLabel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 2
                        text: parent.filename
                        horizontalAlignment: TextEdit.AlignHCenter
                        verticalAlignment: TextEdit.AlignVCenter
                        elide: Text.ElideMiddle
                    }

                    XsLabel {
                        Layout.preferredWidth: 65
                        Layout.fillHeight: true
                        Layout.leftMargin: 4
                        Layout.rightMargin: 2
                        text: "Notes Scope:"
                        horizontalAlignment: TextEdit.AlignLeft
                        verticalAlignment: TextEdit.AlignVCenter
                    }
                    XsComboBox {
                        id: filterDepth
                        // Layout.preferredWidth: note.width
                        Layout.preferredWidth: 100
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        Layout.preferredHeight: note.height
                        model: ["Media", "Media List", "Playlist", "Session"]
                        currentIndex: preferences.note_depth.value
                        onCurrentIndexChanged: preferences.note_depth.value = currentIndex
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                XsLabel {
                    anchors.fill: parent
                    text: "Notes"
                    // visible: ! bookmarks_model.count
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 120
                    color: XsStyle.mainBackground
                    rotation: -45
                    z: 1
                }

                ScrollView {
                    z: 2
                    anchors.fill: parent

                    ListView {
                        property var lastFocus: 0
                        property var lastJump: 0
                        id: view
                        anchors.fill: parent
                        model: bookmarks_model
                        clip: true
                        spacing: 4
                        cacheBuffer: 400

                        Connections {
                            target: playerWidget.playerWidget ? playhead : null
                            function onMediaFrameChanged() {
                                positionTimer.start()
                            }
                        }

                        function jump() {
                            if(view.lastJump != view.lastFocus) {
                                view.lastJump = view.lastFocus
                                view.positionViewAtIndex(view.lastFocus, ListView.Visible)
                            }
                        }

                        Timer {
                            id: positionTimer
                            interval: 200
                            repeat: false
                            onTriggered: view.jump()
                        }
                    }
                }
            }

        }
    }
}
