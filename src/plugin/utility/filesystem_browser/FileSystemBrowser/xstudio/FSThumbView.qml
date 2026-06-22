// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Qt.labs.qmlmodels
import xStudio 1.0

import ".."
import "."

// Thumbnail view: Flickable + Flow for reliable scrolling with folder headers
Flickable {

    id: thumbFlickable
    anchors.fill: parent
    visible: root.viewMode === 3
    clip: true
    flickableDirection: Flickable.VerticalFlick
    focus: visible
    onVisibleChanged: { if (visible) forceActiveFocus() }
    contentHeight: mainLayout.height

    ScrollBar.vertical: XsScrollBar {
        policy: ScrollBar.AlwaysOn
    }

    // these properties track the current visible area of the flicable, 
    // to allow thumb delegates to decide when to load/clear their thumbnail source 
    // for better thumbnail load performance
    property var windowBottom: 0
    property var windowTop: thumbFlickable.height + 160
    property var windowBottomMore: windowBottom-2000
    property var windowTopMore: windowTop+2000
    property var yMin: 0
    property var smallResultSet: scanResultsModel.numMediaFiles < 2000

    // When the user is scrolling we delay update of the windowBottom/windowTop 
    // properties until scrolling has stopped for 200ms, to avoid excessive 
    // thumbnail loading/unloading during scroll
    Timer {
        id: scrollChangeTimer
        interval: 200
        onTriggered: {
            yMin = thumbFlickable.contentY - thumbFlickable.originY
            windowBottom = yMin - 130
            windowTop = windowBottom + thumbFlickable.height + 130
        }
    }

    onOriginYChanged: scrollChangeTimer.restart()

    property var aRootIndex: scanResultsModel.rootIndex

    ColumnLayout {
        id: mainLayout
        width: parent.width
        spacing: 0
        Repeater {
            id: thumbRepeater
            model: delegateModel
        }
    }

    function getItemAtIndex(index) {
        let p = index.parent
        if (p.valid) {
            let row = p.row
            let group = thumbRepeater.itemAt(row)
            if (group) {
                return group.getThumbAtIndex(index.row)
            }
        }
        return undefined
    }

    DelegateModel {
                
        id: delegateModel
        model: scanResultsModel

        rootIndex: aRootIndex

        delegate: FSThumbGroup {
            Layout.fillWidth: true
            //Layout.preferredHeight: expectedHeight
            thumbsModel: scanResultsModel
            thumbsModelIndex: scanResultsModel.index(index, 0, delegateModel.rootIndex)  
        }
    }

    onContentYChanged: {
        scrollChangeTimer.restart()
    }

} // Flickable
