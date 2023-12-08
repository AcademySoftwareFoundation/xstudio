import QtQuick 2.15

Rectangle {

    property bool isVertical: true
    property real thresholdSize: 10
    property real dividerSize: 5
    property real minLimit: 0
    property real maxLimit: isVertical? parent.width : parent.height
    property int id: -1
    property bool isDragging: mArea.drag.active

    color: "white"//mArea.containsMouse || mArea.drag.active? mArea.pressed? "yellow" : "#AA000000": "#AA000000"

    width: isVertical? dividerSize : parent.width
    height: isVertical? parent.height : dividerSize

    property var computed_position: (isVertical ? parent.width : parent.height)*fractional_position

    onComputed_positionChanged: {
        if (!isDragging) {
            if (isVertical) x = computed_position
            else y = computed_position
        }
    }

    onYChanged: {
        if (isDragging && !isVertical) {
            fractional_position = y/parent.height
            dividerMoved()
        }
    }

    onXChanged: {
        if (isDragging && isVertical) {
            fractional_position = x/parent.width
            dividerMoved()
        }
    }

    signal dividerMoved

    MouseArea{ id: mArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: isVertical? Qt.SizeHorCursor : Qt.SizeVerCursor

        drag.target: parent
        drag.axis: isVertical? Drag.XAxis : Drag.YAxis
        drag.smoothed: false
        drag.minimumY: drag.minimumX
        drag.maximumY: drag.maximumX
        drag.minimumX: minLimit + (dividerSize + thresholdSize)
        drag.maximumX: maxLimit - (dividerSize + thresholdSize)

    }

}