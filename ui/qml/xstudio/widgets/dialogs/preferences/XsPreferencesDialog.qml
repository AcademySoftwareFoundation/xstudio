// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.qmlmodels 1.0

import xstudio.qml.models 1.0
import xstudio.qml.global_store_model 1.0
import xStudio 1.0

import "./delegates"

XsWindow {
    id: dialog

	width: 550
	minimumWidth: 550
	height: 250
	// minimumHeight: 250

    title: "xSTUDIO Preferences"

    XsPreferencesModel {
        id: preferencesModel
    }
    property alias preferencesModel: preferencesModel

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 0
        spacing: 10

        TabBar {
            id: tabBar

            Layout.fillWidth: true
            background: Rectangle {
                color: palette.base
            }

            Repeater {
                model: preferencesModel

                TabButton {

                    width: implicitWidth

                    contentItem: XsText {
                        text: categoryRole
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: tabBar.currentIndex == index
                    }

                    background: Rectangle {
                        border.color: hovered? palette.highlight : "transparent"
                        color: tabBar.currentIndex == index ? XsStyleSheet.panelTitleBarColor : Qt.darker(XsStyleSheet.panelTitleBarColor, 1.5)
                    }
                }

            }

        }

        StackLayout {

            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            Repeater {
                model: preferencesModel

                XsPreferenceCategory {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    theIndex: preferencesModel.index(index, 0)
                    category: categoryRole
                }
            }
        }

        XsSimpleButton {

            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
            Layout.rightMargin: 10
            Layout.bottomMargin: 10
            text: qsTr("Close")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.close()
            }
        }
    }

}