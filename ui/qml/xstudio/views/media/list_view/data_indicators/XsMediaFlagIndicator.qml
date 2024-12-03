// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudio 1.0

Item {

    Rectangle{
        width: parent.width
        height: parent.height
        anchors.centerIn: parent
        color: flagColourRole == undefined ? "transparent" : flagColourRole
    }

    Rectangle{
        width: headerThumbWidth;
        height: parent.height
        anchors.right: parent.right
        color: headerThumbColor
    }

}