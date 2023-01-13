// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.1
import QtQuick.Layouts 1.12

import xStudio 1.0

RowLayout {

    //text: "Transport Controls"

    /*Rectangle {
        width: multiplierLabelWidth
        color: "transparent"
        height:XsStyle.controlHeight
        Text {
            id: rewindMultiplier
            anchors.fill: parent
            anchors.rightMargin: 5
            visible: !playhead ? false : playhead.playing&&playhead.velocityMultiplier>1&&!playhead.forward
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            text: !playhead ? "" : "x" + playhead.velocityMultiplier
            color:XsStyle.highlightColor
            font.family: XsStyle.fontFamily
            font.pixelSize: 13

        }
    }*/

    property int pad: 0

    XsRewindWidget {
        Layout.fillHeight: true
        //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
        buttonPadding: pad
    }

    XsStepBackWidget {
        Layout.fillHeight: true
        //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
        buttonPadding: pad
    }

    XsTogglePlayWidget {
        Layout.fillHeight: true
        //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
        buttonPadding: pad
    }

    XsStepForwardWidget {
        Layout.fillHeight: true
        //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
        buttonPadding: pad
    }

    XsForwardWidget {
        Layout.fillHeight: true
        //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
        buttonPadding: pad
    }

    /*Rectangle {
        width: multiplierLabelWidth
        height:XsStyle.controlHeight
        color: "transparent"
        Text {
            id: forwardMultiplier
            anchors.fill: parent
            anchors.leftMargin: 5
            width: multiplierLabelWidth
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            visible: !playhead ?false:playhead.playing&&playhead.velocityMultiplier>1&&playhead.forward
            text: !playhead ?"":"x" + playhead.velocityMultiplier
            color:XsStyle.highlightColor
            font.family: XsStyle.fontFamily
            font.pixelSize: 13
        }
    }*/
}
