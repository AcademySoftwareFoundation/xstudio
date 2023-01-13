// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.12

import xStudio 1.0
Item {

    width: box_size+(4*drag_drop_items.length-1)
    height: width
    property var drag_drop_items: []
    z:100
    property int box_size: 40

    Repeater {

        id: drag_outside_repeater
        model: drag_drop_items

        Rectangle {
            id: theRect
            width: box_size
            height: box_size
            color: "transparent"
            property var source_aspect: icon.sourceSize.width/icon.sourceSize.height

            Image {
                id: icon
                anchors.fill: parent
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                cache: true
                source: drag_drop_items[index].current ? drag_drop_items[index].current.thumbnailURL : "qrc:///feather_icons/film.svg"
                // sourceSize.height: height * 5
                // sourceSize.width:  width * 5
                smooth: true
                transformOrigin: Item.Center
                
                /*layer {
                    enabled: !(media_item  &&  media_item.current)
                    effect: ColorOverlay {
                        color: XsStyle.controlTitleColor
                    }
                }
                visible: imageVisible === 1*/
            }

            Rectangle {
                x: (box_size - width)/2.0
                y: (box_size - height)/2.0
                width: source_aspect > 1 ? box_size : box_size*source_aspect
                height: source_aspect > 1 ? box_size/source_aspect : box_size
                color: "transparent"
                border.color: "gray"
            }

        
        }

        onCountChanged: {
            var sx = -count/2*4-box_size/2
            var tt = false
            for (var idx = 0; idx < count; idx++) {
                itemAt(idx).x = sx
                itemAt(idx).y = sx
                sx = sx + 4
            }
        }

    }
}

