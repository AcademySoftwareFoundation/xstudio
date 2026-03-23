import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// This Item extends the pure 'Viewport' QQuickItem from the cpp side

Item {
    
    id: root

    property bool muted: true
    
    function loadMedia(path) {
        the_viewport.quickViewFromPath(path);
    }

    function playToggle() {
        viewportPlayhead.playing = !viewportPlayhead.playing
        if (muted) {
            viewportPlayhead.volume = 0
        } else {
            viewportPlayhead.volume = 100;
        }
    }

    onMutedChanged: {
        if (muted) {
            viewportPlayhead.volume = 0
        } else {
            viewportPlayhead.volume = 100;
        }
    }

    XsPlayhead {
        id: viewportPlayhead
        uuid: the_viewport.playheadUuid
    }

    ColumnLayout {

        anchors.fill: parent
        spacing: 0

        Viewport {
            id: the_viewport
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
                        playToggle()
                    }
                }

                XsViewerTimeline {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                XsSecondaryButton { 

                    Layout.preferredWidth: XsStyleSheet.widgetStdHeight
                    Layout.fillHeight: true
                    imgSrc: muted ? "qrc:/icons/volume_mute.svg" : "qrc:/icons/volume_up.svg"
                    onClicked: {
                        muted = !muted
                    }
                }


            }
        }

    }

}
