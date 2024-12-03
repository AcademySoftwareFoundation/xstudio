import QtQuick 2.15
import QtQuick.Controls 2.15

import xStudio 1.0

Popup {

    id: widget
    topPadding: XsStyleSheet.menuPadding
    bottomPadding: XsStyleSheet.menuPadding
    leftPadding: 0
    rightPadding: 0

    property bool repositioned: false
    // parent: Overlay.overlay //#TODO

    property alias bgDiv: bgDiv

    background: XsGradientRectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 200
        border.width: 1
        border.color: XsStyleSheet.baseColor
    }

    onVisibleChanged: {
        if (visible && !repositioned) {
            repositionPopupMenu(widget, widget.parent, x, y, undefined)
        } else if (!visible) {
            repositioned = false
        }
    }

    onWidthChanged: {
        if (visible) {
            repositionPopupMenu(widget, widget.parent, x, y, undefined)
        }
    }

    onHeightChanged: {
        if (visible) {
            repositionPopupMenu(widget, widget.parent, x, y, undefined)
        }
    }

    function showMenu(refItem, refX, refY, altRightEdge) {

        // this function should be defined at the parent Window level
        repositionPopupMenu(widget, refItem, refX, refY, altRightEdge)
    }

}


