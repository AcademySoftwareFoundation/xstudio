// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle

import xStudio 1.1

TabBar { id: widget

    background: Rectangle {
        color: "transparent" //palette.base
    }

    // contentItem: ListView {
    //     model: widget.contentModel
    //     currentIndex: widget.currentIndex
    //     spacing: widget.spacing
    //     orientation: ListView.Vertical
    //     boundsBehavior: Flickable.StopAtBounds
    //     flickableDirection: Flickable.AutoFlickIfNeeded
    //     snapMode: ListView.SnapToItem
    //     highlightMoveDuration: 0
    //     highlightRangeMode: ListView.ApplyRange
    //     preferredHighlightBegin: 40
    //     preferredHighlightEnd: height - 40
    // }

}