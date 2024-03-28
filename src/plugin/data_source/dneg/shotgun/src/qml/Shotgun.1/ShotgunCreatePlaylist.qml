// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xStudio 1.1

import xstudio.qml.module 1.0

XsDialogModal {
    id: dialog
    title: "Create ShotGrid Playlist"

    property var playlist_uuid: null
    property int validMediaCount: 0
    property int invalidMediaCount: 0
    property bool linking: false

    property var siteModel: null
    property var siteCurrentIndex: -1
    property var projectModel: null
    property var projectCurrentIndex: -1
    property var playlistTypeModel: null


    property alias playlist_name: playlist_name_.text
    property alias site_name: siteCB.currentText
    property alias playlist_type: ptype.currentText
    property int project_id: project.currentIndex !==-1 ? data_source.termModels.projectModel.get(project.currentIndex, "idRole") : -1

    onVisibleChanged: {
        if(visible) {
            playlist_name_.selectAll()
            playlist_name_.forceActiveFocus()
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        GridLayout {
            id: main_layout
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            XsLabel {
                id: project_label
                text: "Project : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }
            XsComboBox {
                id: project
                Layout.fillWidth: true
                Layout.preferredHeight: project_label.height*2
                Layout.rightMargin: 6

                model: projectModel
                currentIndex: projectCurrentIndex
                textRole: "nameRole"
                valueRole: "nameRole"

                font.pixelSize: XsStyle.sessionBarFontSize
                font.family: XsStyle.controlTitleFontFamily
                font.hintingPreference: Font.PreferNoHinting
                selectTextByMouse: true
                editable: true
            }

            XsLabel {
                text: "Site : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }
            XsComboBox {
                id: siteCB
                Layout.fillWidth: true
                Layout.preferredHeight: project_label.height*2
                Layout.rightMargin: 6

                font.pixelSize: XsStyle.sessionBarFontSize
                font.family: XsStyle.controlTitleFontFamily
                font.hintingPreference: Font.PreferNoHinting

                model: siteModel
                currentIndex: siteCurrentIndex
                textRole: "nameRole"
                valueRole: "nameRole"
            }

            XsLabel {
                id: ptype_label
                text: "Type : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }
            XsComboBox {
                id: ptype
                Layout.fillWidth: true
                Layout.preferredHeight: ptype_label.height*2
                Layout.rightMargin: 6

                model: playlistTypeModel

                onModelChanged: {
                    if(model)
                        currentIndex = model.search("Dailies", "nameRole")
                }

                currentIndex: -1
                textRole: "nameRole"
                valueRole: "nameRole"

                font.pixelSize: XsStyle.sessionBarFontSize
                font.family: XsStyle.controlTitleFontFamily
                font.hintingPreference: Font.PreferNoHinting
                selectTextByMouse: true
                editable: true
            }


            XsLabel {
                text: "Name : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            TextField {
                id: playlist_name_
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft

                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                font.pixelSize: XsStyle.popupControlFontSize
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                onAccepted: dialog.accept()
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }
            }
            XsLabel {
                text: "Valid ShotGrid Media : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }
            XsLabel {
                text: linking ? "Linking media to versions..." : validMediaCount + " / " + (validMediaCount+invalidMediaCount)
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
            }
            XsLabel {
                Layout.fillHeight: true
            }
        }


        RowLayout {
            id: myFooter
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 35

            focus: true
            Keys.onReturnPressed: accept()
            Keys.onEscapePressed: reject()

            XsRoundButton {
                text: qsTr("Cancel")
                Layout.leftMargin: 10
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: dialog.width / 5

                onClicked: {
                     reject()
                }
            }
            XsHSpacer{}
            XsRoundButton {
                text: "Create"
                highlighted: !linking
                enabled: !linking

                Layout.rightMargin: 10
                Layout.minimumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10
                onClicked: {
                    accept()
                }
            }
        }
    }
}

