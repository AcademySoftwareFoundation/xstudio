// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import Qt.labs.qmlmodels 1.0 //for RadialGradient

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

Item { id: dialog
    anchors.fill: parent

    GTAttributes { id: attrs }

    MAttributes { id: mask_attrs }

    property real itemSpacing: 1
    property real buttonSpacing: 1
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight + 4
    property real panelPadding: XsStyleSheet.panelPadding
    property color panelColor: XsStyleSheet.panelBgColor

    property alias grading_sliders_model: attrs.grading_sliders_model
    property alias grading_wheels_model: attrs.grading_wheels_model

    Component.onCompleted: {
        // If created in hidden state, just rely on onVisibleChanged
        // to increment the property when the component becomes visible.
        if (visible) {
            attrs.tool_opened_count += 1
        }
    }
    Component.onDestruction: {
        attrs.tool_opened_count -= 1
    }

    onVisibleChanged: {
        if (attrs.tool_opened_count === undefined) {
            return
        }

        if (visible) {
            attrs.tool_opened_count += 1
        } else {
            attrs.tool_opened_count -= 1
        }
    }

    function hasActiveGrade() {
        return attrs.grading_bookmark && attrs.grading_bookmark != "00000000-0000-0000-0000-000000000000"
    }

    XsBookmarkFilterModel {
        id: bookmarkFilterModel
        sourceModel: bookmarkModel
        currentMedia: currentPlayhead.mediaUuid
        showHidden: true
        showUserType: "Grading"
        sortbyCreated: true
    }


    property alias moreMenu: menus.moreMenu
    
    Sec0Menu{
        id: menus
    }


    property alias bookmarkList: listDiv.bookmarkList
    
    Item { 
        anchors.fill: parent
        anchors.margins: panelPadding

        XsSplitView {
            id: rightPanel
            width: parent.width
            height: parent.height

            thumbWidth: XsStyleSheet.panelPadding
            colorHandleBg: XsStyleSheet.panelBgGradTopColor

            ColumnLayout{ id: leftBar
                SplitView.minimumWidth: 270
                Layout.preferredWidth: 290
                SplitView.fillHeight: true
                spacing: panelPadding
                
                Sec1Header{ id: header
                    Layout.fillWidth: true
                    Layout.preferredHeight: btnHeight
                    y: 0 //header.height + panelPaddingeight
                    Layout.maximumHeight: btnHeight
                }
                Sec2LayerList{ id: listDiv
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            ColumnLayout{ id: rightBar
                SplitView.minimumWidth: 200
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                spacing: panelPadding

                RowLayout{
                    Layout.fillWidth: true
                    Layout.preferredHeight: btnHeight
                    SplitView.fillHeight: true

                    Sec3MaskTools{
                        Layout.fillWidth: true
                        Layout.preferredHeight: btnHeight
                    } 
                    XsPrimaryButton{ id: moreBtn
                        Layout.preferredWidth: btnWidth
                        Layout.preferredHeight: btnHeight
                        Layout.alignment: Qt.AlignRight
                        imgSrc: "qrc:/icons/more_vert.svg"
                        onClicked:{
                            if(moreMenu.visible) moreMenu.visible = false
                            else{
                                moreMenu.x = x + width + leftBar.width
                                moreMenu.y = y + height
                                moreMenu.visible = true
                            }
                        }
                    }
                }        
                RowLayout{
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 1
    
                    GTSliderItem {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 150
                        Layout.maximumWidth: rightPanel.width/4
                        Layout.fillHeight: true
                    }
                    Repeater{
                        model: attrs.grading_wheels_model
    
                        GTWheelItem {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 150
                            Layout.maximumWidth: rightPanel.width/4
                        }
                    }
                }
            }
                
            
        }


    }

}
