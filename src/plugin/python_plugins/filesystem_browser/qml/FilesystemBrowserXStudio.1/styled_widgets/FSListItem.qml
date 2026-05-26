// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

import "."
import ".."

Rectangle {

    id: delegate
    height: XsStyleSheet.widgetStdHeight

    property bool isSelected: selectedItems.includes(index)
    property bool isHovered: underMouseIndex === index
    property string itemPath: modelData.path
    property bool isItemFolder: modelData.isFolder

    Rectangle {
        anchors.fill: parent
        color: (isHovered ? XsStyleSheet.panelBgGradTopColor : (index % 2 == 0 ? XsStyleSheet.panelBgColor : Qt.lighter(XsStyleSheet.panelBgColor, 1.1)))
    }

    Rectangle {
        anchors.fill: parent
        color: isSelected ? XsStyleSheet.accentColor : "transparent"
        opacity: 0.5
    }
        
    property var indent: (modelData.depth || 0) * 20 + 10

    // Cells
    component Cell: XsText {
        property int index: 0
        x: columnPositions[index] + (index === 0 ? indent + 24 : 0)
        width: columnWidths[index]
        property int elideMode: Text.ElideRight
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: index ? Text.AlignHCenter : Text.AlignLeft
        elide: elideMode
        color: isSelected ? XsStyleSheet.primaryTextColor : XsStyleSheet.secondaryTextColor
    }
                            
    // Expander
    XsSecondaryButton {
        x: indent
        width: visible ? 20 : 0
        height: visible ? 20 : 0
        imgSrc: "qrc:/icons/folder.svg"
        visible: (root.viewMode !== 0 && root.viewMode !== 3 && modelData.isFolder)
        //rotation: modelData.expanded ? 90 : 0
        imageSrcSize: width
        onClicked: toggleExpand(index)
    }

    Cell { index: 0; text: modelData.name || ""; elideMode: Text.ElideMiddle }
    Cell { index: 1; text: (modelData.data && modelData.data.version) ? "v"+modelData.data.version : "" }
    Cell { index: 2; text: (modelData.data && modelData.data.frames) || "" }
    Cell { index: 3; text: (modelData.data && modelData.data.owner) || "" }
    Cell { index: 4; text: modelData.data ? formatDate(modelData.data.date) : "" }
    Cell { index: 5; text: (modelData.data && modelData.data.size_str) || "" }
    
}