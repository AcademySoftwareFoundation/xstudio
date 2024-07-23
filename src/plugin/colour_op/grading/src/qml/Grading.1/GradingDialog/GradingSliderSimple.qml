// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0

Item {
    id: root

    width: 500
    height: 30

    property string attr_group

    XsModuleAttributesModel {
        id: model
        attributesGroupNames: root.attr_group
    }

    Column {
        anchors.fill: parent
        anchors.topMargin: 50
        anchors.leftMargin: 15
        spacing: 10

        Repeater {
            model: model
            delegate: GradingHSlider {}
        }
    }
}
