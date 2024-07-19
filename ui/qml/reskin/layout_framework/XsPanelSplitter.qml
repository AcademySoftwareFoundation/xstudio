import QtQuick 2.15
import QtQuick.Controls 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0

/* This widget is a custom layout that divides itself between one or more
children. The children can be XsPanelSplitters too, and as such we can 
recursively subdivide a window into many resizable panels. It's all based
on the 'panels_layout_model' that is a tree like structure that drives the
recursion. The 'panels_layout_model' is itself backed by json data that comes
from the xstudio preferences files. Look for 'reskin_windows_and_panels_model'
in the preference files for more. In practice we problably don't need anything
this flexible but the capability is there in case we do need it one day. */
Rectangle {

    id: topItem
    color: "transparent"

    anchors.fill: parent
    property var panels_layout_model
    property var panels_layout_model_index
    property bool isVertical: true
    property var rowCount: panels_layout_model.rowCount(panels_layout_model_index)

    property var dividers: child_dividers !== undefined ? child_dividers : []
    
    Repeater {

        // 'child_dividers' is data exposed by the model and should be
        // a vector of floats- eachvalue saying where the split is in normalised
        // width/height of the pane. For a pane split into three equally sized panels,
        // say, you would have child_dividers = [0.333, 0.666]
        model: dividers.length

        XsDivider { 
            
            isVertical: topItem.isVertical                
            z:1000 // make sure the dividers are on top

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

                // using the dividers positions to work out the size and position
                // of this child panel
                property var d: index == 0 ? 0.0 : topItem.dividers[index-1]
                property var e: index >= topItem.dividers.length ? 1.0 : topItem.dividers[index]
                property var frac_size: e-d

                width: topItem.isVertical ? topItem.width*frac_size : topItem.width
                height: topItem.isVertical ? topItem.height : topItem.height*frac_size
                x: topItem.isVertical ? topItem.width*d : 0
                y: topItem.isVertical ? 0 : topItem.height*d

                property var child
                property var child_type

                function buildSubPanels() {                    

                    // if 'split_horizontal' is defined (either true or fale),
                    // then we have hit another splitter
                    if (split_horizontal !== undefined) {

                        if (child && child_type == "XsSplitPanel") {
                            child.buildSubPanels()
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
                                    isVertical: split_horizontal
                                })
        
                        } else {
                            console.log("component", component, component.errorString())
                        }

                    } else {

                        // 'split_horizontal' is not defined, so we are at a 
                        // 'leaf' node, as it were, and we need to create 
                        // the container that holds an actual UI panel
                        if (child && child_type == "XsPanelContainer") {
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
                }

                Component.onCompleted: {
                    buildSubPanels()
                }
            
            }
        }
    }
}