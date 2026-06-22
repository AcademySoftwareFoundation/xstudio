# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Sam Richards

from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, FrameList, add_media_atom, Uuid, event_atom
from xstudio.core import KeyboardModifier
import os
from enum import Enum
import sys
import json
import threading
import time
import subprocess
import shutil
import pathlib
import atexit
import traceback
class HostApp(Enum):
    generic = 1
    xstudio = 2
    rv = 3
    rpa = 4

HOST_APP = HostApp.xstudio

# Try importing fileseq
try:
    import fileseq
    fileseq_available = True
except ImportError:
    fileseq_available = False
    _dbg("Warning: fileseq module not found. Sequence detection will be disabled.")

# File-based debug log (more reliable than _dbg in xStudio's embedded Python)
_DEBUG_LOG = "/{}/xstudio_thumb_debug.txt".format(os.environ.get("TMPDIR", "/tmp").rstrip("/"))
def _dbg(*msg):
    return
    try:
        with open(_DEBUG_LOG, "a") as _f:
            _f.write(f"{msg}\n")
            _f.flush()
    except Exception:
        pass

def get_windows_drives():
    """Return a list of available drive letters on Windows."""
    if sys.platform != "win32":
        return []
    drives = []
    from ctypes import windll
    bitmask = windll.kernel32.GetLogicalDrives()
    for letter in range(26):
        if bitmask & (1 << letter):
            drives.append(f"{chr(65 + letter)}:/")
    return drives

# PySide6 dependency removed
# from PySide6.QtCore import QObject, Signal, Qt
# from PySide6.QtWidgets import QApplication, QFileDialog

# MainThreadExecutor removed. 
# xstudio attributes .set_value() is generally thread-safe (posts to actor).
# For GUI dialogs, we need another approach or they are disabled without PySide.


class XStudioHostInterface:
    """
    Concrete implementation of the host application interface for xStudio.
    Handles loading, previewing, and comparing media, as well as playlist management.
    To port to OpenRV or another app, create a new host interface mapping to these methods.
    """
    def __init__(self, connection, plugin):
        """
        Initializes the xStudio host interface.

        Args:
            connection (RemoteConnection): The xStudio remote API connection object.
            plugin (PluginBase): The parent plugin instance, which holds shared state 
                                 or helper functions if needed.
        """
        self.connection = connection
        self.plugin = plugin
        self.preview_playlist = None
        self.previously_viewed_container = None

    def _resolve_active_playlist(self):
        """
        Attempts to find an active (on-screen or selected) playlist that isn't the Preview playlist.

        Returns:
            Playlist | None: The active playlist container, or None if no valid playlist is found.
        """
        return self.connection.api.session.inspected_container

    @staticmethod
    def _format_sequence_path(path):
        """
        Converts a fileseq path string into the specific URI formatting xStudio demands 
        for loading image sequences (e.g. `/dir/prefix{:04d}.ext=1001-1050`).

        Args:
            path (str): The raw file path or fileseq string.
        
        Returns:
            str | None: The formatted sequence path, or None if fileseq is unavailable or it's a single file.
        """
        if not fileseq_available: return None
        try:
            seq = fileseq.FileSequence(path)
            if len(seq) <= 1: return None
            pad_str = seq.padding()
            if pad_str and pad_str.startswith("%"):
                import re
                m = re.search(r"%(0(\d+))?d", pad_str)
                pad_len = int(m.group(2)) if m and m.group(2) else 0
            elif pad_str:
                pad_len = pad_str.count('#') * 4 + pad_str.count('@')
            else:
                pad_len = 0
            brace_padding = f"{{:0{pad_len}d}}" if pad_len > 0 else "{:d}"
            return f"{seq.dirname()}{seq.basename()}{brace_padding}{seq.extension()}={seq.frameRange()}"
        except Exception: return None

    def _add_media_to_playlist(self, playlist, path, before_uuid=Uuid()):
        """
        Adds a file or sequence to the given playlist. Formats the path as a sequence if applicable.

        Args:
            playlist (Playlist): The target xStudio playlist object.
            path (str): The filesystem path to load.

        Returns:
            Media | None: The newly added media object, or None if it fails.
        """
        try:
            seq_path = self._format_sequence_path(path)
            return playlist.add_media(seq_path if seq_path else path, before_uuid=before_uuid)
        except Exception as e:
            import traceback
            _dbg (traceback.format_exc())
            _dbg(f"Add media error: {e}")
            return None

    def _find_container_uuid(self, tree, target_value_uuid):
        """
        Recursively searches the playlist tree to find the internal tree-node UUID 
        corresponding to a known actor value UUID.

        Args:
            tree (TreeNode): The root node of the playlist tree sequence.
            target_value_uuid (Uuid): The value_uuid to search for.

        Returns:
            Uuid | None: The tree node UUID, or None if not found.
        """
        if hasattr(tree, 'value_uuid') and str(tree.value_uuid) == str(target_value_uuid):
            return tree.uuid
        if hasattr(tree, 'children'):
            for child in tree.children:
                res = self._find_container_uuid(child, target_value_uuid)
                if res: return res
        return None

    def load_media(self, path):
        """
        Loads the specified media into xStudio, creating a new playlist if necessary, or attaching it 
        to the currently active one. Handles sequence detection, local state caching, and playhead setup.
        Will not add the file if it detects a duplicate already inside the playlist.

        Args:
            path (str): The path to the file or sequence to load.
        """
        try:
            target_playlist = self.connection.api.session.inspected_container
        except Exception:
            target_playlist = None
        if not target_playlist:
            _, target_playlist = self.connection.api.session.create_playlist("File System Imports")
            self.connection.api.session.inspected_container = target_playlist
            

        try:
            is_timeline = target_playlist.type == "Timeline"
        except Exception:
            is_timeline = False

        for m in target_playlist.media:
            try:
                if m.metadata.get("/fs_browser/import_path") == path:
                    # already loaded this media into the playlist
                    return
            except:
                pass

        add_path = (self._format_sequence_path(path) if fileseq_available else None) or path

        if is_timeline:
            media = target_playlist.parent_playlist.add_media(add_path)
            target_playlist.add_media(media)
        else:
            media = target_playlist.add_media(add_path)

        self.connection.api.session.viewed_container = target_playlist
        target_playlist.playhead_selection.set_selection([media.uuid])
        target_playlist.playhead.playing = True

    def compare_with_current_media(self, paths):
        """
        Adds the specified media to the current selection, and puts the playhead into A/B compare mode.

        Args:
            paths (list): The new media file paths to compare.
        """
        try:
            playlist = self._resolve_active_playlist()
            if not playlist: return
            self.connection.api.session.set_on_screen_source(playlist)
            new_media = []
            for path in paths:
                m = self._add_media_to_playlist(playlist, path)
                if m:
                    new_media.append(m)
            if not new_media: return
            new_selection = []
            if hasattr(playlist, 'playhead_selection'):
                for m in playlist.playhead_selection.selected_sources:
                    new_selection.append(m.uuid)
            for m in new_media:
                new_selection.append(m.uuid)
            if hasattr(playlist, 'playhead_selection'):
                playlist.playhead_selection.set_selection(new_selection)
            if hasattr(playlist, 'playhead'):
                playlist.playhead.compare_mode = "A/B"
                playlist.playhead.playing = True
        except Exception as e: _dbg(f"Compare error: {e}")

    def replace_current_media(self, paths):
        """
        Replaces the currently selected/playing media in the active playlist with the new source(s).

        Args:
            paths (list): The new media file paths to insert into the playlist in place of the old one.
        """
        try:
            playlist = self._resolve_active_playlist()
            if not playlist: return
            self.connection.api.session.set_on_screen_source(playlist)
            new_media = []
            items_to_remove = []
            if hasattr(playlist, 'playhead_selection'):
                current_selection = playlist.playhead_selection.selected_sources
                if current_selection: items_to_remove = current_selection

            if hasattr(playlist, 'playhead_selection'):
                new_selection = [m.uuid for m in new_media]
                playlist.playhead_selection.set_selection(new_selection)

            for path in paths:
                m = self._add_media_to_playlist(playlist, path, before_uuid=items_to_remove[0].uuid if items_to_remove else Uuid())
                if m:
                    new_media.append(m)
            if not new_media: return

            if items_to_remove:
                for m in items_to_remove:
                    try: playlist.remove_media(m)
                    except Exception: pass

            if hasattr(playlist, 'playhead'):
                playlist.playhead.playing = True

        except Exception as e: _dbg(f"Replace error: {e}")

    def append_media(self, paths):
        """
        Appends a piece of media to the currently active playlist without altering 
        the playhead's current playback mode or selection.

        Args:
            paths (list): The media file paths to append.
        """
        try:
            playlist = self._resolve_active_playlist()
            if not playlist: return
            for path in paths:
                self._add_media_to_playlist(playlist, path)
        except Exception as e: _dbg(f"Append error: {e}")

    def add_to_new_playlist(self, paths):
        """
        Add a piece of media to a new playlist without altering 
        the playhead's current playback mode or selection.

        Args:
            paths (list): The media file paths to append.
        """
        try:
            target_playlist = self.connection.api.session.create_playlist("")[1]
            for path in paths:
                self._add_media_to_playlist(target_playlist, path)
        except Exception as e: _dbg(f"Append error: {e}")

    def preview_media(self, path):
        """
        Creates or utilizes a hidden 'Preview' playlist to temporarily view an item. 

        Args:
            path (str): The media file path to preview.
        """

        if self.connection.api.session.viewed_container != self.preview_playlist:
            self.previously_viewed_container = self.connection.api.session.viewed_container

        if not self.preview_playlist:
            self.preview_playlist = self.connection.api.session.create_hidden_playlist("File Browser Preview")

        self.preview_playlist.clear()
        media = self._add_media_to_playlist(self.preview_playlist, path)
        if not media: return

        self.plugin.preview_media_uuid.set_value(str(media.uuid))

        self.connection.api.session.viewed_container = self.preview_playlist
        self.preview_playlist.playhead_selection.set_selection([media.uuid])
        self.preview_playlist.playhead.playing = True

    def stop_preview(self):
        """
        Stops the preview by clearing the 'Preview' playlist and resetting the viewed container.
        """
        if self.connection.api.session.viewed_container.uuid == self.preview_playlist.uuid and self.previously_viewed_container:
            self.connection.api.session.viewed_container = self.previously_viewed_container
            self.plugin.preview_media_uuid.set_value("")
            self.preview_playlist.clear()

class FilesystemBrowserPlugin(PluginBase):
    def __init__(self, connection):
        PluginBase.__init__(
            self,
            connection,
            "File Browser"
        )
        # Initialize the host application interface (xStudio concrete implementation)
        self.host = XStudioHostInterface(self.connection, self)

        # Load Configuration
        self.config = self.load_config()

        if HOST_APP == HostApp.generic:
            # instanciate object that manages ffmpeg subprocess and threads for
            # generation of thumbnail images
            from .ffmpeg_thumbnails import FFMpegThumbnails
            self.thumbnail_generator = FFMpegThumbnails(self._update_file_thumbnail, _dbg)
        
        # self.main_executor = MainThreadExecutor()

        # Attribute to communicate list of files to QML (as JSON string)
        self.files_attr = self.add_attribute(
            "file_list",
            "", # empty string - sets file type as string
            {"title": "file_list"}, # Explicit title for QML lookup
            register_as_preference=False
        )
        self.files_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # Attribute for current path
        self.current_path_attr = self.add_attribute(
            "current_path",
            os.getcwd(),
            {"title": "current_path"},
            register_as_preference=True
        )
        self.current_path_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # Attribute for commands from QML
        self.command_attr = self.add_attribute(
            "command_channel",
            "",
            {"title": "command_channel"},
            register_as_preference=False
        )
        self.command_attr.expose_in_ui_attrs_group("Filesystem Browser")
        
        self.preview_media_uuid = self.add_attribute(
            "preview_media_uuid",
            "",
            {"title": "preview_media_uuid"},
            register_as_preference=False
        )
        self.preview_media_uuid.expose_in_ui_attrs_group("Filesystem Browser")

        # Action to toggle the panel
        self.toggle_action_uuid = "2669e4a3-7186-4556-9818-80949437b018"

        self.toggle_browser_action = self.register_hotkey(
            self.toggle_browser, # hotkey_callback
            "B", # default_keycode
            KeyboardModifier.MetaModifier,
            "Show Filesystem Browser",
            "Toggles the Filesystem Browser panel",
            False, # auto_repeat
            "FilesystemBrowser", # component
            "Window" # context
        )
        
        # Register the panel, passing the action
        self.register_ui_panel_qml(
            "File Browser",
            """
            import FileSystemBrowser 1.0
            FileSystemBrowser {
                anchors.fill: parent
            }
            """,
            10.0, # Position in menu
            "qrc:/icons/folder.svg", # No icon = Standard Panel (Docked)
            5.0,
            self.toggle_browser_action # Pass the action UUID
        )
        
        # New: Completion attribute
        self.completions_attr = self.add_attribute(
            "completions_attribute",
            "[]",
            {"title": "completions_attr"},
            register_as_preference=False
        )
        self.completions_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # New: Search state attribute
        self.searching_attr = self.add_attribute(
            "searching",
            False,
            {"title": "searching"},
            register_as_preference=False
        )
        self.searching_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # New: Progress attribute
        self.progress_attr = self.add_attribute(
            "scan_progress",
            "0",
            {"title": "scan_progress"},
            register_as_preference=False
        )
        self.progress_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # New: Progress attribute
        self.scanned_attr = self.add_attribute(
            "scanned_count",
            "0",
            {"title": "scanned_count"},
            register_as_preference=False
        )
        self.scanned_attr.expose_in_ui_attrs_group("Filesystem Browser")
        
        # New: Scanned directories list
        self.scanned_dirs_attr = self.add_attribute(
            "scanned_dirs",
            "[]",
            {"title": "scanned_dirs"},
            register_as_preference=False
        )
        self.scanned_dirs_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # New: Directory Query Result (for Tree View)
        self.directory_query_result = self.add_attribute(
            "directory_query_result",
            "{}",
            {"title": "directory_query_result"},
            register_as_preference=False
        )
        self.directory_query_result.expose_in_ui_attrs_group("Filesystem Browser")

        self.depth_limit_attr = self.add_attribute(
            "recursion_limit",
            self.config.get("max_recursion_depth", 6),
            {"title": "Recursion Limit", "category": "Filesystem Browser"},
            register_as_preference=True
        )
        self.depth_limit_attr.expose_in_ui_attrs_group("Filesystem Browser Settings")

        # stores the view mode (list, tree, groups, thumbs)
        self.view_mode_attr = self.add_attribute(
            "view_mode",
            3,
            {},
            register_as_preference=True
        )
        self.view_mode_attr.expose_in_ui_attrs_group("Filesystem Browser")

        self.directory_tree_width = self.add_attribute(
            "directory_tree_width",
            300,
            {},
            register_as_preference=True
        )
        self.directory_tree_width.expose_in_ui_attrs_group("Filesystem Browser Settings")

        self.directory_tree_visible = self.add_attribute(
            "directory_tree_visible",
            True,
            {},
            register_as_preference=True
        )
        self.directory_tree_visible.expose_in_ui_attrs_group("Filesystem Browser Settings")

        # New: Scan Required flag (for manual scan mode)
        self.scan_required_attr = self.add_attribute(
            "scan_required",
            False,
            {"title": "scan_required"},
            register_as_preference=False
        )
        self.scan_required_attr.expose_in_ui_attrs_group("Filesystem Browser")

        self.is_deepscan_attr = self.add_attribute(
            "is_deepscan",
            False
        )
        self.is_deepscan_attr.expose_in_ui_attrs_group("Filesystem Browser")

        self.auto_scan_threshold_attr = self.add_attribute(
            "auto_scan_threshold",
            self.config.get("auto_scan_threshold", 4),
            {"title": "auto_scan_threshold", "category": "Filesystem Browser"},
            register_as_preference=True
        )
        self.auto_scan_threshold_attr.expose_in_ui_attrs_group("Filesystem Browser Settings")
        
        # New: Filter attributes
        self.filter_time_attr = self.add_attribute(
            "Mod. Time",
            "Any", 
            {"combo_box_options": ["Any", "Last 1 day", "Last 2 days", "Last 1 week", "Last 1 month"]},
            register_as_preference=True
        )
        self.filter_time_attr.expose_in_ui_attrs_group(["Filesystem Browser", "Filter Terms"])
        
        self.filter_version_attr = self.add_attribute(
            "Version",
            "All Versions",
            {"title": "Version", "combo_box_options": ["All Versions", "Latest Version", "Latest 2 Versions"]},
            register_as_preference=True
        )
        self.filter_version_attr.expose_in_ui_attrs_group(["Filesystem Browser", "Filter Terms"])

        self.sort_role = self.add_attribute(
            "Sort Role",
            "name",
            {},
            register_as_preference=True
        )
        self.sort_role.expose_in_ui_attrs_group(["Filesystem Browser Settings"])

        self.sort_ascending_attr = self.add_attribute(
            "Sort Ascending",
            True,
            {},
            register_as_preference=True
        )
        self.sort_ascending_attr.expose_in_ui_attrs_group(["Filesystem Browser Settings"])

        # History and Pinned Attributes
        self.history_attr = self.add_attribute(
            "history_paths",
            "[]",
            {"title": "history_paths"},  # Must match QML attributeTitle
            register_as_preference=True
        )
        self.history_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # Default pinned items
        default_pins = []
        
        # 1. Environment Variable Pre-defines (JSON list of dicts or paths)
        env_pins = os.environ.get("XSTUDIO_BROWSER_PINS")
        if env_pins:
            try:
                # Try parsing as JSON first
                parsed = json.loads(env_pins)
                if isinstance(parsed, list):
                    for item in parsed:
                        if isinstance(item, dict) and "path" in item:
                            default_pins.append(item)
                        elif isinstance(item, str):
                            default_pins.append({"name": os.path.basename(item), "path": item})
            except:
                # Fallback to standard path separator (colon on Unix, semicolon on Win)
                # We also normalize semicolons to os.pathsep to be lenient
                normalized = env_pins
                if os.pathsep == ":":
                    normalized = env_pins.replace(";", ":")
                
                paths = normalized.split(os.pathsep)
                for p in paths:
                    p = p.strip()
                    if p:
                        default_pins.append({"name": os.path.basename(p), "path": p})

        # 2. Standard Defaults
        home = os.environ.get("HOME")
        if home:
            # Avoid duplicates
            if not any(p["path"] == home for p in default_pins):
                default_pins.append({"name": "Home", "path": home})
            
            downloads = os.path.join(home, "Downloads")
            if os.path.exists(downloads):
                if not any(p["path"] == downloads for p in default_pins):
                    default_pins.append({"name": "Downloads", "path": downloads})
        
        self.pinned_attr = self.add_attribute(
            "pinned_paths",
            json.dumps(default_pins),
            {"title": "pinned_paths"},  # Must match QML attributeTitle
            register_as_preference=True
        )
        self.pinned_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # ENFORCE: Merge default_pins (env vars + explicit defaults) into the actual attribute value
        try:
            current_val = self.pinned_attr.value()
            
            current_pins = []
            if current_val:
                try:
                    current_pins = json.loads(current_val)
                except Exception:
                    current_pins = []
            
            # Merge
            changed = False
            existing_paths = set(p["path"] for p in current_pins)
            
            for pin in reversed(default_pins): 
                if pin["path"] not in existing_paths:
                    current_pins.insert(0, pin)
                    existing_paths.add(pin["path"])
                    changed = True
            
            if changed or not current_val:
                new_val = json.dumps(current_pins)
                self.pinned_attr.set_value(new_val)
                
        except Exception as e:
            _dbg(f"FilesystemBrowser: Error merging pins: {e}")

        # Connect listeners
        # Note: We need to register callbacks properly.
        # attribute_changed method handles all.
        
        # Configuration preferences with fallbacks from config.json
        self.extensions_attr = self.add_attribute(
            "Media Extensions",
            ", ".join(self.config.get("extensions", [])),
            {"title": "Media Extensions", "category": "Filesystem Browser"},
            register_as_preference=True
        )
        self.extensions_attr.expose_in_ui_attrs_group("Filesystem Browser Settings")

        self.ignore_dirs_attr = self.add_attribute(
            "Ignore Directories",
            ", ".join(self.config.get("ignore_dirs", [])),
            {"title": "Ignore Directories", "category": "Filesystem Browser"},
            register_as_preference=True
        )
        self.ignore_dirs_attr.expose_in_ui_attrs_group("Filesystem Browser Settings")

        self.root_ignore_dirs_attr = self.add_attribute(
            "Root Ignore Directories",
            ", ".join(self.config.get("root_ignore_dirs", [])),
            {"title": "Root Ignore Directories", "category": "Filesystem Browser"},
            register_as_preference=True
        )
        self.root_ignore_dirs_attr.expose_in_ui_attrs_group("Filesystem Browser Settings")
        self.search_thread = None
        self.cancel_search = False
        self.results_lock = threading.Lock()  # Protects current_scan_results
        self.current_scan_results = []
        
        # Dedicated attribute for batch thumbnail requests from QML.
        # QML writes a JSON array of paths; Python reads and queues them all at once.
        self.thumbnail_request_attr = self.add_attribute(
            "thumbnail_request",
            "[]",
            {"title": "thumbnail_request"},
            register_as_preference=False
        )
        self.thumbnail_request_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # Build the QML command dispatch table
        self._command_handlers = self._build_command_handlers()

        self.results_backend_receiver = None
        for plugin_detail in self.connection.api.plugin_manager.plugin_detail:
            if plugin_detail.name() == "Scan Results Middleman":
                self.results_backend_receiver = connection.api.plugin_manager.get_plugin_instance(plugin_detail.uuid())

        # Initial search
        self.start_search(self.current_path_attr.value())

    @property
    def extensions(self):
        val = self.extensions_attr.value()
        if not val: return []
        return [item.strip() for item in val.split(',') if item.strip()]

    @property
    def ignore_dirs(self):
        val = self.ignore_dirs_attr.value()
        if not val: return set()
        return set(item.strip() for item in val.split(',') if item.strip())

    @property
    def root_ignore_dirs(self):
        val = self.root_ignore_dirs_attr.value()
        if not val: return set()
        return set(item.strip() for item in val.split(',') if item.strip())
        
    def toggle_browser(self, converting, context):
        _dbg(f"Toggling Filesystem Browser (Action Triggered). Context: {context}")
        # We can also verify visibility here if possible, but the Model handles it.


    def _open_browser_dialog(self, initial_path):
        """Runs on main thread to show dialog."""
        try:
            from PySide6.QtWidgets import QFileDialog
            dir_path = QFileDialog.getExistingDirectory(None, "Select Directory", initial_path)
            if dir_path:
                self.current_path_attr.set_value(dir_path)
                self.start_search(dir_path)
        except ImportError:
            _dbg("PySide6 not available. Directory dialog disabled.")
        except Exception as e:
            _dbg(f"Error opening dialog: {e}")


    def _build_command_handlers(self):
        """Build the QML command dispatch table. Called once from __init__."""
        def _cmd_change_path(data):
            new_path = data.get("path")
            if os.path.exists(new_path) and os.path.isdir(new_path):
                self.current_path_attr.set_value(new_path)
                self._add_to_history(new_path)
                self.start_search(new_path)
            else:
                _dbg(f"Invalid path: {new_path}")

        def _cmd_set_attribute(data):
            attr_name = data.get("name")
            attr_value = data.get("value")
            if attr_name == "Mod. Time":
                self.filter_time_attr.set_value(attr_value)
            elif attr_name == "Version":
                self.filter_version_attr.set_value(attr_value)
            elif attr_name == "recursion_limit":
                self.depth_limit_attr.set_value(attr_value)

        def _cmd_copy_path(data):
            path = data.get("path")
            if not path:
                return
            try:
                if sys.platform == "darwin":
                    subprocess.run(["pbcopy"], input=path.encode(), check=True)
                elif sys.platform == "win32":
                    subprocess.run(["clip"], input=path.encode(), check=True)
                else:
                    subprocess.run(["xclip", "-selection", "clipboard"],
                                   input=path.encode(), check=True)
            except Exception as e:
                _dbg(f"copy_path: Error: {e}")

        def _cmd_reveal_in_finder(data):
            path = data.get("path")
            if not path:
                return
            # Resolve sequence to a concrete first frame
            if fileseq_available:
                try:
                    seq = fileseq.FileSequence(path)
                    if seq:
                        path = str(seq[0])
                except Exception:
                    pass
            try:
                if sys.platform == "darwin":
                    subprocess.run(["open", "-R", path], check=True)
                elif sys.platform == "win32":
                    subprocess.run(["explorer", "/select,", os.path.normpath(path)], check=True)
                else:
                    subprocess.run(["open", os.path.dirname(path)], check=True)
            except Exception as e:
                _dbg(f"reveal_in_finder: Error: {e}")

        def _cmd_go_to_parent(data):
            from pathlib import Path
            path = Path(self.current_path_attr.value())
            self.current_path_attr.set_value(str(path.parent.absolute()))
            self.start_search(self.current_path_attr.value())

        def _cmd_stop_scan(data):
            self.cancel_search = True
            if hasattr(self, 'scanner'):
                self.scanner.stop()
            if self.search_thread and self.search_thread.is_alive():
                self.search_thread.join()
            self.searching_attr.set_value(False)
            self.progress_attr.set_value("0")
            with self.results_lock:
                self.current_scan_results = []
            self.scanned_attr.set_value("0")
            self.scanned_dirs_attr.set_value("[]")

        def _cmd_force_scan(data):
            path = data.get("path")
            if path:
                self.current_path_attr.set_value(path)
                self._add_to_history(path)
                self.start_search(path, force=True, depth=20)
            else:
                self.start_search(self.current_path_attr.value(), force=True, depth=20)

        def _scan_single(data):
            path = data.get("path")
            if path and os.path.exists(path):
                self.start_search(path, force=True, depth=1, refresh=False)

        def _load_paths(data):
            paths = data.get("paths", [])
            for p in paths:
                self.load_file(p)

        return {
            "change_path":              _cmd_change_path,
            "load_file":                lambda d: self.load_file(d.get("path")),
            "load_files":               lambda d: [self.load_file(p) for p in d.get("paths", [])],
            "preview_file":             lambda d: self.host.preview_media(d.get("path")),
            "request_browser":          lambda d: self._open_browser_dialog(self.current_path_attr.value()),
            "complete_path":            lambda d: self.compute_completions(d.get("path", "")),
            "replace_current_media":    lambda d: self.host.replace_current_media(d.get("path")),
            "compare_with_current_media": lambda d: self.host.compare_with_current_media(d.get("paths")),
            "append_media":             lambda d: self.host.append_media(d.get("paths")),
            "replace_current_media":    lambda d: self.host.replace_current_media(d.get("paths")),
            "add_to_new_playlist":      lambda d: self.host.add_to_new_playlist(d.get("paths")),
            "set_attribute":            _cmd_set_attribute,
            "copy_path":                _cmd_copy_path,
            "reveal_in_finder":         _cmd_reveal_in_finder,
            "add_pin":                  lambda d: self._add_pin(d.get("name"), d.get("path")),
            "remove_pin":               lambda d: self._remove_pin(d.get("path")),
            "force_scan":               _cmd_force_scan,
            "stop_scan":                _cmd_stop_scan,
            "stop_preview":             lambda d: self.host.stop_preview(),
            "go_to_parent_folder":      _cmd_go_to_parent,
            "get_subdirs":              lambda d: self._get_subdirs(d.get("path")),
            "request_thumbnail":        lambda d: self.thumbnail_generator(d.get("path")),
            "scan_single":              _scan_single
        }

    def attribute_changed(self, attribute, role):
        from xstudio.core import AttributeRole

        if attribute.uuid == self.command_attr.uuid and role == AttributeRole.Value:
            try:
                val = self.command_attr.value()
            except TypeError:
                return
            if not val:
                return
            try:
                data = json.loads(val)
                action = data.get("action")
                handler = self._command_handlers.get(action)
                if handler:
                    handler(data)
                elif action:
                    _dbg(f"FilesystemBrowser: Unknown command action: {action!r}")
                # Clear command channel
                self.command_attr.set_value("")
            except Exception as e:
                _dbg(f"Command error: {e}")
                import traceback
                traceback.print_exc()
                
        elif attribute.uuid in (self.filter_time_attr.uuid, self.filter_version_attr.uuid):
            pass
        elif attribute.uuid == self.depth_limit_attr.uuid:
            if role == AttributeRole.Value:
                # Recursion limit changed, re-scan
                current = self.current_path_attr.value()
                self.start_search(current)
        elif attribute.uuid == self.thumbnail_request_attr.uuid and role == AttributeRole.Value:
            # QML has written a JSON array of paths to request thumbnails for.
            # Handle here on the plugin's message thread, then clear the attribute.
            try:
                val = attribute.value()
                if val and val not in ("", "[]"):
                    paths = json.loads(val)
                    _dbg(f"BATCH: received {len(paths)} paths")
                    for p in paths:
                        self.thumbnail_generator.request_thumbnail(p)
                    self.thumbnail_request_attr.set_value("[]")
            except Exception as e:
                import traceback
                _dbg(f"BATCH ERROR: {e}\n{traceback.format_exc()}")

    def _manual_scan_required(self):
        """Called when a path is selected that is shallow enough to require 
        manual confirmation before scanning."""
        self.scan_required_attr.set_value(True)
        self.searching_attr.set_value(False)
        self.progress_attr.set_value("0")
        
        with self.results_lock:
            self.current_scan_results = []
        self.scanned_attr.set_value("0") 
        self.scanned_dirs_attr.set_value("[]")
                
        if self.search_thread and self.search_thread.is_alive():
            self.cancel_search = True
            if hasattr(self, 'scanner'):
                self.scanner.stop()
            self.search_thread.join()

    def start_search(self, start_path, force=False, depth=None, refresh=True):
        """
        Start the file search in a separate thread.
        If force=False and depth <= 4, skip auto-scan and ask user to confirm.
        """
        if not start_path:
            return

        # Reset message to backend receiver
        if refresh:
            self.connection.send(self.results_backend_receiver, event_atom(), True)

        # Check path depth
        norm_path = os.path.normpath(start_path)
        parts = norm_path.strip(os.sep).split(os.sep)
        
        if HOST_APP == HostApp.xstudio:
            if not force:
                depth = 0
            if depth == None:
                # if no depth is set, we only scan the current folder
                depth = 0
        else:
            p_depth = len([p for p in parts if p])        
            threshold = self.config.get("auto_scan_threshold", 4)
            if not force and p_depth <= threshold:
                self._manual_scan_required()
                return

        self.is_deepscan_attr.set_value(depth != 0)

        self.scan_required_attr.set_value(False)

        if self.search_thread and self.search_thread.is_alive():
            self.cancel_search = True
            if hasattr(self, 'scanner'):
                self.scanner.stop()
                self.scanner.shutdown()
            self.search_thread.join()
        
        self.cancel_search = False
        self.searching_attr.set_value(True)
        self.search_thread = threading.Thread(target=self._search_worker, args=(start_path, depth))
        self.search_thread.daemon = True
        self.search_thread.start()

    def _search_worker(self, start_path, custom_depth=None):
        _dbg(f"Starting search in {start_path} (depth={custom_depth if custom_depth is not None else 'default'})")
        
        from .scanner import FileScanner
        
        self.cached_filter_time = self.filter_time_attr.value()
        self.cached_filter_version = self.filter_version_attr.value()
        
        max_depth = custom_depth if custom_depth is not None else self.depth_limit_attr.value()
        config = {
            "extensions": list(self.extensions),
            "ignore_dirs": list(self.ignore_dirs),
            "max_depth": max_depth
        }
        
        self.scanner = FileScanner(config)
        with self.results_lock:
            self.current_scan_results = []
        self.pending_scan_results = []
        self.scanned_dirs_cache = []
        self.scanned_dirs_attr.set_value("[]")
        self.last_update = 0
        self.last_progress_update = 0
        
        def progress_callback(results, scanned_dir, index_path_in_tree, info):

            import json


            if scanned_dir != "":
                if self.results_backend_receiver:
                    self.connection.send(self.results_backend_receiver, event_atom(), scanned_dir, index_path_in_tree==[], json.dumps(results))

            scanned = info.get("scanned", 0)
            phase = info.get("phase", "")
            progress = info.get("progress", 0)
            new_dirs = info.get("scanned_dirs", [])
            
            now_p = time.time()
            if now_p - self.last_progress_update >= 0.5:
                self.last_progress_update = now_p
                biased_progress = pow(progress / 100.0, 2.0)*100
                self.progress_attr.set_value(str(biased_progress))
                self.scanned_attr.set_value(str(scanned))
            
            if new_dirs:
                self.scanned_dirs_cache.extend(new_dirs)
                self.scanned_dirs_attr.set_value(json.dumps(self.scanned_dirs_cache))

            if results and phase == "scanning":
                self.pending_scan_results.extend(results)
                now = time.time()
                if now - self.last_update > 5:
                    self.last_update = now
                    with self.results_lock:
                        self.current_scan_results.extend(self.pending_scan_results)
                    self.pending_scan_results = []

            if phase == "complete":
                self.searching_attr.set_value(False)
            
        try:
            results = self.scanner.scan(start_path, callback=progress_callback)
            
            if self.cancel_search:
                return

            with self.results_lock:
                self.current_scan_results = results        
            
            _dbg(f"Search finished, found {len(results)} items")
            
        except Exception as e:
            _dbg(f"Search error: {e}")
            import traceback
            traceback.print_exc()
        finally:
            self.searching_attr.set_value(False)

    def compute_completions(self, partial_path):
        """Minimal logic to find subdirectories matching partial path."""
        try:
            # If empty, do nothing
            if not partial_path:
                self.completions_attr.set_value("[]")
                return

            # Determine directory to scan
            # Handle absolute paths vs relative correctly
            if partial_path.endswith(os.path.sep):
                directory = partial_path
                base = ""
            else:
                directory = os.path.dirname(partial_path)
                base = os.path.basename(partial_path)

            # If directory part is empty (e.g. user typed "home")
            if not directory:
                directory = "."

            if not os.path.exists(directory) or not os.path.isdir(directory):
                self.completions_attr.set_value("[]")
                return

            candidates = []
            try:
                with os.scandir(directory) as it:
                    for entry in it:
                        if entry.name in self.ignore_dirs or entry.name.startswith('.'):
                            continue

                        if entry.is_dir():
                            # Filter by base case-insensitive
                            if entry.name.lower().startswith(base.lower()):
                                candidates.append(entry.path + os.path.sep)
            except OSError:
                pass

            # Sort and limit
            candidates.sort()
            self.completions_attr.set_value(json.dumps(candidates[:20]))

        except Exception as e:
            _dbg(f"Completion error: {e}")
            self.completions_attr.set_value("[]")

    def _update_file_thumbnail(self, path, thumb_uri):
        """Update thumbnailSource in current_scan_results and push to files_attr
        WITHOUT calling apply_filters() to avoid a full QML model rebuild."""
        with self.results_lock:
            found = False
            for r in self.current_scan_results:
                if r.get("path") == path:
                    if r.get("thumbnailSource") == thumb_uri:
                        return  # Already up to date; don't trigger another rebuild
                    r["thumbnailSource"] = thumb_uri
                    found = True
                    break
            if not found:
                return
            # Serialise only what QML needs — same JSON format as apply_filters
            serialised = json.dumps(self.current_scan_results)

        # Push the update; QML will merge thumbnailSource via the Image.source binding
        self.files_attr.set_value(serialised)

    def load_config(self):
        """Load configuration from config.json in the plugin directory."""
        config_path = os.path.join(os.path.dirname(__file__), "config.json")
        default_config = {
            "extensions": [".mov", ".mp4", ".mkv", ".exr", ".jpg", ".jpeg", ".png", 
                           ".dpx", ".tiff", ".tif", ".wav", ".mp3"],
            "ignore_dirs": [".git", ".quarantine", "eryx_unreal_plugin", ".DS_Store"],
            "root_ignore_dirs": [],
            "max_recursion_depth": 6,
            "auto_scan_threshold": 4
        }
        
        if os.path.exists(config_path):
            try:
                with open(config_path, 'r') as f:
                    loaded_config = json.load(f)
                    # Merge with defaults
                    for key, value in loaded_config.items():
                        default_config[key] = value
                    _dbg(f"FilesystemBrowser: Loaded config from {config_path}")
            except Exception as e:
                _dbg(f"FilesystemBrowser: Error loading config: {e}")
        
        return default_config

    def _get_subdirs(self, path):
        """Fetch subdirectories for the given path and update attribute."""
        result = {"path": path, "dirs": []}
        try:
            if path == "/" and sys.platform == "win32":
                drivenames = get_windows_drives()
                result["dirs"] = [{"name": d, "path": d, "has_subdir": True} for d in drivenames]
            elif os.path.exists(path) and os.path.isdir(path):
                dirs = []
                with os.scandir(path) as it:
                    for entry in it:
                        # Check ignore dirs (names)
                        if entry.name in self.ignore_dirs or entry.name.startswith('.'):
                            continue
                        
                        # Check root ignore dirs (paths)
                        if entry.path in self.root_ignore_dirs:
                            continue
                            
                        if entry.is_dir():
                            # Does this hit performance of the tree? 
                            has_subdir = False
                            try:
                                with os.scandir(entry.path) as sub_it:
                                    for sub_entry in sub_it:
                                        if sub_entry.is_dir():
                                            has_subdir = True
                                            break
                            except Exception:
                                pass
                            dirs.append({
                                "name": entry.name,
                                "path": entry.path,
                                "has_subdir": has_subdir
                            })
                # Sort alphabetically
                dirs.sort(key=lambda x: x["name"].lower())
                result["dirs"] = dirs
        except Exception as e:
            _dbg(f"Error getting subdirs for {path}: {e}")
        
        import time
        result["timestamp"] = time.time()
        
        # Ensure we use JSON dumping
        import json
        self.directory_query_result.set_value(json.dumps(result))

    def load_file(self, path):
        """Handle directory navigation or delegating file loading to host interface."""
        if os.path.isdir(path):
            self.current_path_attr.set_value(path)
            self._add_to_history(path)
            self.start_search(path)
            return
        self.host.load_media(path)

    def _add_to_history(self, path):
        try:
            current_history = json.loads(self.history_attr.value())
        except:
            current_history = []
        
        # Remove if exists to bubble to top
        try:
            current_history.remove(path)
        except ValueError:
            pass
            
        current_history.insert(0, path)
        # Limit history
        if len(current_history) > 20:
            current_history = current_history[:20]
            
        self.history_attr.set_value(json.dumps(current_history))

    def _add_pin(self, name, path):
        try:
            pins = json.loads(self.pinned_attr.value())
        except:
            pins = []
            
        # Check if already pinned
        for p in pins:
            if p["path"] == path:
                 return # Already pinned
        
        pins.append({"name": name, "path": path})
        self.pinned_attr.set_value(json.dumps(pins))

    def _remove_pin(self, path):
        try:
            pins = json.loads(self.pinned_attr.value())
        except:
            pins = []
        
        new_pins = [p for p in pins if p["path"] != path]
        if len(new_pins) != len(pins):
            self.pinned_attr.set_value(json.dumps(new_pins))

    def _cleanup(self):
        """atexit handler: shut down scanner thread pool and remove temp thumbnail dir."""
        if hasattr(self, 'scanner'):
            try:
                self.scanner.stop()
                self.scanner.shutdown()
            except Exception:
                pass

def create_plugin_instance(connection):
    return FilesystemBrowserPlugin(connection)
