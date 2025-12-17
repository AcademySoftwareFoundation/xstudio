// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

XsWindow{
    id: dialog
    title: "DnTag Shot Filter"

    readonly property real itemHeight: btnHeight
    readonly property real itemSpacing: 5

    property var model: []

    property var tags: []
    property var notTags: []
    property bool andMode: true

    property bool ignoreUpdate: false

    minimumWidth: 460
    minimumHeight: 500

    palette.base: XsStyleSheet.panelTitleBarColor

    onModelChanged: {
        let tmp = []
        model.forEach(function(item,index){
            if(item)
                tmp.push({"name": item})
        })
        searchData.modelData = tmp
    }

    XsJsonTreeModel {
        id: searchData
        modelData: []
    }

    XsFilterModel {
        id: searchModel
        sourceModel: searchData
    }

    onAndModeChanged: {
        modeCB.currentIndex = (andMode ? 1 : 0);
    }

    onTagsChanged: {
        if(!ignoreUpdate)
            tagWidget.text = tags.join("\n");
        ignoreUpdate = false
    }

    onNotTagsChanged: {
        if(!ignoreUpdate)
            notWidget.text = notTags.join("\n");
        ignoreUpdate = false
    }

    function parsePosition(widget) {
        // default to selection as key to tab completion
        let linestart = widget.cursorPosition;
        let start = widget.cursorPosition;
        let end = widget.cursorPosition;
        let lineend = widget.cursorPosition;

        if(widget.selectionStart != widget.selectionEnd) {
            start = widget.selectionStart
            end = widget.selectionEnd
        } else {
            // capture current line.
            while(start) {
                start -= 1;
                if(widget.text[start] == "\n"){
                    start += 1;
                    break;
                }
            }
            while(end != widget.text.length) {
                end += 1;
                if(widget.text[end] == "\n"){
                    end -= 1;
                    break;
                }
            }
        }

        while(linestart) {
            linestart -= 1;
            if(widget.text[linestart] == "\n"){
                linestart += 1;
                break;
            }
        }

        while(lineend != widget.text.length) {
            lineend += 1;
            if(widget.text[lineend] == "\n"){
                lineend -= 1;
                break;
            }
        }

        return [linestart, start, end, lineend]
    }

    function tabComplete(widget) {
        // get line up to '\n'
        let [linestart, start, end, lineend] = parsePosition(widget)
        widget.select(start, end)


        let key = widget.text.slice(start, end)
        let fullkey = widget.text.slice(linestart, lineend)

        let matches = []
        dialog.model.forEach(function (item, index) {
            if(item.includes(key))
                matches.push(item)
        })

        if(matches.length) {
            let match = matches[0];

            for(let i=0; i<matches.length; i++) {
                if(matches[i] == fullkey && i != matches.length-1) {
                    match = matches[i+1]
                    break;
                }
            }

            widget.remove(linestart, lineend)
            widget.insert(linestart, match)
            // find offset into match..
            let sstart = widget.text.indexOf(key, linestart)
            widget.select(sstart, sstart+key.length)
            widget.moveCursorSelection(sstart+key.length)
        }
    }

    function parseTags(tags) {
        ignoreUpdate = true
        let items = []
        tags.split("\n").forEach(function (item, index) {
            let tmp = item.trim()
            if(tmp)
                items.push(tmp)
        });
        items.sort()
        return items
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing
        anchors.margins: 4

        RowLayout{
            Layout.preferredHeight: itemHeight
            Layout.maximumHeight: itemHeight

            XsLabel {
                Layout.preferredHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.alignment: Qt.AlignLeft

                text: "Include Tags:"
            }


            XsSBTreeSearchButton {
                Layout.fillWidth: true
                Layout.fillHeight: true

                hint: "Search For Tags..."
                isExpanded: true
                model: searchModel
                onIndexSelected: (index ) => {
                    tagWidget.append(searchModel.get(searchModel.index(index,0)))
                    tags = parseTags(tagWidget.text)
                }
            }

            XsComboBox {
                id: modeCB
                Layout.maximumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true

                model: ["ANY", "ALL"]
                onActivated: {andMode = (currentIndex == 1)}
            }
        }


        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: dialog.height / 3

            color: panelColor

            XsTextEdit {
                id: tagWidget
                anchors.fill: parent
                anchors.margins: 4
                hint: "Enter Tags, Repeatedly TAB to autocomplete with selection..."

                onEditingFinished: tags = parseTags(text)
                Keys.onTabPressed: {
                    tabComplete(this)
                    tags = parseTags(text)
                }

                Keys.onReturnPressed: (event)=> {
                    // clear selection..
                    if(selectedText) {
                        let [linestart, start, end, lineend] = parsePosition(this)
                        moveCursorSelection(lineend)
                        deselect()
                    }
                    tags = parseTags(text)
                    event.accepted = false;
                }
            }
        }
        RowLayout{
            Layout.preferredHeight: itemHeight
            Layout.maximumHeight: itemHeight

            XsLabel {
                Layout.preferredHeight: itemHeight
                Layout.maximumHeight: itemHeight
                Layout.alignment: Qt.AlignLeft
                text: "Exclude Tags"
            }

            XsSBTreeSearchButton {
                Layout.fillWidth: true
                Layout.fillHeight: true

                hint: "Search For Tags..."
                isExpanded: true
                model: searchModel
                onIndexSelected: (index ) => {
                    notWidget.append(searchModel.get(searchModel.index(index,0)))
                    notTags = parseTags(notWidget.text)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: dialog.height / 3

            color: panelColor

            XsTextEdit {
                id: notWidget
                anchors.fill: parent
                anchors.margins: 4
                hint: "Enter Tags, Repeatedly TAB to autocomplete with selection..."

                onEditingFinished: notTags = parseTags(text)
                Keys.onTabPressed: {
                    tabComplete(this)
                    notTags = parseTags(text)
                }

                Keys.onReturnPressed: (event)=> {
                    // clear selection..
                    if(selectedText) {
                        let [linestart, start, end, lineend] = parsePosition(this)
                        moveCursorSelection(lineend)
                        deselect()
                    }
                    event.accepted = false;
                    notTags = parseTags(text)
                }
            }
        }

        RowLayout{
            Layout.preferredHeight: itemHeight
            Layout.maximumHeight: itemHeight

            XsPrimaryButton {
                Layout.maximumWidth: dialog.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "Clear"
                onClicked: {
                    tags = [];
                    notTags = [];
                    andMode = false;
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            XsPrimaryButton {
                Layout.maximumWidth: dialog.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "Close"
                onClicked: dialog.visible = false
            }
        }
    }
}