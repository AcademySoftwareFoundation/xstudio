// SPDX-License-Identifier: Apache-2.0
//
// Import / Export dialog for the panel layouts in the layout bar at the top
// right of the main window (New / Rename / Duplicate Layout). One layout
// carries its whole panel arrangement: splitters, divider positions, tab
// groups, and which view sits in each tab.
//
// One component serves both flows, chosen by the ``mode`` property that the
// plugin sets in the QML snippet ("export" or "import"). The item is created
// once and only hidden when closed, so everything is reset in start() each
// time the window is shown.
//
// This dialog does all of the access to the layouts model, Python only does
// the file IO and the validation of the file (see layout_io.py for why). A
// locally declared XsPanelsLayoutModel registers as another client of the
// same backend model, so what is written here is broadcast to the session
// window and saved to the preferences in the usual way.
//
// export: show the layouts read from the model -> save dialog ->
//         write_layouts() in Python.
// import: open dialog -> read_layouts() in Python validates and flags clashes
//         and unknown panels -> list the layouts -> Import applies them to
//         the model.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import "."

XsWindow {

    id: dialog
    width: 560
    height: 520
    minimumWidth: 420
    minimumHeight: 320

    flags: Qt.platform.os === "windows" ? Qt.Window : Qt.Dialog

    // "export" or "import", set by the plugin's QML snippet.
    property string mode: "export"
    readonly property bool isExport: mode === "export"

    title: isExport ? "Export Layouts" : "Import Layouts"

    property int checkedCount: 0
    property int duplicateCount: 0
    property int missingCount: 0
    property string versionWarning: ""
    // Full layout nodes, index-aligned with the "key" role of each row.
    property var sourceNodes: []

    // Further clients of the shared "panels model" and "view widgets" models.
    XsPanelsLayoutModel { id: layoutsModel }
    XsViewsModel { id: registeredViews }

    ListModel { id: rowModel }

    // ---- model reading -----------------------------------------------------

    function childrenOf(node) {
        if (!node) return []
        if (Array.isArray(node)) return node
        if (node.children && Array.isArray(node.children)) return node.children
        return []
    }

    // The layouts are the children of the "main_window" node. The same
    // top-level array also holds the geometry of floating windows, which are
    // not layouts.
    function mainWindowNode() {
        var windows = childrenOf(layoutsModel.modelData)
        for (var i = 0; i < windows.length; i++) {
            if (windows[i] && windows[i].window_name === "main_window") return windows[i]
        }
        return null
    }

    function existingLayoutNames() {
        var names = []
        var nodes = childrenOf(mainWindowNode())
        for (var i = 0; i < nodes.length; i++) {
            if (nodes[i] && nodes[i].layout_name) names.push(nodes[i].layout_name)
        }
        return names
    }

    function registeredViewNames() {
        var names = []
        var nodes = childrenOf(registeredViews.modelData)
        for (var i = 0; i < nodes.length; i++) {
            var name = nodes[i] ? nodes[i].view_name : ""
            if (name && name !== "divider" && names.indexOf(name) === -1) names.push(name)
        }
        return names
    }

    function collectViews(node, out) {
        if (!node) return
        if (typeof node.tab_view === "string" && node.tab_view.length && out.indexOf(node.tab_view) === -1) {
            out.push(node.tab_view)
        }
        var kids = childrenOf(node)
        for (var i = 0; i < kids.length; i++) collectViews(kids[i], out)
    }

    function describeLayout(node) {
        var views = []
        collectViews(node, views)
        return views.join(", ")
    }

    // ---- row helpers -------------------------------------------------------

    function recount() {
        var n = 0
        for (var i = 0; i < rowRepeater.count; i++) {
            var it = rowRepeater.itemAt(i)
            if (it && it.rowChecked) n++
        }
        checkedCount = n
    }

    function setAllChecked(v) {
        for (var i = 0; i < rowRepeater.count; i++) {
            var it = rowRepeater.itemAt(i)
            if (it) it.rowChecked = v
        }
        recount()
    }

    function selectedNodes() {
        var out = []
        for (var i = 0; i < rowRepeater.count; i++) {
            var it = rowRepeater.itemAt(i)
            if (!it || !it.rowChecked) continue
            var node = sourceNodes[parseInt(it.rowKey)]
            if (node) out.push(JSON.parse(JSON.stringify(node)))
        }
        return out
    }

    // ---- start -------------------------------------------------------------

    function start() {
        rowModel.clear()
        sourceNodes = []
        duplicateCount = 0
        missingCount = 0
        versionWarning = ""
        Qt.callLater(recount)

        if (isExport) {
            populateExport()
        } else {
            chooseImportFile()
        }
    }

    onVisibleChanged: {
        if (visible) start()
    }

    // ---- export ------------------------------------------------------------

    function populateExport() {
        var nodes = childrenOf(mainWindowNode())
        sourceNodes = nodes
        for (var i = 0; i < nodes.length; i++) {
            var node = nodes[i]
            if (!node || !node.layout_name) continue
            rowModel.append({
                key: String(i),
                name: node.layout_name,
                detail: describeLayout(node),
                duplicate: false,
                missing: ""
            })
        }
        Qt.callLater(recount)
    }

    function chooseExportFile() {
        dialogHelpers.showFileDialog(
            doExport,
            "",
            "Export Layouts",
            "json",
            ["JSON (*.json)"],
            false,
            false)
    }

    function doExport(path) {

        if (path === false) return // cancelled

        var py_args = {
            "url": "" + path,
            "layouts": selectedNodes()
        }

        Future.promise(
            helpers.pythonAsyncCallback("LayoutIO", "write_layouts", py_args)
        ).then(function(return_val) {

            if (Array.isArray(return_val) && return_val[0] === true) {
                dialog.hide()
            } else {
                dialogHelpers.errorDialogFunc("Export Layouts", "" + return_val)
            }

        },
        function(err) {
            dialogHelpers.errorDialogFunc("Export Layouts", "" + err)
        })
    }

    // ---- import ------------------------------------------------------------

    function chooseImportFile() {
        dialogHelpers.showFileDialog(
            doReadImportFile,
            "",
            "Import Layouts",
            "json",
            ["JSON (*.json)", "All files (*)"],
            true,
            false)
    }

    function doReadImportFile(path) {

        // Cancelled: the window stays up so "Choose file..." can try again.
        if (path === false) return

        var py_args = {
            "url": "" + path,
            "existing_names": existingLayoutNames(),
            "view_names": registeredViewNames()
        }

        Future.promise(
            helpers.pythonAsyncCallback("LayoutIO", "read_layouts", py_args)
        ).then(function(return_val) {
            showImportRows(return_val)
        },
        function(err) {
            dialogHelpers.errorDialogFunc("Import Layouts", "" + err)
        })
    }

    function showImportRows(raw) {

        var result
        try { result = JSON.parse(raw) } catch (e) { result = undefined }

        if (!result) {
            dialogHelpers.errorDialogFunc("Import Layouts", "The file could not be read.")
            return
        }
        if (!result.ok) {
            dialogHelpers.errorDialogFunc("Import Layouts", result.error)
            return
        }

        sourceNodes = result.layouts || []
        versionWarning = result.version_warning || ""

        rowModel.clear()
        var dups = 0
        var missing = 0
        var rows = result.rows || []
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i]
            rowModel.append({
                key: row.key,
                name: row.name,
                detail: describeLayout(sourceNodes[parseInt(row.key)]),
                duplicate: row.duplicate === true,
                missing: row.missing || ""
            })
            if (row.duplicate === true) dups++
            if (row.missing) missing++
        }
        duplicateCount = dups
        missingCount = missing
        Qt.callLater(recount)
    }

    // Names that clash get a suffix: nothing enforces unique layout names, and
    // two tabs with the same label cannot be told apart.
    function uniqueName(name, takenLower) {
        var base = (name || "Layout").trim()
        if (takenLower.indexOf(base.toLowerCase()) === -1) return base
        for (var n = 2; n < 1000; n++) {
            var candidate = base + " (" + n + ")"
            if (takenLower.indexOf(candidate.toLowerCase()) === -1) return candidate
        }
        return base + " (" + Date.now() + ")"
    }

    function applyImport() {

        var taken = existingLayoutNames().map(function (s) { return s.toLowerCase() })

        var nodes = selectedNodes()
        for (var i = 0; i < nodes.length; i++) {

            var copy = nodes[i]
            copy.layout_name = uniqueName(copy.layout_name, taken)
            copy.enabled = true
            taken.push(copy.layout_name.toLowerCase())

            // Look the window row up again each time, the rows below it change.
            var winIdx = layoutsModel.searchRecursive("main_window", "window_name")
            if (!winIdx.valid) {
                console.log("LayoutIO: main_window node not found, aborting import")
                break
            }

            // Same as PanelsModel::duplicate_layout: append an empty row then
            // replace it with the whole node. Setting "jsonRole" replaces the
            // node rather than merging into it, so each layout must be a
            // complete node.
            var rc = layoutsModel.rowCount(winIdx)
            layoutsModel.insertRowsSync(rc, 1, winIdx)
            layoutsModel.set(layoutsModel.index(rc, 0, winIdx), JSON.stringify(copy), "jsonRole")
        }
    }

    // ---- layout ------------------------------------------------------------

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        XsText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignLeft
            font.bold: true
            text: isExport ? "Select layouts to export" : "Select layouts to import"
        }

        XsText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignLeft
            color: XsStyleSheet.hintColor
            wrapMode: Text.WordWrap
            visible: rowModel.count > 0 && text !== ""
            text: {
                if (isExport) return "Each layout carries its whole panel arrangement."
                var parts = []
                if (duplicateCount > 0)
                    parts.push(duplicateCount + " name(s) already in use, those are imported with a suffix.")
                if (missingCount > 0)
                    parts.push(missingCount + " layout(s) use panels that are not available here, those tabs will be blank.")
                if (versionWarning) parts.push(versionWarning)
                return parts.join("\n")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: rowModel.count > 0
            spacing: 8

            XsPrimaryButton {
                text: "Select all"
                Layout.preferredWidth: 90
                onClicked: setAllChecked(true)
            }
            XsPrimaryButton {
                text: "Select none"
                Layout.preferredWidth: 90
                onClicked: setAllChecked(false)
            }
            Item { Layout.fillWidth: true }
            XsText {
                color: XsStyleSheet.hintColor
                text: checkedCount + " / " + rowModel.count + " selected"
            }
        }

        // A Repeater rather than a ListView so that every delegate exists and
        // the checkboxes can be read back reliably. A layout list is short.
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: rowModel.count > 0
            clip: true
            contentWidth: width
            contentHeight: rowColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: XsScrollBar {
                visible: rowColumn.implicitHeight > flick.height
            }

            Column {
                id: rowColumn
                width: parent.width
                spacing: 6

                Repeater {
                    id: rowRepeater
                    model: rowModel

                    delegate: Column {
                        id: rowDelegate
                        width: rowColumn.width
                        spacing: 1

                        property alias rowChecked: rowCheck.checked
                        property string rowKey: model.key

                        RowLayout {
                            width: parent.width
                            spacing: 6

                            XsCheckBox {
                                id: rowCheck
                                checked: true
                                onToggled: recount()
                            }
                            XsText {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                                text: model.name
                            }
                            XsText {
                                visible: model.duplicate === true
                                color: XsStyleSheet.accentColor
                                font.bold: true
                                text: "name in use"
                            }
                        }

                        XsText {
                            visible: model.detail !== ""
                            width: parent.width
                            leftPadding: 26
                            horizontalAlignment: Text.AlignLeft
                            color: XsStyleSheet.hintColor
                            elide: Text.ElideRight
                            text: model.detail
                        }

                        XsText {
                            visible: model.missing !== ""
                            width: parent.width
                            leftPadding: 26
                            horizontalAlignment: Text.AlignLeft
                            color: XsStyleSheet.accentColor
                            wrapMode: Text.WordWrap
                            text: "missing here: " + model.missing
                        }
                    }
                }
            }
        }

        // Nothing listed yet.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: rowModel.count === 0
            spacing: 10

            Item { Layout.fillHeight: true }
            XsText {
                Layout.alignment: Qt.AlignHCenter
                color: XsStyleSheet.hintColor
                text: isExport ? "No layouts to export." : "Choose a file to import."
            }
            XsPrimaryButton {
                visible: !isExport
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                text: "Choose file..."
                onClicked: chooseImportFile()
            }
            Item { Layout.fillHeight: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            XsPrimaryButton {
                text: "Cancel"
                Layout.preferredWidth: 90
                onClicked: dialog.hide()
            }
            XsPrimaryButton {
                text: isExport ? "Export..." : "Import"
                Layout.preferredWidth: 90
                isUnClickable: checkedCount === 0
                onClicked: {
                    if (checkedCount === 0) return
                    if (isExport) {
                        chooseExportFile()
                    } else {
                        applyImport()
                        dialog.hide()
                    }
                }
            }
        }
    }
}
