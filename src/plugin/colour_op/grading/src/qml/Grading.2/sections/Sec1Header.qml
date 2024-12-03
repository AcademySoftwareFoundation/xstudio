// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

Item{ id: toolDiv

    property real categoryBtnWidth: btnWidth * 1.3

    RowLayout{
        spacing: buttonSpacing
        width: parent.width
        height: btnHeight
        anchors.centerIn: parent

        XsPrimaryButton{ id: addBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            Layout.alignment: Qt.AlignLeft
            imgSrc: "qrc:/icons/add.svg"
            //tooltip: "Add a color correction"
            onClicked: {
                attrs.grading_action = "Add CC"
            }
        }
        XsPrimaryButton{ id: deleteBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            imgSrc: "qrc:/icons/delete.svg"
            //tooltip: "Remove the currently selected color correction"
            enabled: bookmarkList.count > 0
            onClicked: {
                attrs.grading_action = "Remove CC"
            }
        }
        Item{
            Layout.fillWidth: true
            Layout.minimumWidth: 0//btnWidth
            Layout.preferredHeight: btnHeight
        }
        XsPrimaryButton{ id: hideBtn
            Layout.preferredWidth: btnWidth*2
            Layout.preferredHeight: btnHeight
            text: "Hide Shapes"
            imgSrc: ""
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            isActive: !mask_attrs.mask_shapes_visible
            onClicked:{
                mask_attrs.mask_shapes_visible = !mask_attrs.mask_shapes_visible;
            }
        }
        XsPrimaryButton{ id: bypassBtn
            Layout.preferredWidth: btnWidth*1.5
            Layout.preferredHeight: btnHeight
            text: "Bypass"
            imgSrc: ""
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            //tooltip: "Bypass all CDLs or not"
            isActive: attrs.grading_bypass
            onClicked: {
                attrs.grading_bypass = !attrs.grading_bypass
            }
            toolTip: "Bypass all grades applied on all media "
            hotkeyNameForTooltip: "Bypass all grades"
            
        }
        XsPrimaryButton{ id: moreBtn
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: btnHeight
            Layout.alignment: Qt.AlignRight
            imgSrc: "qrc:/icons/more_vert.svg"
            onClicked:{
                if(moreMenu.visible) moreMenu.visible = false
                else{
                    moreMenu.x = x + width
                    moreMenu.y = y + height
                    moreMenu.visible = true
                }
            }
        }

    }
}