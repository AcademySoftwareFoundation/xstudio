// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0

XsListView{
    id: list
    anchors.fill: parent
    spacing: panelPadding

    property int rightSpacing: list.height < list.contentHeight ? 16 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}

    XsLabel {
        text: "Select the 'Scope' and the 'Note Type' to view the Note History."
        color: XsStyleSheet.hintColor
        visible: !activeScopeIndex.valid || !activeTypeIndex.valid

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    XsLabel {
        text: !queryRunning ? "No Results Found" : ""
        // text: isPaused ? "Updates Paused" : !queryRunning ? "No Results Found" : ""
        color: XsStyleSheet.hintColor
        visible: dataModel && activeScopeIndex.valid && activeTypeIndex.valid && !dataModel.count //#TODO

        anchors.fill: parent

        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    model: DelegateModel {
        id: chooserModel
        model: dataModel
        delegate: NotesHistoryListDelegate{
            width: list.width - rightSpacing
            height: XsStyleSheet.widgetStdHeight * 8

            delegateModel: chooserModel
            popupMenu: resultPopup
        }
    }
}
