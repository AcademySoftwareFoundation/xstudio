
import QtQuick 2.15
import QuickFuture 1.0
import QuickPromise 1.0

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.clipboard 1.0
import xStudio 1.0

Item {


    /**************************************************************

    COMMON FUNCTIONS

    ****************************************************************/
    function newSession() {
        studio.newSession("New Session")
        studio.clearImageCache()
    }

    function modSaveWarning(incoming) {
        if (sessionPath != undefined) {
            return incoming.replace("This Session has been modified", "This Session (" + sessionPath + ") has been modified")
        }
        return incoming
    }

    function newSessionWithCheck() {

        if (theSessionData.modified) {

            dialogHelpers.multiChoiceDialog(
                newSessionWithCheckCallback,
                "New Session",
                modSaveWarning("This Session has been modified. Would you like to save it before loading a new one?"),
                ["Cancel", "Don't Save", "Save"],
                undefined)
        } else {
            newSession()
        }

    }

    function newSessionWithCheckCallback(response, chaser) {
        if (response == "Save") {
            saveSessionCheck(newSession)
        } else if (response == "Don't Save") {
            newSession()
        }
    }

    function exportSequencePath(chaserFunc=undefined) {
        dialogHelpers.showFileDialog(
            function(fileUrl, undefined, func) {
                exportSequence(fileUrl, "otio", func)
            },
            defaultSessionFolder(),
            "Export Sequence",
            "otio",
            ["OTIO (*.otio)"],
            false,
            false,
            chaserFunc
        )
    }

    function exportSequence(path, type, chaserFunc) {
        if (path == false) return; // save was cancelled

        Future.promise(theSessionData.exportOTIO(sessionSelectionModel.selectedIndexes[0], path, type)).then(function(result){
            if (chaserFunc != undefined) chaserFunc(result)
            },
            function (err) {
                dialogHelpers.errorDialogFunc("Export Sequence Failed", err)
            }
        )
    }

    function saveSelectionAs(path, folder, chaserFunc) {

        if (path == false) return; // save was cancelled

        var spath = "" + path
        if (!(spath.endsWith(".xst") || spath.endsWith(".xsz"))) {
            path = path + ".xst"
        }

        Future.promise(theSessionData.saveFuture(path, sessionSelectionModel.selectedIndexes)).then(function(result){
            if (result == false) {
                // cancelled
                if (chaserFunc != undefined) chaserFunc()
            } else if (result != "") {
                dialogHelpers.errorDialogFunc("Save Session Failed", result)
            } else {
                newRecentPath(path)
                if (chaserFunc != undefined) chaserFunc()
            }
        })
    }

    function saveSessionAs(path, folder, chaserFunc) {

        if (path == false) return; // save was cancelled

        var spath = "" + path
        if (!(spath.endsWith(".xst") || spath.endsWith(".xsz"))) {
            path = path + ".xst"
        }

        Future.promise(theSessionData.saveFuture(path)).then(function(result) {
            if (result == false) {
                // cancelled
                if (chaserFunc != undefined) chaserFunc()
            } else if (result != "") {
                dialogHelpers.errorDialogFunc("Save Session Failed", result)
            } else {
                newRecentPath(path)
                if (chaserFunc != undefined) chaserFunc()
            }
        })
    }

    function saveOverwriteCallback(response, chaserFunc) {
        if (response == "Save As...") {
            saveSessionNewPath(chaserFunc)
        } else if (response == "Overwrite") {
            saveSessionAs(sessionPath, undefined, chaserFunc)
        }
    }

    function saveSessionCheck(chaserFunc) {
        if (sessionPath == undefined) {
            saveSessionNewPath(chaserFunc)
        } else if (sessionMTime.getTime() != helpers.getFileMTime(sessionPath).getTime()) {
            dialogHelpers.multiChoiceDialog(
                saveOverwriteCallback,
                "Session File Modified",
                "Session file \"" + sessionPath + "\" has been modified by something else, do you want to overwrite?",
                ["Cancel", "Overwrite", "Save As..."],
                ["Cancel", "Overwrite", "Save As..."],
                chaserFunc
                )
        } else {
            saveSessionAs(sessionPath, undefined, chaserFunc)
        }
    }

    function saveSessionNewPath(chaserFunc) {

        dialogHelpers.showFileDialog(
            saveSessionAs,
            defaultSessionFolder(),
            "Save Session",
            sessionCompression ? "xsz" : "xst",
            ["xStudio (*.xst *.xsz)"],
            false,
            false,
            chaserFunc
            )

    }

    function saveSelectionNewPath(chaserFunc) {

        dialogHelpers.showFileDialog(
            saveSelectionAs,
            defaultSessionFolder(),
            "Save Session",
            sessionCompression ? "xsz" : "xst",
            ["xStudio (*.xst *.xsz)"],
            false,
            false,
            chaserFunc
            )
    }

    function doLoadSession(path) {

        if (path == false) return; // load was cancelled

        Future.promise(studio.loadSessionFuture(path)).then(function(result) {
            if (result != true) {
                dialogHelpers.errorDialogFunc("Load Session Failed", result)
            } else {
                newRecentPath(path)
            }})
    }

    function doImportSession(path) {

        if (path == false) return; // load was cancelled

        Future.promise(theSessionData.importFuture(path, null)).then(
            function(result) {
                newRecentPath(path)
            })
    }

    function doImportOTIO(path) {

        if (path == false) return; // import was cancelled in file dialog

        let index=null

        if(index == null) {
            // create new playlist..
            index = theSessionData.createPlaylist("Imported OTIO")
        }

        Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": path}, index)).then(
            function(quuids){}
        )
    }

    function loadSession() {

        dialogHelpers.showFileDialog(
            doLoadSession,
            defaultSessionFolder(),
            "Open Session",
            "xst",
            ["xStudio (*.xst *.xsz)"],
            true,
            false)

    }

    property var file_to_load

    function loadSessionPath() {
        doLoadSession(file_to_load)
    }

    function loadSessionPathWithCheckCallback(result) {
        if (result == "Don't Save") {
            loadSessionPath()
        } else if (result == "Save") {
            saveSessionCheck(loadSessionPath)
        }
    }

    function loadSessionFile(session_file_path, file_name) {
        if (theSessionData.modified) {
            file_to_load = session_file_path
            dialogHelpers.multiChoiceDialog(
                loadSessionPathWithCheckCallback,
                "Load Snapshot Session",
                modSaveWarning("This Session has been modified. Would you like to save it before loading " + file_name + " ?"),
                ["Cancel", "Don't Save", "Save"],
                undefined)
        } else {
            doLoadSession(session_file_path)
        }
    }

    function loadSessionWithCheck() {

        if (theSessionData.modified) {
            dialogHelpers.multiChoiceDialog(
                loadSessionWithCheckCallback,
                "Load Session",
                modSaveWarning("This Session has been modified. Would you like to save it before loading a new one?"),
                ["Cancel", "Don't Save", "Save"],
                undefined)
        } else {
            loadSession()
        }

    }

    function loadSessionWithCheckCallback(result) {
        if (result == "Don't Save") {
            loadSession()
        } else if (result == "Save") {
            saveSessionCheck(loadSession)
        }
    }

    function loadMedia(media_urls) {

        dialogHelpers.showFileDialog(
            loadMediaCallback,
            defaultMediaFolder,
            "Select Media Files",
            undefined,
            ["Media files ("+helpers.validMediaExtensions()+")", "All files (*)" ],
            true,
            true)

    }

    function loadMediaCallback(media_urls, folder) {

        if (media_urls === false) return

        let uris = ""
        media_urls.forEach(function (item, index) {
            uris = uris + String(item) +"\n"
        })

        var index = currentMediaContainerIndex

        if(!index.valid) {
            // create new playlist..
            index = theSessionData.createPlaylist("New Playlist")
        }

        callbackTimer.setTimeout(function(capture) { return function(){
            Future.promise(
                capture.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": uris}, capture)
            ).then(function(quuids){
                mediaSelectionModel.selectFirstNewMedia(index, quuids)
            }) }}(index), 100
        );

        defaultMediaFolder = folder
    }

    function addMediaFromClipboard() {
        if(clipboard.text.length) {
            let ct = ""
            clipboard.text.split("\n").forEach(function (item, index) {
                    // replace #'s
                    // item.replace(/[#]+/, "*")
                    ct = ct + "file://" + item + "\n"
                }
            )
            if(!sessionSelectionModel.currentIndex.valid) {
                var index = theSessionData.createPlaylist("Add Media")
                Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": ct}, index)).then(function(quuids){
                    mediaSelectionModel.selectFirstNewMedia(index, quuids)
                })
            }
            else {
                let index = sessionSelectionModel.currentIndex
                Future.promise(index.model.handleDropFuture(Qt.CopyAction, {"text/uri-list": ct}, index)).then(function(quuids){
                    mediaSelectionModel.selectFirstNewMedia(index, quuids)
                })
            }
        }
    }

    function doQuit() {
        Qt.quit()
    }

    function quitWithCheckCallback(response) {
        if (response == "Quit Without Saving") {
            doQuit()
        } else if (response == "Save and Quit") {
            saveSessionCheck(doQuit)
        }
    }

    function quitWithCheck() {

        if (theSessionData.modified) {
            dialogHelpers.multiChoiceDialog(
                quitWithCheckCallback,
                "Quit xSTUDIO",
                modSaveWarning("This Session has been modified. Would you like to save it before closing xSTUDIO?"),
                ["Cancel", "Quit Without Saving", "Save and Quit"],
                undefined)
        } else {
            doQuit()
        }

    }

    function importSession() {
        dialogHelpers.showFileDialog(
            doImportSession,
            defaultSessionFolder(),
            "Import Session",
            "xst",
            ["xStudio (*.xst *.xsz)"],
            true,
            false)
    }

    function importOTIO() {
        dialogHelpers.showFileDialog(
            doImportOTIO,
            defaultSessionFolder(),
            "Import OTIO",
            "",
            ["All files (*)"],
            true,
            false)
    }

    function exportNotedToCSV() {

        dialogHelpers.showFileDialog(
            function(path) {
                if (path == false) return
                Future.promise(
                    bookmarkModel.exportCSVFuture(path)
                ).then(function(result) {
                    dialogHelpers.errorDialogFunc("Export Notes to CSV", result)
                })
            },
            defaultSessionFolder(),
            "Export Notes to CSV",
            "csv",
            ["CSV (*.csv)"],
            false,
            false)

    }


    function defaultSessionFolder() {
        // pick most recent path
        if(recentFiles.length && typeof recentFiles[0] == "string") {
            let p = recentFiles[0]
            return p.substr(0, p.lastIndexOf("/"))
        }
        return null
    }

    function newRecentPath(path) {
        let old = recentFiles
        if(old == undefined || !old.length){
            old = Array()
        }
        if(old.length) {
            // remove duplicate
            old = old.filter(function(value, index, arr){
                return value != path;
            });
        }
        // add to top
        old.unshift(path)

        //prune old entries
        if(old.length > 10)
            old.pop(old.length - 10)

        recentFiles = old

    }

    XsPreference {
        id: sessionLinkPrefix
        path: "/core/session/session_link_prefix"
    }

    function doCopySessionLink() {
        clipboard.text = sessionLinkPrefix.value + "xstudio://open_session?uri=" + sessionPath
    }

    function copySessionLink() {

        if(theSessionData.modified) {
            saveSessionCheck(doCopySessionLink)
        } else {
            doCopySessionLink()
        }

    }

    // Make a connection to the 'recent_history' preference (i.e. recent files)
    XsModelProperty {
        id: recent_files
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/recent_history", "pathRole")
    }
    property alias recentFiles: recent_files.value

    XsModelProperty {
        id: default_media_folder
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/default_media_folder", "pathRole")
    }
    property alias defaultMediaFolder: default_media_folder.value

    XsModelProperty {
        id: session_compression
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/core/session/compression", "pathRole")
    }
    property alias sessionCompression: session_compression.value

    property var sessionMTime: sessionProperties.values.mtimeRole
    property var sessionPathNative: sessionPath ? helpers.pathFromURL(sessionPath) : ""

    Clipboard {
        id: clipboard
    }

}

