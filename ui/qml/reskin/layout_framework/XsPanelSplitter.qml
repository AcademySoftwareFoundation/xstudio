import QtQuick 2.15
import QtQuick.Controls 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0


Rectangle {

    id: topItem
    color: "transparent"
    property var panels_layout_model
    property var panels_layout_model_index
    property bool isVertical: true
    property var rowCount: panels_layout_model.rowCount(panels_layout_model_index)
    onHeightChanged: {
        resizeWidgets()
    }

    onWidthChanged: {
        resizeWidgets()
    }
    onRowCountChanged: {
        resizeWidgets()
    }

    Repeater {

        model: DelegateModel{           

            model: panels_layout_model
            rootIndex: panels_layout_model_index
            delegate: 

            XsDivider { 
                
                visible: index != 0
                isVertical: topItem.isVertical                
                z:1000

                onDividerMoved: {
                    topItem.resizeWidgets()
                }
            }
        }
    }

    function resizeWidgets() {
        var prev_frac = 0.0
        let rc = panels_layout_model.rowCount(panels_layout_model_index)
        if (rc) {
            panels_layout_model.set(panels_layout_model.index(0, 0, panels_layout_model_index), 0.0, "fractional_position")
            for(let i=1; i<rc; i++) {
                var pos = panels_layout_model.get(panels_layout_model.index(i, 0, panels_layout_model_index), "fractional_position");
                if (pos == undefined || pos == null) {
                    panels_layout_model.set(panels_layout_model.index(i, 0, panels_layout_model_index), (i)/rc, "fractional_position");
                }
            }
        }
        for(let i=0; i<rc; i++) {
            var frac = i == (rc-1) ? 1.0 : panels_layout_model.get(panels_layout_model.index(i+1, 0, panels_layout_model_index), "fractional_position")
            if (frac == undefined || frac == null) frac = (i+1)/rc
            panels_layout_model.set(panels_layout_model.index(i, 0, panels_layout_model_index), frac-prev_frac, "fractional_size")
            prev_frac = frac
        }
    }

    Repeater {

        id: the_view

        model: DelegateModel{           

            model: panels_layout_model
            rootIndex: panels_layout_model_index
            delegate: 

            Item { 
                
                id: root

                width: topItem.isVertical ? topItem.width*fractional_size : topItem.width
                height: topItem.isVertical ? topItem.height : topItem.height*fractional_size
                x: topItem.isVertical ? topItem.width*fractional_position : 0
                y: topItem.isVertical ? 0 : topItem.height*fractional_position
                property var child
                property var ll: split
                property var ll2: panel_source_qml
                property var ll3: fractional_position
                property var ll4: fractional_size
                property var child_type

                onLl4Changed: {
                    if (child) {
                        child.width = width
                        child.height = height
                    }
                }

                onWidthChanged: {
                    if (child) child.width = width
                }
                onHeightChanged: {
                    if (child) child.height = height
                }

                function resizeWidgets() {
                    if (child) {
                        child.width = width
                        child.height = height
                    }
                    topItem.resizeWidgets()
                }

                function loadWindows() {                    

                    if (split !== undefined && split !== "tbd") {

        

                        if (child && child_type == "XsSplitPanel") {
                            resizeWidgets()
                            return
                        }
                        if (child) child.destroy()
                        
                        let component = Qt.createComponent("./XsPanelSplitter.qml")
                        let recurse_into_model_idx = topItem.panels_layout_model.index(index,0,panels_layout_model_index)
                        child_type = "XsSplitPanel"

                        if (component.status == Component.Ready) {
        
                            child = component.createObject(
                                root,
                                {
                                    panels_layout_model: topItem.panels_layout_model,
                                    panels_layout_model_index: recurse_into_model_idx,
                                    isVertical: split == "widthwise"
                                })
        
                        } else {
                            console.log("component", component, component.errorString())
                        }

                    } else {

                        if (child && child_type == "XsPanelContainer") {
                            resizeWidgets()
                            return
                        }
                        if (child) child.destroy()
                        child_type = "XsPanelContainer"

                        let component = Qt.createComponent("./XsViewContainer.qml")
                        let recurse_into_model_idx = topItem.panels_layout_model.index(index,0,panels_layout_model_index)
                        if (component.status == Component.Ready) {
        
                            child = component.createObject(
                                root,
                                {
                                    panels_layout_model: topItem.panels_layout_model,
                                    panels_layout_model_index: recurse_into_model_idx,
                                    panels_layout_model_row: index
                                })
        
                        } else {
                            console.log("component", component, component.errorString())
                        }
                    }
                    resizeWidgets()
                }

                onLlChanged: {
                    loadWindows()
                }

                onLl2Changed: {
                    loadWindows()
                }

                Component.onCompleted: {
                    loadWindows()
                }
            
            }
        }
    }
}