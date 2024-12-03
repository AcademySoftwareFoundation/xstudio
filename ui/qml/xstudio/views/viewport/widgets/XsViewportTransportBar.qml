import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
// import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import "."
import "./transport_bar"

Item {
    id: transportBar
    width: parent.width

    height: (btnHeight+(barPadding*2))*opacity
    visible: opacity != 0.0
    Behavior on opacity {NumberAnimation{ duration: 150 }}

    property string panelIdForMenu: panelId //TODO: to fix, not defined for quick-view

    property real barPadding: XsStyleSheet.panelPadding
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+(2*2)

    function skipToNext(forwards) {

        // jumpToNext will return false if there is only one source selected
        if (viewportPlayhead.mediaTransitionFrames.length > 1) {
            if (forwards) {
                var t = viewportPlayhead.mediaTransitionFrames;
                for (var i = 0; i < t.length; ++i) {
                    if (t[i] > viewportPlayhead.logicalFrame) {
                        viewportPlayhead.logicalFrame = t[i]
                        return
                    }
                }
                return;
            } else {
                var t = viewportPlayhead.mediaTransitionFrames;
                for (var i = (t.length-1); i >= 0; --i) {
                    if (t[i] < viewportPlayhead.logicalFrame) {
                        viewportPlayhead.logicalFrame = t[i]
                        return
                    }
                }
            }
        }

        // move selection forwards or backwards
        if (mediaSelectionModel.selectedIndexes.length == 1) {
            var idx = mediaSelectionModel.selectedIndexes[0]
            var idx2 = idx.model.index(idx.row + (forwards ? 1 : -1),0,idx.parent)
            if (idx2.valid)
                mediaSelectionModel.select(idx2, ItemSelectionModel.ClearAndSelect)
        }

    }

    function fastPlayback(rewind) {
        if (!rewind && !viewportPlayhead.playingForwards) {
            // playing backwards but fast forward was hit ... play forwards
            // at normal speed
            viewportPlayhead.playingForwards = true
            viewportPlayhead.velocityMultiplier = 1.0
        } else if (rewind && viewportPlayhead.playingForwards) {
            // playing forwards but fast rewind was hit ... play backwards
            // at normal speed
            viewportPlayhead.playingForwards = false
            viewportPlayhead.velocityMultiplier = 1.0
        } else {
            if (viewportPlayhead.velocityMultiplier == 16.0) {
                viewportPlayhead.velocityMultiplier = 1.0
            } else {
                viewportPlayhead.velocityMultiplier = viewportPlayhead.velocityMultiplier*2.0;
            }
        }
    }

    RowLayout{
        x: barPadding
        spacing: barPadding
        width: parent.width-(x*2)
        height: btnHeight
        anchors.verticalCenter: parent.verticalCenter

        RowLayout{
            spacing: 1
            Layout.preferredWidth: btnWidth*3
            Layout.maximumHeight: parent.height

            // XsPrimaryButton{
            //     id: rewindButton
            //     Layout.preferredWidth: btnWidth
            //     Layout.preferredHeight: parent.height
            //     imgSrc: "qrc:/icons/fast_rewind.svg"
            //     onClicked: fastPlayback(true)
            //     isActive: !viewportPlayhead.playginForwards && viewportPlayhead.velocityMultiplier > 1
            //     XsLabel {
            //         anchors.right: parent.right
            //         anchors.bottom: parent.bottom
            //         anchors.margins: 3
            //         color: palette.highlight
            //         text: viewportPlayhead.velocityMultiplier > 1 ? "x" + viewportPlayhead.velocityMultiplier : ""
            //         shadow: true
            //         visible: rewindButton.isActive
            //     }
            // }
            XsPrimaryButton{ id: previousButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/skip_previous.svg"
                onPressed: skipToNext(false)
            }
            XsPrimaryButton{ id: playButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: viewportPlayhead.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play_arrow.svg"
                onClicked: {
                    if(!viewportPlayhead.playing) {
                        viewportPlayhead.velocityMultiplier = 1.0
                        viewportPlayhead.playingForwards = true
                    }
                    viewportPlayhead.playing = !viewportPlayhead.playing
                }
            }
            XsPrimaryButton{ id: nextButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/skip_next.svg"
                onPressed: skipToNext(true)
            }
            // XsPrimaryButton{ id: forwardButton
            //     Layout.preferredWidth: btnWidth
            //     Layout.preferredHeight: parent.height
            //     imgSrc: "qrc:/icons/fast_forward.svg"
            //     onClicked: fastPlayback(false)
            //     isActive: (viewportPlayhead.playginForwards == true && viewportPlayhead.velocityMultiplier > 1.0)
            //     XsLabel {
            //         anchors.right: parent.right
            //         anchors.bottom: parent.bottom
            //         anchors.margins: 3
            //         color: palette.highlight
            //         text: viewportPlayhead.velocityMultiplier > 1 ? "x" + viewportPlayhead.velocityMultiplier : ""
            //         shadow: true
            //         visible: forwardButton.isActive
            //     }
            // }
        }

        XsCurrentFrameIndicator {
            id: currentFrameInd
            fontSize: 14
        }

        Rectangle {
            color: "black"
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height

            XsViewerTimeline {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                anchors.topMargin: 2
                anchors.bottomMargin: 2
            }
        }


        XsDurationFPSIndicator {
            timelineUnits: currentFrameInd.timelineUnits
            fontSize: 14
        }

        RowLayout{
            spacing: 1
            Layout.preferredHeight: parent.height

            XsViewerVolumeButton{
                id: volumeButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                volume: 4
                enabled: !volumeButton.muted
            }

            XsPrimaryButton{ id: muteButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height

                imgSrc: "qrc:/icons/volume_off.svg"

                isActive: volumeButton.muted != undefined ? volumeButton.muted : false

                property color bgColorPressed: palette.highlight
                property color bgColorNormal: "#1AFFFFFF"
                property color forcedBgColorNormal: "#E6676767" //bgColorNormal

                onClicked: volumeButton.muted = !volumeButton.muted
            }



            XsPrimaryButton{

                id: loopModeButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: viewportPlayhead.loopMode == "Loop" ? "qrc:/icons/repeat.svg" : viewportPlayhead.loopMode == "Play Once" ? "qrc:/icons/keyboard_tab.svg" : "qrc:/icons/arrow_range.svg"
                isActive: loopModeBtnMenu.visible
                enabled: viewportPlayhead.loopMode != undefined

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
                    menuPath: ""
                    menuItemType: "radiogroup"
                    menuItemPosition: 1
                    choices: ["Play Once", "Loop", "Ping Pong"]
                    currentChoice: viewportPlayhead.loopMode ? viewportPlayhead.loopMode : "Loop"
                    property var lm: viewportPlayhead.loopMode
                    onLmChanged: {
                        if (viewportPlayhead.loopMode != currentChoice && viewportPlayhead.loopMode != undefined) {
                            currentChoice = viewportPlayhead.loopMode
                        }
                    }
                    onCurrentChoiceChanged: {
                        if (viewportPlayhead.loopMode != currentChoice) {
                            viewportPlayhead.loopMode = currentChoice
                        }
                    }
                    menuModelName: "LoopModeMenu-"+panelIdForMenu
                }

            }

            XsPrimaryButton{
                id: snapshotButton
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/photo_camera.svg"
                onClicked: {
                    view.doSnapshot()
                }
            }

        }
    }

}