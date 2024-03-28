import QtQuick 2.15

import xStudioReskin 1.0

Item {

    id: divider
    property bool isVertical: true
    property real thresholdSize: 10
    property real dividerSize: isHovered || dragging? 1.5*2.5 : 1.5
    property real minLimit: 0
    property real maxLimit: isVertical? parent.width : parent.height
    property int id: -1
    
    property bool dragging: mArea.drag.active
    property bool isHovered: mArea.containsMouse


    width: isVertical? dividerSize*2 : parent.width
    height: isVertical? parent.height : dividerSize*2

    Rectangle{ id: visualThumb

        width: isVertical? dividerSize : parent.width
        height: isVertical? parent.height : dividerSize
        color: isHovered || dragging? mArea.pressed? palette.highlight : XsStyleSheet.secondaryTextColor : "#AA000000"

        Component.onCompleted: {
            if(isVertical) anchors.left = parent.left
            else anchors.top = parent.top
        }

    }

    property var fractional_position: child_dividers[index]

    property var computed_position: (isVertical ? parent.width : parent.height)*fractional_position

    onComputed_positionChanged: {
        if (!dragging) {
            if (isVertical) x = computed_position
            else y = computed_position
        }
    }

    onYChanged: {
        if (dragging && !isVertical) {
            var v = child_dividers;
            v[index] = y/parent.height
            child_dividers= v;
        }
    }

    onXChanged: {
        if (dragging && isVertical) {
            var v = child_dividers;
            v[index] = x/parent.width
            child_dividers= v;
        }
    }

    MouseArea{ id: mArea

        width: divider.width
        height: divider.height
        anchors.centerIn: divider
        preventStealing: true

        hoverEnabled: true
        cursorShape: isVertical? Qt.SizeHorCursor : Qt.SizeVerCursor

        drag.target: divider
        drag.axis: isVertical? Drag.XAxis : Drag.YAxis

    }

}