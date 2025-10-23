import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// This Item extends the pure 'Viewport' QQuickItem from the cpp side

Item {
    
    id: root
    function loadMedia(path) {
        view.quickViewFromPath(path);
    }

    function playToggle() {
        viewportPlayhead.playing = true
    }

    XsPlayhead {
        id: viewportPlayhead
        uuid: view.playheadUuid
    }

    ColumnLayout {

        anchors.fill: parent
        spacing: 2

        Viewport {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            z: 100.0
            isQuickview: true
            hasOverlays: false
        }

        Rectangle{
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            color: "black"
            RowLayout {
                anchors.fill: parent

                XsSecondaryButton { 

                    Layout.preferredWidth: XsStyleSheet.widgetStdHeight
                    Layout.fillHeight: true
                    imgSrc: "qrc:/icons/close.svg"
                    onClicked: {
                        viewportPlayhead.playing = false
                        root.visible = false
                    }
                }

                XsSecondaryButton { 

                    Layout.preferredWidth: XsStyleSheet.widgetStdHeight
                    Layout.fillHeight: true
                    imgSrc: viewportPlayhead.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play_arrow.svg"
                    onClicked: {
                        if(!viewportPlayhead.playing) {
                            viewportPlayhead.velocityMultiplier = 1.0
                            viewportPlayhead.playingForwards = true
                        }
                        viewportPlayhead.playing = !viewportPlayhead.playing
                    }
                }

                XsViewerTimeline {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }

    }

}
