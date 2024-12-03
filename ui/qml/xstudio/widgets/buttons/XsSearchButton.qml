// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0

Item {
    id: widget

    property bool isExpanded: false
    property bool isExpandedToLeft: false
    property real expandedWidth: XsStyleSheet.primaryButtonStdWidth * 5

    property alias searchBtn: searchBtn
    property alias imgSrc: searchBtn.imgSrc
    property string hint: ""
    property alias text: searchBar.text
    property alias searchBar: searchBar

    property int buttonWidth: XsStyleSheet.primaryButtonStdWidth

    implicitWidth: isExpanded? expandedWidth : buttonWidth

    height: XsStyleSheet.widgetStdHeight + 4

    signal editingCompleted()


    XsPrimaryButton{ id: searchBtn
        x: isExpandedToLeft? searchBar.width : 0
        width: buttonWidth
        height: parent.height
        imgSrc: "qrc:/icons/search.svg"
        text: "Search"
        isActive: isExpanded

        onClicked: {
            isExpanded = !isExpanded

            if(isExpanded) searchBar.forceActiveFocus()
            else {
                searchBar.clearSearch()
                searchBar.focus = false
            }
        }
    }

    XsSearchBar{ id: searchBar

        Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutQuart }  }
        width: isExpanded? parent.width - searchBtn.width : 0

        height: parent.height
        // anchors.left: searchBtn.right

        placeholderText: isExpanded? hint : "" //activeFocus? "" : hint
        onEditingCompleted: widget.editingCompleted()

        Component.onCompleted: {
            if(isExpandedToLeft) anchors.right = searchBtn.left
            else anchors.left = searchBtn.right
        }
    }

}

