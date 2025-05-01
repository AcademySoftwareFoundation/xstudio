import QtQuick

import xStudio 1.0

Item {

    id: divider
    property bool isVertical: true
    property real thresholdSize: 10
    property real thickness: (isHovered || dragging) ? XsStyleSheet.dividerSize+2 : XsStyleSheet.dividerSize //isHovered || dragging? 20 : XsStyleSheet.dividerSize

    property bool dragging: mArea.drag.active
    property bool isHovered: mArea.containsMouse


    width: isVertical? thickness : parent.width
    height: isVertical? parent.height : thickness

    Rectangle{ id: visualThumb

        anchors.fill: parent
        color: dragging? palette.highlight : isHovered ? XsStyleSheet.secondaryTextColor : "#FF000000"

    }
    property var fractional_position_minus: index > 0 ? child_dividers[index-1] : 0.0
    property var fractional_position_plus: index < (child_dividers.length-1) ? child_dividers[index+1] : 1.0
    property real minLimit: (isVertical ? parent.width : parent.height)*fractional_position_minus + 20
    property real maxLimit: (isVertical ? parent.width : parent.height)*fractional_position_plus - 20

    property var fractional_position: child_dividers[index]

    property var computed_position: (isVertical ? parent.width : parent.height)*fractional_position - thickness/2

    onComputed_positionChanged: {
        if (!dragging) {
            if (isVertical) x = computed_position
            else y = computed_position
        }
    }

    onYChanged: {
        if (dragging && !isVertical) {
            var v = child_dividers;
            v[index] = (y+thickness/2)/parent.height
            child_dividers= v;
        }
    }

    onXChanged: {
        if (dragging && isVertical) {
            var v = child_dividers;
            v[index] = (x+thickness/2)/parent.width
            child_dividers= v;
        }
    }

    MouseArea{

        // Note mouse area is 2 pixels thicker either side of the divider to
        // make it easier to grab
        id: mArea
        width: divider.width + (isVertical ? 4 : 0)
        height: divider.height + (isVertical ? 0 : 4)
        anchors.centerIn: divider
        preventStealing: true

        hoverEnabled: true
        cursorShape: isVertical? Qt.SizeHorCursor : Qt.SizeVerCursor

        drag.target: divider
        drag.axis: isVertical? Drag.XAxis : Drag.YAxis
        drag.minimumX: minLimit
        drag.minimumY: minLimit
        drag.maximumX: maxLimit
        drag.maximumY: maxLimit

    }

}