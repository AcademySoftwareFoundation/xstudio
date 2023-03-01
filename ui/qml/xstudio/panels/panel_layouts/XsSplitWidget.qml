// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0


//    A simple rectangle item that sizes two sibling
//    items inside itself with a vertical splitter that
//    can be dragged left/right. It also handles
//    hiding either one of the widgets, making the other
//    widget fill its area
//
//    |----------------------------|
//    |                            |
//    |             A              |
//    |                            |
//    |                            |
//    |----------------------------|
//    |                            |
//    |                            |
//    |             B              |
//    |                            |
//    |----------------------------|

Rectangle {

    color: "#00000000"
    property var widgetA
    property var widgetB
    property bool active
    property int index: 0
    property var prefs_store_object: undefined
    property bool is_horizontal: true
    property var divider: is_horizontal ? horiz_divider : vert_divider
    property var ratio: divider.ratio

    onRatioChanged: {
        if (!divider.hidden && prefs_store_object != undefined && prefs_store_object.properties.divider_positions) {
            // console.log(prefs_store_object, prefs_store_object.index)
            let ratios = prefs_store_object.properties.divider_positions
            if (ratios != undefined) {
                ratios[index] = ratio
                prefs_store_object.properties.divider_positions = ratios
            }
        }
        arrange()
    }

    Component.onCompleted: arrange()
    onActiveChanged: arrange()
    onVisibleChanged: arrange()
    onWidthChanged: arrange()
    onHeightChanged: arrange()
    onWidgetAChanged: arrange()
    onWidgetBChanged: arrange()

    XsHorizontalPaneDivider {

        anchors.left: parent.left
        anchors.right: parent.right
        visible: parent.is_horizontal
        id: horiz_divider
        ratio: prefs_store_object !== undefined ? prefs_store_object.properties.divider_positions[index] : 0.3
        onYChanged: arrange()
        onHeightChanged: arrange()

    }

    XsVerticalPaneDivider {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        ratio: prefs_store_object  !== undefined ? prefs_store_object.properties.divider_positions[index] : 0.3
        visible: !parent.is_horizontal
        id: vert_divider
        onXChanged: arrange()
        onWidthChanged: arrange()

    }

    // if one of the widgets is hidden, return the other widget
    // that is 'filling' this splitter
    function fill_widget() {
        if (divider.ratio <= 0.0) {
            return widgetB
        } else if (divider.ratio >= 1.0) {
            return widgetA
        }
        return null
    }

    function recursiveContainsWidget(widget) {
        if (widgetA == widget || widgetB == widget) return true;
        if (widgetA.recursiveContainsWidget != undefined && widgetA.recursiveContainsWidget(widget)) return true
        if (widgetB.recursiveContainsWidget != undefined && widgetB.recursiveContainsWidget(widget)) return true
        return false
    }

    function sectionAContainsWidget(widget) {
        if (widgetA == widget) return true
        if (widgetA.recursiveContainsWidget != undefined) return widgetA.recursiveContainsWidget(widget)
        return false
    }

    function sectionBContainsWidget(widget) {
        if (widgetB == widget) return true
        if (widgetB.recursiveContainsWidget != undefined) return widgetB.recursiveContainsWidget(widget)
        return false
    }

    function sectionAIsFilledWith(widget) {
        return widgetA.fill_widget != undefined && widgetA.fill_widget() == widget;
    }

    function hide_widget(widget) {
        if (widget == widgetA || (widgetA.fill_widget != undefined && widgetA.fill_widget() == widget)) {
            divider.send_to(0.0)
        } else if (widget == widgetB || (widgetB.fill_widget != undefined && widgetB.fill_widget() == widget)) {
            divider.send_to(1.0)
        } else {
            if (widgetA && widgetA.hide_widget != undefined) {
                widgetA.hide_widget(widget)
            }
            if (widgetB && widgetB.hide_widget != undefined) {
                widgetB.hide_widget(widget)
            }
        }
    }

    function unhide_widget(widget) {

        if (sectionAContainsWidget(widget)) {

            if (divider.ratio <= 0.0) {
                divider.unhide()
            }
            if (widgetA.unhide_widget != undefined) {
                widgetA.unhide_widget(widget)
            }
        }

        if (sectionBContainsWidget(widget)) {
            if (divider.ratio >= 1.0) {
                divider.unhide()
            }
            if (widgetB.unhide_widget != undefined) {
                widgetB.unhide_widget(widget)
            }
        }

    }

    function arrange() {

        if (!active) return
        if (!visible) {
            widgetA.visible = false
            widgetB.visible = false
            return
        }

        var global_pos = parent.mapToGlobal(x, y)

        if (is_horizontal) {
            if (widgetA) {
                if (divider.ratio <= 0.0) {
                    widgetA.visible = false
                } else {
                    widgetA.visible = true
                    var local_pos = widgetA.parent.mapFromGlobal(global_pos.x, global_pos.y)
                    widgetA.x = local_pos.x
                    widgetA.y = local_pos.y
                    widgetA.height = divider.y
                    widgetA.width = width
                }
            }
            if (widgetB) {
                if (divider.ratio >= 1.0) {
                    widgetB.visible = false
                } else {
                    widgetB.visible = true
                    var local_pos = widgetB.parent.mapFromGlobal(global_pos.x, global_pos.y)
                    widgetB.x = local_pos.x
                    widgetB.y = local_pos.y + divider.y + divider.height
                    widgetB.height = height - (divider.y + divider.height)
                    widgetB.width = width
                }
            }
        } else {

            if (widgetA) {
                if (vert_divider.ratio <= 0.0) {
                    widgetA.visible = false
                } else {
                    widgetA.visible = true
                    var local_pos = widgetA.parent.mapFromGlobal(global_pos.x, global_pos.y)
                    widgetA.x = local_pos.x
                    widgetA.y = local_pos.y
                    widgetA.height = height
                    widgetA.width = vert_divider.x
                }
            }
            if (widgetB) {
                if (vert_divider.ratio >= 1.0) {
                    widgetB.visible = false
                } else {
                    widgetB.visible = true
                    var local_pos = widgetB.parent.mapFromGlobal(global_pos.x, global_pos.y)
                    widgetB.x = local_pos.x + vert_divider.x + vert_divider.width
                    widgetB.y = local_pos.y
                    widgetB.height = height
                    widgetB.width = width - (vert_divider.x + vert_divider.width)
                }
            }
        }

    }
}