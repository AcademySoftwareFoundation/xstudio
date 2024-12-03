// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15

import "."

Item {

     /*****************************************************************
     *
     * QuickViewer management
     *
     ****************************************************************/


    Connections {

        target: studio

        function onOpenQuickViewers(media_actors, compare_mode, in_frame, out_frame) {
            launchQuickViewer(media_actors, compare_mode, in_frame, out_frame)
        }

        function onSessionRequest(path, jsn) {
            // TODO: handle remote request to load a session - need a prompt to
            // check if user wants to overwrite the current session?
            Future.promise(studio.loadSessionFuture(path, jsn)).then(function(result){})
            file_functions.newRecentPath(path)
        }

        function onShowMessageBox(title, body, closeButton, timeoutSecs) {

            dialogHelpers.multiChoiceDialog(
                undefined,
                title,
                body,
                closeButton ? ["Close"] : [])
        
            // try again in 200 milliseconds
            callbackTimer.setTimeout(function() { return function() {
                dialogHelpers.hideLastDialog()
            }}(), timeoutSecs*1000);
                
        }

    }

    // QuickView window position management
    property var quickWinPosition: Qt.point(100, 100)
    property var quickWinSize: Qt.size(1280,720)
    property bool quickWinPosSet: false

    function closingQuickviewWindow(position, size) {
        // when a QuickView window is closed, remember its size and position and
        // re-use for next QuickView window
        quickWinPosition = position
        quickWinSize = size
        quickWinPosSet = true
    }

    function launchQuickViewer(sources, compare_mode, in_frame, out_frame) {
        launchQuickViewerWithSize(sources, compare_mode, in_frame, out_frame, quickWinPosition, quickWinSize)
        if (quickWinPosSet) {
            // rest the default position for the next QuickView window
            quickWinPosition = Qt.point(100, 100)
            quickWinPosSet = false
        } else {
            // each new window will be positioned 100 pixels to the bottom and
            // right of the previous one
            quickWinPosition = Qt.point(quickWinPosition.x + 100, quickWinPosition.y + 100)
        }
    }

    function launchQuickViewerWithSize(sources, compare_mode, in_frame, out_frame, __position, __size) {

        var component = Qt.createComponent("XsQuickViewWindow.qml");
        if (component.status == Component.Ready) {
            if (compare_mode == "Off" || compare_mode == "") {
                for (var source in sources) {
                    var quick_viewer = component.createObject(appWindow, {x: __position.x, y: __position.y, width: __size.width, height: __size.height});
                    quick_viewer.show()
                    quick_viewer.viewport.view.quickViewSource([sources[source]], "Off", in_frame, out_frame)
                    quick_viewer.raise()
                    quick_viewer.requestActivate()
                    quick_viewer.raise()
                }
            } else {
                var quick_viewer = component.createObject(appWindow, {x: __position.x, y: __position.y, width: __size.width, height: __size.height});
                quick_viewer.show()
                quick_viewer.viewport.view.quickViewSource(sources, compare_mode, in_frame, out_frame)
                quick_viewer.raise()
                quick_viewer.requestActivate()
                quick_viewer.raise()
        }
        } else {
            // Error Handling
            console.log("Error loading component:", component.errorString());
        }
    }
}