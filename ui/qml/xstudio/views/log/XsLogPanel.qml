// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudio 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.embedded_python 1.0

Item{

    id: panel
    anchors.fill: parent

    XsGradientRectangle{
        anchors.fill: parent
    }

    // XsPreference {
    //     id: pythonHistory
    //     path: "/ui/qml/python_history"
    // }

    property bool autoScroll: true

    property int itemHeight: XsStyleSheet.widgetStdHeight //22
    property int btnHeight: XsStyleSheet.widgetStdHeight+4

    property int itemSpacing: 1
    property int padding: XsStyleSheet.panelPadding

    property int buttonCount: 3
    property int totalLogCount: logsView.count
    onTotalLogCountChanged: {
        statusText = "Status: Count updated."
        if(autoScroll) logsView.currentIndex = totalLogCount - 1
    }

    property int startSelection: -1
    property int endSelection: -1
    property int selectedCount: startSelection == -1 ? 0 : endSelection - startSelection + 1

    // property int showLevel: 5

    property string statusText

    function resetSelection(){
        startSelection= -1
        endSelection= -1
    }

    Rectangle{

        width: parent.width - padding*2
        height: parent.height - padding*2
        x: padding
        y: padding
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: btnHeight + 2
                spacing: 5

                XsSearchButton{
                    id: searchBtn
                    Layout.fillWidth: isExpanded
                    Layout.minimumHeight: btnHeight
                    Layout.maximumHeight: btnHeight
                    enabled: logger.logLevel != 6
                    isExpanded: true
                    searchBtn.enabled: false
                    searchBtn.isUnClickable: true
                    hint: "Enter filter text"
                    onTextChanged: {
                        logger.setFilterWildcard(text)
                    }
                }


                XsTextWithCheckBox {
                    Layout.minimumHeight: btnHeight
                    Layout.maximumHeight: btnHeight
                    Layout.preferredWidth: 100

                    paddingLR: 0
                    text: "Auto-scroll"
                    checked: autoScroll
                    onCheckedChanged: {
                        autoScroll = checked
                    }
                }

                XsComboBox { id: logLevel
                    Layout.minimumHeight: btnHeight
                    Layout.maximumHeight: btnHeight
                    Layout.preferredWidth: 100
                    model: logger.logLevels
                    currentIndex: logger.logLevel
                    onCurrentIndexChanged: {
                        logger.logLevel = currentIndex
                        resetSelection()
                    }
                }

            }

            ListView {
                id: logsView
                Layout.fillWidth: true
                Layout.fillHeight: true

                // contentWidth: width*2
                flickableDirection: Flickable.AutoFlickDirection
                boundsBehavior: Flickable.StopAtBounds
                highlightRangeMode: ListView.ApplyRange
                clip: true

                ScrollBar.vertical: XsScrollBar {
                    // policy: ScrollBar.AlwaysOn
                    visible: logsView.height < logsView.contentHeight
                }
                ScrollBar.horizontal: ScrollBar {}
                model: logger
                enabled: logger.logLevel != 6

                Rectangle{
                    anchors.fill: parent
                    color: XsStyleSheet.panelBgColor
                    z: -1
                    opacity: enabled? 1:0.8
                }

                delegate:
                Rectangle{
                    width: logsView.width
                    height: msg.contentHeight+4
                    color:  Qt.darker("#222", index % 2 ? 1.0 :1.25)

                    required property string timeRole
                    required property int levelRole
                    required property string levelStringRole
                    required property string stringRole
                    required property int index
                    property color level_color: logsView.level_colour(levelRole)

                    Rectangle{ id: selectionDiv
                        width: parent.width-2
                        height: parent.height-1
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.leftMargin: 1
                        color: XsStyleSheet.accentColor //baseColor
                        opacity: 0.25
                        visible: index >= startSelection && index <= endSelection
                    }

                    MouseArea{
                        anchors.fill: parent
                        z: 1
                        onClicked: {
                            if (mouse.button == Qt.LeftButton) {
                                if (mouse.modifiers & Qt.ShiftModifier) {
                                    if(index < startSelection) {
                                        endSelection = startSelection
                                        startSelection = index
                                    } else {
                                        endSelection = index
                                    }
                                } else {
                                    startSelection = endSelection = index
                                }
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 2
                        anchors.rightMargin: 10

                        XsTextEdit {
                            id: msg
                            wrapMode: TextEdit.Wrap
                            readOnly: true
                            verticalAlignment: Text.AlignVCenter
                            color: level_color
                            text: stringRole
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Text {
                            verticalAlignment: Text.AlignTop
                            color: level_color
                            text: "["+levelStringRole+"]"
                            Layout.fillHeight: true
                        }
                        Text {
                            verticalAlignment: Text.AlignTop
                            color: level_color
                            text: Qt.formatDateTime(timeRole, "hh:mm:ss")
                            Layout.fillHeight: true
                        }
                    }
                }


                function level_colour(level)
                {
                    if(level == 0) //trace
                    return "cyan"
                    if(level == 1) //debug
                    return "grey"
                    if(level == 2) // info
                    return "green"
                    if(level == 3) // warn
                    return "yellow"
                    if(level == 4) // error
                    return "red"
                    if(level == 5) // critical
                    return "purple"
                    if(level == 6) // off
                    return "black"
                }

            }

            Rectangle{id: countTexts
                Layout.fillWidth: true
                Layout.minimumHeight: itemHeight/1.5
                Layout.maximumHeight: itemHeight/1.5
                color: "transparent"
                enabled: logger.logLevel != 6
                opacity: enabled? 1:0.5

                Text { id: textStatus
                    anchors.left: parent.left
                    anchors.leftMargin: itemSpacing/2
                    color: "light grey"
                    elide: Text.ElideRight
                    width: parent.width-70
                    anchors.verticalCenter: parent.verticalCenter

                    text: statusText
                    onTextChanged: if(statusText!="") opacity=1
                    opacity: 0
                    onOpacityChanged: if(opacity==0) statusText=""

                    Behavior on opacity { NumberAnimation { easing.type: Easing.InQuart; duration: 3000; from: 1; to: 0; onRunningChanged: if(!running) textStatus.opacity = 0 } }
                    Component.onCompleted: textStatus.opacity = 0
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: itemSpacing/2
                    anchors.verticalCenter: parent.verticalCenter
                    color: "light grey"
                    text: "Shown: "+totalLogCount + " | Selected: " + selectedCount
                }
            }

            Rectangle{id: buttons
                Layout.fillWidth: true
                Layout.minimumHeight: itemHeight
                Layout.maximumHeight: itemHeight
                color: "transparent"
                enabled: logger.logLevel != 6

                Row{
                    width: parent.width
                    height: itemHeight
                    x: spacing/2
                    spacing: itemSpacing

                    XsPrimaryButton {
                        text: "Select All"
                        onClicked: {startSelection = 0; endSelection = logger.rowCount()-1}
                        width: parent.width/buttonCount - itemSpacing
                        height: itemHeight
                    }
                    XsPrimaryButton {
                        text: "Copy to Clipboard"
                        enabled: selectedCount
                        onClicked: {
                            logger.copyLogToClipboard(startSelection, selectedCount)
                            statusText = "Status: Copied to Clipboard."
                        }

                        width: parent.width/buttonCount - itemSpacing
                        height: itemHeight
                    }
                    XsPrimaryButton {
                        text: "Save to File"
                        onClicked: {
                            dialogHelpers.showFileDialog(
                                function(fileUrl, undefined, func) {
                                    if(fileUrl !== false) {
                                        var path = fileUrl.toString()
                                        var ext = path.split('.').pop()
                                        if(path == ext) {
                                            path = path + ".log"
                                        }
                                        logger.saveLogToFile(Qt.resolvedUrl(path), startSelection, selectedCount)
                                        statusText = "Status: Saved to "+path
                                    }
                                },
                                file_functions.defaultSessionFolder(),
                                "Save Log",
                                "log",
                                ["Log Files (*.log)"],
                                false,
                                false
                            )
                        }
                        enabled: selectedCount
                        width: parent.width/buttonCount - itemSpacing
                        height: itemHeight
                    }
                }
            }
        }
    }
}