import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.models 1.0

XsListView {

    id: row
    orientation: ListView.Horizontal
    interactive: false

    property string placement: "left"
    property var dockedWidgetsModel
    property real framePadding: XsStyleSheet.panelPadding/2

    model: delegate_model

    XsModuleData {
        id: dockables
        modelDataName: "dockable viewport toolboxes"
    }

    // when dockedWidgetsModel array changes, qml can only say the whole thing
    // has changed (even when a single element has changed). So if our delegate
    // model was driven directly by dockedWidgetsModel all the widgets would
    // instantaneously be re-created or destroyed in response to a change so
    // we couldn't do fancy show/hide animation.
    // So for coherent tracking of which widgets are visible or not (to support
    // the animation) we build a ListModel from dockedWidgetsModel
    ListModel{
        id: docked_widgets
    }

    onDockedWidgetsModelChanged: {

        if (dockedWidgetsModel == undefined) return

        // dockedWidgetsModel is provided and managed by the parent widget
        // 'dockedWidgetsModel' here is an array - each element in the array should be
        // an array of length 3: [widget_name, visibility, placement]
        // where 'placement' will be 'left', 'right', 'top' or 'bottom' and
        // visibility is true or false
        var visible_widgets = []
        for (var i = 0; i < dockedWidgetsModel.length; ++i) {
            if (dockedWidgetsModel[i][2] == placement) {
                if (dockedWidgetsModel[i][1]) {
                    visible_widgets.push(dockedWidgetsModel[i][0])
                }
            }
        }

        // hide/show any existing widgets that aren't in visible_widgets
        for (var j = 0; j < docked_widgets.count; ++j) {
            if (visible_widgets.indexOf(docked_widgets.get(j).widget_name) == -1) {
                docked_widgets.get(j).showing = false
            } else {
                docked_widgets.get(j).showing = true
            }
        }

        // add widgets that are new
        for (var i = 0; i < visible_widgets.length; ++i) {
            var widget_name = visible_widgets[i]
            var found = false
            for (var j = 0; j < docked_widgets.count; ++j) {
                if (docked_widgets.get(j).widget_name == widget_name) {
                    found = true
                    break
                }
            }
            if (!found) {
                docked_widgets.append({"widget_name": widget_name, "showing": true})
            }
        }
    }

    DelegateModel {
        id: delegate_model
        model: docked_widgets
        delegate: Item {
            id: container
            clip: true
            height: row.height
            property var widgetName: widget_name
            property var dynamic_widget
            onWidgetNameChanged: load_widget(0)
            
            function load_widget(attempt) {
                var idx = dockables.searchRecursive(widgetName, "title")                
                if (!idx.valid && attempt < 10) {
                    callbackTimer.setTimeout(function() { return function() {
                        load_widget(attempt+1)
                    }}(), 500);
                    return
                } else if (!idx.valid) {
                    console.log("Unable to create dockable widget: ", widgetName)
                    return;
                }
                var source = dockables.get(idx, "left_right_dock_widget_qml_code")
                if (source != undefined && source != "") {
                    dynamic_widget = Qt.createQmlObject(source, container)
                } else if (source == undefined) {
                    console.log("Unable to make a", widgetName, "left/right dockable widget - plugin does not provide the widget")
                }

                widgetNameForMenu = widgetName
            }
            states: [
                State {
                    name: "showing"
                    when: showing
                    PropertyChanges { target: container; implicitWidth: dynamic_widget ? dynamic_widget.dockWidgetSize : 0}
                },
                State {
                    name: "hiding"
                    when: !showing
                    PropertyChanges { target: container; implicitWidth: 0}
                }
            ]

            transitions: Transition {
                NumberAnimation { properties: "implicitWidth"; duration: 150 }
            }

            XsButtonWithImageAndText{ id: groupBtn
                iconText: "Position"
                anchors.bottom: parent.bottom
                width: parent.width - framePadding*2
                height: XsStyleSheet.primaryButtonStdHeight
                anchors.horizontalCenter: parent.horizontalCenter
                z: 100
                iconSrc: "qrc:/icons/dock_left.svg"
                iconRotation: placement == "left" ? 0 : 180
                textDiv.visible: true
                textDiv.font.bold: false
                textDiv.font.pixelSize: XsStyleSheet.fontSize
                paddingSpace: 4

                onClicked:{
                    if(dockPositionMenu.visible) dockPositionMenu.visible = false
                    else{
                        dockPositionMenu.x = x
                        dockPositionMenu.y = y + height
                        dockPositionMenu.visible = true
                    }
                }
                forcedBgColorNormal: XsStyleSheet.panelBgGradTopColor
            }

        }
    }


    XsPopupMenu {
        id: dockPositionMenu
        visible: false
        menu_model_name: "dockMenu"+ row
    }

    property var widgetNameForMenu: null
    XsMenuModelItem {
        text: "Left"
        // enabled: annotationBtn.text !== "Left"
        menuPath: ""
        menuCustomIcon: "qrc:/icons/dock_left.svg"
        menuItemPosition: 1
        menuModelName: dockPositionMenu.menu_model_name
        onActivated: {
            move_dockable_widget(widgetNameForMenu, "left")
        }
    }
    XsMenuModelItem {
        text: "Right"
        menuPath: ""
        menuCustomIcon: "qrc:/icons/dock_right.svg"
        menuItemPosition: 2
        menuModelName: dockPositionMenu.menu_model_name
        onActivated: {
            move_dockable_widget(widgetNameForMenu, "right")
        }
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 3
        menuPath: ""
        menuModelName: dockPositionMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Top"
        menuPath: ""
        menuCustomIcon: "qrc:/icons/dock_bottom_flipped.png"
        menuItemPosition: 4
        menuModelName: dockPositionMenu.menu_model_name
        onActivated: {
            move_dockable_widget(widgetNameForMenu, "top")
        }
    }
    XsMenuModelItem {
        text: "Bottom"
        menuPath: ""
        menuCustomIcon: "qrc:/icons/dock_bottom.svg"
        menuItemPosition: 5
        menuModelName: dockPositionMenu.menu_model_name
        onActivated: {
            move_dockable_widget(widgetNameForMenu, "bottom")
        }
    }


}