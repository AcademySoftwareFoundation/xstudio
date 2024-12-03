// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

Rectangle {
    id: control
    property var termModel: null
    color: XsStyleSheet.widgetBgNormalColor

    RowLayout {
        anchors.fill: parent
        spacing: 1

        // Item{
        //     Layout.maximumWidth: dragWidth
        //     Layout.minimumWidth: dragWidth
        //     Layout.fillHeight: true
        // }
        Item {
            Layout.maximumWidth: control.height+1
            Layout.minimumWidth: control.height+1
            Layout.fillHeight: true
        }

        XsComboBox {
            Layout.maximumWidth: termWidth
            Layout.minimumWidth: termWidth
            Layout.preferredHeight: control.height

            model: termModel
            displayText: currentIndex == -1? "Select Term..." : currentText

            currentIndex: -1
            onModelChanged: {
                // console.log("new item", "onModelChanged", model)
                currentIndex = -1
            }

            // don't use onCurrentIndex changed! As that'll get programatic changes as well.
            onActivated: {
                if(index != -1) {
                    let row = ShotBrowserEngine.presetsModel.rowCount(presetIndex)
                    let i = ShotBrowserEngine.presetsModel.insertTerm(
                        textAt(index),
                        row,
                        presetIndex
                    )

                    if(i.valid) {
                        let t = ShotBrowserEngine.presetsModel.get(i, "termRole")
                        let tm = ShotBrowserEngine.presetsModel.termModel(t, entityType, projectId)
                        if(tm.length && tm.get(tm.index(0,0), "nameRole") == "True") {
                            ShotBrowserEngine.presetsModel.set(i, "True", "valueRole")
                        }
                    }
                    currentIndex = -1
                }
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}

