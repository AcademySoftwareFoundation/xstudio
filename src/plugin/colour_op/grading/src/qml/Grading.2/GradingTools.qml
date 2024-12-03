// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import Qt.labs.qmlmodels 1.0 //for RadialGradient

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

Item { id: dialog
    anchors.fill: parent

    GTAttributes { id: attrs }

    MAttributes { id: mask_attrs }

    property real itemSpacing: 1
    property real buttonSpacing: 1
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight + 4
    property real panelPadding: XsStyleSheet.panelPadding
    property color panelColor: XsStyleSheet.panelBgColor

    property alias grading_sliders_model: attrs.grading_sliders_model
    property alias grading_wheels_model: attrs.grading_wheels_model

    Component.onCompleted: {
        // If created in hidden state, just rely on onVisibleChanged
        // to increment the property when the component becomes visible.
        if (visible) {
            attrs.tool_opened_count += 1
        }
    }
    Component.onDestruction: {
        attrs.tool_opened_count -= 1
    }

    onVisibleChanged: {
        if (attrs.tool_opened_count === undefined) {
            return
        }

        if (visible) {
            attrs.tool_opened_count += 1
        } else {
            attrs.tool_opened_count -= 1
        }
    }

    function hasActiveGrade() {
        return attrs.grading_bookmark && attrs.grading_bookmark != "00000000-0000-0000-0000-000000000000"
    }

    XsBookmarkFilterModel {
        id: bookmarkFilterModel
        sourceModel: bookmarkModel
        currentMedia: currentPlayhead.mediaUuid
        showHidden: true
        showUserType: "Grading"
        sortbyCreated: true
    }


    property alias bookmarkList: listDiv.bookmarkList
    
    ColumnLayout { id: leftView
        anchors.fill: parent
        anchors.margins: panelPadding
        spacing: panelPadding

        Sec1Header{
            Layout.fillWidth: true
            Layout.preferredHeight: btnHeight
        }

        GridLayout { id: itemsGrid
            property bool isVertical: false
    
            Layout.fillWidth: true
            Layout.fillHeight: true
    
            flow: GridLayout.TopToBottom
            rowSpacing: 1
            columnSpacing: 1
            rows: itemsGrid.isVertical? 3: 1
            columns: itemsGrid.isVertical? 6:18
    
            Behavior on width {NumberAnimation{ duration: 250 }}
            onWidthChanged: {
                if(width < 850) {
                    itemsGrid.isVertical = true
                } else {
                    itemsGrid.isVertical = false
                }
            }
        
            Sec2LayerList{ id: listDiv
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.columnSpan: 3
            }

            Sec3MaskTools{
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: itemsGrid.isVertical? itemsGrid.width/2 : 100
                Layout.maximumWidth: itemsGrid.isVertical? itemsGrid.width/2 : 100
                Layout.columnSpan: 3
            }
            
            GTSliderItem {
                Layout.fillWidth: true
                Layout.preferredWidth: itemsGrid.isVertical? itemsGrid.width/2 : itemsGrid.width/6
                Layout.maximumWidth: itemsGrid.isVertical? itemsGrid.width/2 : itemsGrid.width/6
                Layout.fillHeight: true
                Layout.columnSpan: 3
            }

            Repeater{
                model: attrs.grading_wheels_model

                GTWheelItem {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: itemsGrid.isVertical? itemsGrid.width/2 : itemsGrid.width/6
                    Layout.maximumWidth: itemsGrid.isVertical? itemsGrid.width/2 : itemsGrid.width/6
                    Layout.columnSpan: 3
                }
            }

        }


    }



    






}
