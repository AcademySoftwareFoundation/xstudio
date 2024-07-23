// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
// import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import "./widgets"

Item {
    id: transportBar
    width: parent.width
    height: btnHeight+(barPadding*2)

    property string panelIdForMenu: panelId

    property real barPadding: XsStyleSheet.panelPadding
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+(2*2)

    /*************************************************************************
    
        Access Playhead data
    
    **************************************************************************/
    XsModelProperty {
        id: __playheadLogicalFrame
        role: "value"
        index: currentPlayheadData.search_recursive("Logical Frame", "title")
    }
    XsModelProperty {
        id: __playheadPlaying
        role: "value"
        index: currentPlayheadData.search_recursive("playing", "title")
    }        
    Connections {
        target: currentPlayheadData // this bubbles up from XsSessionWindow
        function onJsonChanged() {
            __playheadLogicalFrame.index = currentPlayheadData.search_recursive("Logical Frame", "title")
            __playheadPlaying.index = currentPlayheadData.search_recursive("playing", "title")
        }
    }
    property alias playheadLogicalFrame: __playheadLogicalFrame.value
    property alias playheadPlaying: __playheadPlaying.value
    /*************************************************************************/
    
    RowLayout{
        x: barPadding
        spacing: barPadding
        width: parent.width-(x*2)
        height: btnHeight
        anchors.verticalCenter: parent.verticalCenter
        
        RowLayout{
            spacing: 1
            Layout.preferredWidth: btnWidth*5
            Layout.maximumHeight: parent.height

            XsPrimaryButton{ id: rewindButton 
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/fast_rewind.svg"
            }
            XsPrimaryButton{ id: previousButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/skip_previous.svg"
            }
            XsPrimaryButton{ id: playButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: playheadPlaying ? "qrc:/icons/pause.svg" : "qrc:/icons/play_arrow.svg"
                onClicked: playheadPlaying = !playheadPlaying
            }
            XsPrimaryButton{ id: nextButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/skip_next.svg"
            }
            XsPrimaryButton{ id: forwardButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/fast_forward.svg"
            }
        }

        XsViewerTextDisplay{ 
            
            id: playheadPosition
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: parent.height
            text: playheadLogicalFrame !== undefined ? playheadLogicalFrame : "-"
            modelDataName: playheadPosition.text+"_ButtonMenu"+panelIdForMenu
            menuWidth: 175

            XsMenuModelItem {
                text: "Time Display"
                menuPath: ""
                menuItemType: "multichoice"
                menuItemPosition: 1
                choices: ["Frames", "Time", "Timecode", "Frames From Timecode"]
                currentChoice: "Frames"
                menuModelName: playheadPosition.text+"_ButtonMenu"+panelIdForMenu
            }
        }

        Rectangle{ id: timeFrame
            Layout.fillWidth: true            
            Layout.preferredHeight: parent.height
            color: "black"
        }
        XsViewerTextDisplay{ id: duration
            Layout.preferredWidth: btnWidth
            Layout.preferredHeight: parent.height
            text: "24.0"
            modelDataName: duration.text+"_ButtonMenu"+panelIdForMenu
            menuWidth: 105


            XsMenuModelItem {
                text: "Duration"
                menuPath: ""
                menuItemPosition: 1
                menuItemType: "toggle"
                menuModelName: duration.text+"_ButtonMenu"+panelIdForMenu
                onActivated: {
                }
            }
            XsMenuModelItem {
                text: "Remaining"
                menuPath: ""
                menuItemPosition: 2
                menuItemType: "toggle"
                menuModelName: duration.text+"_ButtonMenu"+panelIdForMenu
                onActivated: {
                }
            }
            XsMenuModelItem {
                text: "FPS"
                menuPath: ""
                menuItemPosition: 3
                menuItemType: "toggle"
                menuModelName: duration.text+"_ButtonMenu"+panelIdForMenu
                onActivated: {
                }
            }
        }

        RowLayout{
            spacing: 1
            Layout.preferredWidth: (btnWidth*4)+spacing*4
            Layout.preferredHeight: parent.height

            XsViewerVolumeButton{ id: volumeButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                volume: 4
            }
            XsPrimaryButton{ id: loopModeButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/repeat.svg"
                isActive: loopModeBtnMenu.visible

                onClicked: {
                    loopModeBtnMenu.x = x-width//*2
                    loopModeBtnMenu.y = y-loopModeBtnMenu.height
                    loopModeBtnMenu.visible = !loopModeBtnMenu.visible
                }

                XsMenuNew { 
                    id: loopModeBtnMenu
                    // visible: false
                    menu_model: loopModeBtnMenuModel
                    menu_model_index: loopModeBtnMenuModel.index(-1, -1)
                    menuWidth: 100
                }
                XsMenusModel {
                    id: loopModeBtnMenuModel
                    modelDataName: "LoopModeMenu-"+panelIdForMenu
                    onJsonChanged: {
                        loopModeBtnMenu.menu_model_index = index(-1, -1)
                    }
                }
                XsMenuModelItem {
                    text: "Play Once"
                    menuPath: ""
                    menuItemPosition: 1
                    menuItemType: "toggle"
                    menuModelName: "LoopModeMenu-"+panelIdForMenu
                    onActivated: {
                    }
                }
                XsMenuModelItem {
                    text: "Loop"
                    menuPath: ""
                    menuItemPosition: 2
                    menuItemType: "toggle"
                    menuModelName: "LoopModeMenu-"+panelIdForMenu
                    onActivated: {
                    }
                }
                XsMenuModelItem {
                    text: "Ping Pong"
                    menuPath: ""
                    menuItemPosition: 3
                    menuItemType: "toggle"
                    menuModelName: "LoopModeMenu-"+panelIdForMenu
                    onActivated: {
                    }
                }
            }
            XsPrimaryButton{ id: snapshotButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/photo_camera.svg"
                enabled: false
            }
            XsPrimaryButton{ id: popoutButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/open_in_new.svg"
                enabled: false
            }

        }
    }

}