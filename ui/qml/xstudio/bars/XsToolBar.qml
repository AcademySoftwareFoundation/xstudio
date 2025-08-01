// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.14
import QtQuick.Extras 1.4

import xStudio 1.0

import xstudio.qml.module 1.0
import xstudio.qml.models 1.0

import BasicViewportMask 1.0

Rectangle {
    id: myBar
    property int barHeight: 21
    property int topPadding: XsStyle.lrMargin
    height: barHeight
    property int separator_width: 1

    color: XsStyle.mainBackground
    implicitHeight: {
        return (barHeight + topPadding) * opacity
    }

    XsModuleData {
        id: tester
        modelDataName: viewport.name + "_toolbar"
    }

    opacity: 1
    visible: opacity !== 0

    Behavior on opacity {
        NumberAnimation { duration: playerWidget.doTrayAnim?200:0 }
    }

    property var collapse_all: false;

    function checkCollapseMode() {

        // check if any of the widgets need to minimise, if so minimise
        // all widgets (looks much nicer this way) or un-minimize
        var c = false
        for (var index = 0; index < the_view.count; index++) {
            var the_item = the_view.itemAt(index)
            if (the_item) {
                var curr_but = the_item.item
                if (curr_but && curr_but.suggestedCollapseMode == 3) {
                    c = true;
                    break;
                }
            }
        }
        if (c != collapse_all) collapse_all = c

    }

    onCollapse_allChanged: {

        for (var index = 0; index < the_view.count; index++) {
            var obj = the_view.itemAt(index)
            if(obj && obj.item) {
                obj.item.collapse = collapse_all
            }
        }
    }

    onWidthChanged: {
        checkCollapseMode()
    }

    Repeater {

        id: the_view
        anchors.fill: parent
        model: tester

        delegate: Item {

            id: parent_item
            anchors.bottom: parent.bottom
            anchors.top: parent.top
            width: (myBar.width-separator_width*(the_view.count-1))/the_view.count
            property var ordered_x_position: 0
            x: index*(width+separator_width)
            property var dynamic_widget

            // 'title', 'type', 'qml_code' attributes may or may not be provided by the Repeater model,
            // which connects with attributes in the backend via the Module class. We need to map
            // them to local variables like this so we get signals when they are set or changed
            property var title_: title ? title : "nought"
            property var type_: type ? type : null
            property var qml_code_: qml_code ? qml_code : null

            onQml_code_Changed: {
                if (qml_code_) {
                    dynamic_widget = Qt.createQmlObject(qml_code_, parent_item)
                }
            }

            // attributes can have a floating point 'toolbar_position' data value ... this is
            // used to position the attribute widgets in the toolbar, with the one with the
            // lowest toolbar_position value left most and the highest rightmost. See arrange_widgets()
            property var order_value: dynamic_widget ? dynamic_widget.toolbar_position_ ? dynamic_widget.toolbar_position_ : 10000.0 : 0

            // for standard attribute types, we have standard toolbar widgets which
            // we load dynamically here
            onType_Changed: {

                if (qml_code_) {
                    // skip, handled above
                } else if (type == "FloatScrubber") {
                    dynamic_widget = Qt.createQmlObject('import "../base/widgets"; XsToolbarFloatScrubber { anchors.fill: parent}', parent_item)
                } else if (type == "ComboBox") {
                    dynamic_widget = Qt.createQmlObject('import "../base/widgets"; XsToolbarComboBox { anchors.fill: parent}', parent_item)
                } else if (type == "OnOffToggle") {
                    dynamic_widget = Qt.createQmlObject('import "../base/widgets"; XsToolbarOnOffToggle { anchors.fill: parent}', parent_item)
                } else {
                    console.log("XsToolBar failed to instance widget, unknown type:", type)
                }
            }
        }
    }
}
