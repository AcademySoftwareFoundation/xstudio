#!/bin/env python
# SPDX-License-Identifier: Apache-2.0
"""Import / export of xSTUDIO panel layouts.

A "layout" is an entry in the layout bar at the top right of the main window:
the named tab you manage with New / Rename / Duplicate Layout. One layout is
the whole panel arrangement behind that tab - the splitters and their divider
positions, each panel's tab group, and the view sitting in every tab.

This plugin adds "Layouts ..." to the File > Import and File > Export menus.

The work is split between Python and QML:

* The layouts live in the "panels model" owned by the C++ GlobalUIModelData
  actor. The messages that change it are not bound to Python, so QML does all
  of the model access (a QML-side XsPanelsLayoutModel joins the same model as
  another client, and its edits are broadcast to the session window and saved
  to the preferences in the normal way).
* This module does the file IO and the validation of the file's contents.

File format::

    {
      "format": "xstudio_layouts",
      "version": 1,
      "exported_at": "2026-08-07T11:22:33",
      "source": {"xstudio": "1.4.0"},
      "layouts": [
        {"layout_name": "Review", "enabled": true, "children": [...]}
      ]
    }

Every entry must be a COMPLETE layout node: importing replaces the new row
with the node (it does not merge into it), so an entry without a
``layout_name``, ``enabled`` or ``children`` would give an unnamed, empty tab.
"""

import datetime
import json
import os
from urllib.parse import urlsplit
from urllib.request import url2pathname

from xstudio.plugin import PluginBase

# path to QML resources relative to this .py file
qml_folder_name = "qml/LayoutIO.1"

# The mode is baked into the QML snippet, so each menu entry gets its own
# dialog item. The item is created once and shown again on later menu clicks.
import_dialog_qml = """
LayoutIODialog {
    mode: "import"
}
"""

export_dialog_qml = """
LayoutIODialog {
    mode: "export"
}
"""

EXPORT_FORMAT = "xstudio_layouts"
EXPORT_VERSION = 1


def _xstudio_version():
    """The xSTUDIO version string, or "" if the version module is missing."""
    try:
        from xstudio.version import __version__

        return str(__version__)
    except Exception:
        return ""


def _path_from_url(url):
    """Local filesystem path for a ``file://`` URL as sent by a QML FileDialog.

    Handles percent-escapes (spaces etc.) and Windows drive letters.
    """
    return url2pathname(urlsplit(url).path)


def _collect_tab_views(node, out):
    """Gather every ``tab_view`` name referenced below a layout node."""
    if isinstance(node, dict):
        view = node.get("tab_view")
        if isinstance(view, str) and view:
            out.add(view)
        children = node.get("children")
        if isinstance(children, list):
            for child in children:
                _collect_tab_views(child, out)


def _is_layout_node(node):
    """A layout node needs a name and at least one panel below it."""
    if not isinstance(node, dict):
        return False
    name = node.get("layout_name")
    children = node.get("children")
    return (
        isinstance(name, str)
        and bool(name.strip())
        and isinstance(children, list)
        and bool(children)
    )


class LayoutIO(PluginBase):
    def __init__(self, connection):
        PluginBase.__init__(
            self, connection, name="LayoutIO", qml_folder=qml_folder_name
        )

        # The stock File > Import / File > Export submenus are created by
        # xSTUDIO's own menus, positions are relative to the items they hold.
        self.import_menu_id = self.insert_menu_item(
            "main menu bar",
            "Layouts ...",
            "File|Import",
            3.2,
            callback=self.import_menu_callback,
        )
        self.export_menu_id = self.insert_menu_item(
            "main menu bar",
            "Layouts ...",
            "File|Export",
            6.0,
            callback=self.export_menu_callback,
        )

        self.connect_to_ui()

    def import_menu_callback(self):
        self.create_qml_item(import_dialog_qml)

    def export_menu_callback(self):
        self.create_qml_item(export_dialog_qml)

    # The two methods below are called from the dialog's QML through
    # helpers.pythonAsyncCallback.

    def read_layouts(self, url, existing_names=None, view_names=None):
        """Validate a layout file and describe its contents for the dialog.

        existing_names: the layout names in use now, to flag name clashes.
        view_names: the names of the registered views, to flag panels that
            would show up blank here. Empty skips that check.

        Returns a JSON string, {"ok": true, ...} or {"ok": false, "error": ..}.
        """
        path = _path_from_url(url)
        try:
            with open(path, "r", encoding="utf-8") as handle:
                doc = json.load(handle)
        except (OSError, ValueError) as err:
            return json.dumps({"ok": False, "error": "Could not read the file:\n{}".format(err)})

        if (
            not isinstance(doc, dict)
            or doc.get("format") != EXPORT_FORMAT
            or not isinstance(doc.get("layouts"), list)
        ):
            return json.dumps(
                {"ok": False, "error": "The selected file is not an xSTUDIO layout export."}
            )

        taken = {n.strip().lower() for n in (existing_names or []) if isinstance(n, str)}
        known = {n for n in (view_names or []) if isinstance(n, str)}

        rows = []
        layouts = []
        for node in doc["layouts"]:
            if not _is_layout_node(node):
                continue
            views = set()
            _collect_tab_views(node, views)
            rows.append(
                {
                    "key": str(len(layouts)),
                    "name": node["layout_name"],
                    "duplicate": node["layout_name"].strip().lower() in taken,
                    # A panel that is not registered here renders as a blank
                    # tab without any message, so warn about it up front.
                    "missing": ", ".join(sorted(views - known)) if known else "",
                }
            )
            layouts.append(node)

        if not rows:
            return json.dumps({"ok": False, "error": "The file does not contain any usable layouts."})

        # The layout JSON is not versioned by xSTUDIO itself, so a different
        # version is worth a warning but not a refusal.
        theirs = (doc.get("source") or {}).get("xstudio") or ""
        ours = _xstudio_version()
        warning = ""
        if theirs and ours and theirs != ours:
            warning = (
                "Exported from xSTUDIO {} (this is {}), the layouts may not "
                "translate cleanly.".format(theirs, ours)
            )

        return json.dumps(
            {"ok": True, "rows": rows, "layouts": layouts, "version_warning": warning}
        )

    def write_layouts(self, url, layouts):
        """Write the selected layouts, already read out of the model by QML.

        Returns [True, message] on success, otherwise an error string.
        """
        entries = [node for node in (layouts or []) if _is_layout_node(node)]
        if not entries:
            return "There are no valid layouts to export."

        path = _path_from_url(url)
        if not path.lower().endswith(".json"):
            path += ".json"

        doc = {
            "format": EXPORT_FORMAT,
            "version": EXPORT_VERSION,
            "exported_at": datetime.datetime.now().isoformat(timespec="seconds"),
            "source": {"xstudio": _xstudio_version()},
            "layouts": entries,
        }

        # Write beside the target and rename, so a failed write cannot leave a
        # half-written file in place of an existing one.
        tmp_path = path + ".tmp"
        try:
            with open(tmp_path, "w", encoding="utf-8") as handle:
                json.dump(doc, handle, indent=2)
            os.replace(tmp_path, path)
        except OSError as err:
            try:
                os.remove(tmp_path)
            except OSError:
                pass
            return "Could not write the file:\n{}".format(err)

        return [True, "Exported {} layout(s) to {}".format(len(entries), os.path.basename(path))]


def create_plugin_instance(connection):
    return LayoutIO(connection)
