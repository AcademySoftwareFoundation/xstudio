# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Sam Richards

from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, FrameList, add_media_atom, Uuid
from xstudio.api.session.playlist.playlist import Playlist
from xstudio.api.session.playlist.timeline import Timeline
import os
import sys
import json
import threading
import queue
import time
import subprocess
import shutil
import pathlib
import tempfile
import uuid as _uuid
from datetime import datetime

# Try importing fileseq
try:
    import fileseq
    fileseq_available = True
except ImportError:
    fileseq_available = False
    print("Warning: fileseq module not found. Sequence detection will be disabled.")

# File-based debug log (more reliable than print in xStudio's embedded Python)
_DEBUG_LOG = os.path.join(tempfile.gettempdir(), "xstudio_thumb_debug.txt")
def _dbg(msg):
    try:
        with open(_DEBUG_LOG, "a") as _f:
            _f.write(f"{msg}\n")
            _f.flush()
    except Exception:
        pass


def _find_ffmpeg():
    """Find ffmpeg binary. Checks env var, xStudio app bundle, then system PATH."""
    # 1. Explicit override
    env_path = os.environ.get("FFMPEG_PATH")
    if env_path and os.path.isfile(env_path):
        return env_path, None  # (binary, dyld_lib_path)

    # 2. xStudio app bundle (same directory as the main binary)
    exe = sys.argv[0] if sys.argv else ""
    exe_dir = os.path.dirname(exe)
    ffmpeg_name = "ffmpeg.exe" if sys.platform == "win32" else "ffmpeg"
    bundle_ffmpeg = os.path.join(exe_dir, ffmpeg_name)
    if os.path.isfile(bundle_ffmpeg):
        # Bundled ffmpeg needs Frameworks dir on DYLD_LIBRARY_PATH (macOS only)
        frameworks = os.path.join(exe_dir, "..", "Frameworks")
        frameworks = os.path.normpath(frameworks)
        return bundle_ffmpeg, frameworks

    # 3. System PATH
    system_ffmpeg = shutil.which("ffmpeg")
    if system_ffmpeg:
        return system_ffmpeg, None

    return None, None


# PySide6 dependency removed
# from PySide6.QtCore import QObject, Signal, Qt
# from PySide6.QtWidgets import QApplication, QFileDialog

# MainThreadExecutor removed. 
# xstudio attributes .set_value() is generally thread-safe (posts to actor).
# For GUI dialogs, we need another approach or they are disabled without PySide.


class FilesystemBrowserPlugin(PluginBase):
    def __init__(self, connection):
        PluginBase.__init__(
            self,
            connection,
            "Filesystem Browser",
            qml_folder="qml/FilesystemBrowser.1"
        )
        # Load Configuration
        self.config = self.load_config()
        
        # self.main_executor = MainThreadExecutor()

        # Attribute to communicate list of files to QML (as JSON string)
        self.files_attr = self.add_attribute(
            "file_list",
            "[]", # Empty JSON list
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
        
        # Action to toggle the panel
        self.toggle_action_uuid = "2669e4a3-7186-4556-9818-80949437b018"
        
        self.toggle_browser_action = self.register_hotkey(
            self.toggle_browser, # hotkey_callback
            "B", # default_keycode
            0, # default_modifier
            "Show Filesystem Browser",
            "Toggles the Filesystem Browser panel",
            False, # auto_repeat
            "FilesystemBrowser", # component
            "Window" # context
        )
        
        # Menu item triggers this action
        # Removed manual callback to rely on hotkey_uuid linkage 
        # which should toggle the panel automatically if registered correctly.
        self.insert_menu_item(
            "main menu bar",
            "Filesystem Browser",
            "View|Panels",
            0.0,
            hotkey_uuid=self.toggle_browser_action,
            callback=self.toggle_browser_from_menu
        )

        # Add menu item to open as floating window
        self.insert_menu_item(
            "main menu bar",
            "Browser Open",
            "Plugins",
            0.1,
            callback=self.open_floating_browser
        )
        
        # Register the panel, passing the action
        self.register_ui_panel_qml(
            "Filesystem Browser",
            """
            FilesystemBrowser {
                anchors.fill: parent
            }
            """,
            10.0, # Position in menu
            "", # No icon = Standard Panel (Dockable)
            -1.0,
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
            self.config.get("max_recursion_depth", 0),
            {"title": "recursion_limit"},
            register_as_preference=True
        )
        self.depth_limit_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # New: Scan Required flag (for manual scan mode)
        self.scan_required_attr = self.add_attribute(
            "scan_required",
            False,
            {"title": "scan_required"},
            register_as_preference=False
        )
        self.scan_required_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # Auto-scan threshold (read-only for UI logic)
        self.auto_scan_threshold_attr = self.add_attribute(
            "auto_scan_threshold",
            self.config.get("auto_scan_threshold", 4),
            {"title": "auto_scan_threshold"},
            register_as_preference=False
        )
        self.auto_scan_threshold_attr.expose_in_ui_attrs_group("Filesystem Browser")
        
        # New: Filter attributes
        self.filter_time_attr = self.add_attribute(
            "filter_time",
            "Any", 
            {"title": "filter_time", "values": ["Any", "Last 1 day", "Last 2 days", "Last 1 week", "Last 1 month"]},
            register_as_preference=True
        )
        self.filter_time_attr.expose_in_ui_attrs_group("Filesystem Browser")
        
        self.filter_version_attr = self.add_attribute(
            "filter_version",
            "All Versions",
            {"title": "filter_version", "values": ["All Versions", "Latest Version", "Latest 2 Versions"]},
            register_as_preference=True
        )
        self.filter_version_attr.expose_in_ui_attrs_group("Filesystem Browser")

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
        home = os.path.expanduser("~")
        if home and home != "~":
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
            print(f"FilesystemBrowser: Error merging pins: {e}")

        # Connect listeners
        # Note: We need to register callbacks properly.
        # attribute_changed method handles all.
        
        # Internal state
        # Load extensions and ignore dirs from config
        self.extensions = set(self.config.get("extensions", []))
        self.ignore_dirs = set(self.config.get("ignore_dirs", []))
        self.root_ignore_dirs = set(os.path.normpath(p) for p in self.config.get("root_ignore_dirs", []))
        self.search_thread = None
        self.cancel_search = False
        self.results_lock = threading.Lock()  # Protects current_scan_results
        self.current_scan_results = []
        
        # Thumbnail setup — ffmpeg-based, no xStudio actor system needed
        self._ffmpeg_bin, self._ffmpeg_dyld = _find_ffmpeg()
        if self._ffmpeg_bin:
            print(f"FilesystemBrowser: using ffmpeg at {self._ffmpeg_bin}")
        else:
            print("FilesystemBrowser: WARNING — ffmpeg not found, thumbnails disabled")
        self._temp_dir = tempfile.mkdtemp(prefix="xstudio_thumbs_")
        self._thumbnail_cache = {}   # path -> file:///... thumb URI
        self._thumb_lock = threading.Lock()
        self._thumb_pending = set()  # paths currently in queue/processing
        self._thumb_queue = queue.Queue()
        # 4 worker threads — daemon so they die with the process
        for _ in range(4):
            t = threading.Thread(target=self._thumb_worker_loop, daemon=True)
            t.start()

        # State tracking for Preview Mode
        self.original_playlist_uuid = None
        self.preview_playlist_uuid = None

        # Dedicated attribute for batch thumbnail requests from QML.
        # QML writes a JSON array of paths; Python reads and queues them all at once.
        self.thumbnail_request_attr = self.add_attribute(
            "thumbnail_request",
            "[]",
            {"title": "thumbnail_request"},
            register_as_preference=False
        )
        self.thumbnail_request_attr.expose_in_ui_attrs_group("Filesystem Browser")

        # Initial search
        self.start_search(self.current_path_attr.value())
        
    def toggle_browser_from_menu(self, menu_item=None, user_data=None):
        # Wrapper for menu callback
        # Since we are now a standard dockable panel, the user should use View -> Panels -> Filesystem Browser
        # or rely on the hotkey's default action if it maps to the view.
        # We'll just log here.
        print("Menu item clicked. The Filesystem Browser is available in the Panels menu.")
        self.toggle_browser(None, "Menu Click")

    def open_floating_browser(self):
        # Create a floating window containing the FilesystemBrowser component
        qml = """
        import QtQuick.Window 2.15
        import QtQuick.Controls 2.15
        
        Window {
            width: 900
            height: 600
            visible: true
            title: "Filesystem Browser"
            
            FilesystemBrowser {
                anchors.fill: parent
            }
        }
        """
        self.create_qml_item(qml)

    def toggle_browser(self, converting, context):
        print(f"Toggling Filesystem Browser (Action Triggered). Context: {context}")
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
            print("PySide6 not available. Directory dialog disabled.")
        except Exception as e:
            print(f"Error opening dialog: {e}")


    def attribute_changed(self, attribute, role):
        # Handle commands from QML via the command attribute
        from xstudio.core import AttributeRole
        
        # Check if it's our command attribute and the Value changed
        if attribute.uuid == self.command_attr.uuid and role == AttributeRole.Value:
            # Safely get value
            try:
                val = self.command_attr.value()
            except TypeError:
                # Can happen if connection is shutting down or not ready
                return

            if not val:
                return # Empty command
                
            try:
                data = json.loads(val)
                action = data.get("action")
                _dbg(f"CMD: action={action} data={data}")

                if action == "change_path":
                    new_path = data.get("path", "").replace("\\", "/")
                    # Strip trailing slash unless it's a drive root like "X:/"
                    if len(new_path) > 1 and new_path.endswith("/") and not new_path.endswith(":/"):
                        new_path = new_path.rstrip("/")
                    print(f"FilesystemBrowser: change_path -> '{new_path}'")
                    # Virtual root "/" on Windows — just update the path, no scan
                    if new_path == "/" and sys.platform == "win32":
                        self.current_path_attr.set_value("/")
                        # Clear file list immediately so QML shows empty state
                        self.files_attr.set_value("[]")
                        return
                    if os.path.exists(new_path) and os.path.isdir(new_path):
                         self.current_path_attr.set_value(new_path)
                         # Clear file list immediately so user sees feedback
                         self.files_attr.set_value("[]")
                         self._add_to_history(new_path)
                         self.start_search(new_path)
                    else:
                         print(f"FilesystemBrowser: Invalid path: {new_path}")

                elif action == "load_file":
                    file_path = data.get("path")
                    self.load_file(file_path)

                elif action == "load_files":
                    paths = data.get("paths", [])
                    if paths:
                        self._load_multiple_files(paths)

                elif action == "compare_items":
                    paths = data.get("paths", [])
                    if len(paths) >= 2:
                        self._compare_items(paths)

                elif action == "add_to_timeline":
                    paths = data.get("paths", [])
                    if paths:
                        self._add_to_timeline(paths)

                elif action == "preview_file":
                    file_path = data.get("path")
                    self._preview_file(file_path)
                    
                elif action == "request_browser":
                    # Open native directory dialog
                    current = self.current_path_attr.value()
                    # Execute directly (will fail gracefully if PySide6 missing)
                    self._open_browser_dialog(current)
                        
                elif action == "complete_path":
                    partial = data.get("path", "")
                    self.compute_completions(partial)

                elif action == "replace_current_media":
                    path = data.get("path")
                    self._replace_current_media(path)

                elif action == "compare_with_current_media":
                    path = data.get("path")
                    self._compare_with_current_media(path)

                elif action == "set_attribute":
                    attr_name = data.get("name")
                    attr_value = data.get("value")
                    if attr_name == "filter_time":
                        self.filter_time_attr.set_value(attr_value)
                    elif attr_name == "filter_version":
                        self.filter_version_attr.set_value(attr_value)
                    elif attr_name == "recursion_limit":
                        self.depth_limit_attr.set_value(attr_value)

                elif action == "add_pin":
                    path = data.get("path")
                    name = data.get("name")
                    if not name and path:
                        # Derive name from path: use last directory component
                        stripped = path.rstrip("/\\")
                        name = stripped.rsplit("/", 1)[-1].rsplit("\\", 1)[-1] or path
                    self._add_pin(name, path)

                elif action == "remove_pin":
                    path = data.get("path")
                    self._remove_pin(path)

                elif action == "force_scan":
                    # User clicked "Scan" button
                    path = data.get("path")
                    if path:
                        # Ensure we update the attribute (and thus the QML path field)
                        self.current_path_attr.set_value(path)
                        self._add_to_history(path)
                        # Use deep recursion for manual scan (e.g., 20)
                        self.start_search(path, force=True, depth=20)
                    else:
                        # Fallback for the main Refresh button
                        current = self.current_path_attr.value()
                        self.start_search(current, force=True, depth=20)

                elif action == "get_subdirs":
                    path = data.get("path")
                    self._get_subdirs(path)

                elif action == "request_thumbnail":
                    path = data.get("path")
                    self._request_thumbnail(path)
                
                # Clear command channel
                self.command_attr.set_value("")
                
            except Exception as e:
                print(f"Command error: {e}")
                import traceback
                traceback.print_exc()
                
        elif attribute.uuid in (self.filter_time_attr.uuid, self.filter_version_attr.uuid):
            if role == AttributeRole.Value:
                self._on_filter_changed(attribute, role)
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
                        self._request_thumbnail(p)
                    self.thumbnail_request_attr.set_value("[]")
            except Exception as e:
                import traceback
                _dbg(f"BATCH ERROR: {e}\n{traceback.format_exc()}")

    def start_search(self, start_path, force=False, depth=None):
        """
        Start the file search in a separate thread.
        If force=False and depth <= 4, skip auto-scan and ask user to confirm.
        """
        if not start_path:
            return

        self.scan_required_attr.set_value(False)

        if self.search_thread and self.search_thread.is_alive():
            self.cancel_search = True
            if hasattr(self, 'scanner'):
                self.scanner.stop()
            self.search_thread.join()
        
        self.cancel_search = False
        self.searching_attr.set_value(True)
        self.search_thread = threading.Thread(target=self._search_worker, args=(start_path, depth))
        self.search_thread.daemon = True
        self.search_thread.start()

    def _search_worker(self, start_path, custom_depth=None):
        print(f"Starting search in {start_path} (depth={custom_depth if custom_depth is not None else 'default'})")
        
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
        
        def progress_callback(results, info):
            scanned = info.get("scanned", 0)
            phase = info.get("phase", "")
            progress = info.get("progress", 0)
            new_dirs = info.get("scanned_dirs", [])
            
            biased_progress = pow(progress / 100.0, 2.0)*100
            self.progress_attr.set_value(str(biased_progress))
            self.scanned_attr.set_value(str(scanned))
            
            if new_dirs:
                self.scanned_dirs_cache.extend(new_dirs)
                import json
                self.scanned_dirs_attr.set_value(json.dumps(self.scanned_dirs_cache))

            if results and phase == "scanning":
                self.pending_scan_results.extend(results)
                now = time.time()
                if now - self.last_update > 5:
                    self.last_update = now
                    with self.results_lock:
                        self.current_scan_results.extend(self.pending_scan_results)
                    self.apply_filters()
                    self.pending_scan_results = []

            if phase == "complete":
                self.searching_attr.set_value(False)
            
        try:
            results = self.scanner.scan(start_path, callback=progress_callback)
            
            if self.cancel_search:
                return

            with self.results_lock:
                self.current_scan_results = results
            self.apply_filters()
            
            print(f"Search finished, found {len(results)} items")
            
        except Exception as e:
            print(f"Search error: {e}")
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
            print(f"Completion error: {e}")
            self.completions_attr.set_value("[]")



    def load_config(self):
        """Load configuration from config.json in the plugin directory."""
        config_path = os.path.join(os.path.dirname(__file__), "config.json")
        default_config = {
            "extensions": [".mov", ".mp4", ".mkv", ".exr", ".jpg", ".jpeg", ".png", 
                           ".dpx", ".tiff", ".tif", ".wav", ".mp3"],
            "ignore_dirs": [".git", ".quarantine", "eryx_unreal_plugin", ".DS_Store"],
            "root_ignore_dirs": [],
            "max_recursion_depth": 0,
            "auto_scan_threshold": 4
        }
        
        if os.path.exists(config_path):
            try:
                with open(config_path, 'r') as f:
                    loaded_config = json.load(f)
                    # Merge with defaults
                    for key, value in loaded_config.items():
                        default_config[key] = value
                    print(f"FilesystemBrowser: Loaded config from {config_path}")
            except Exception as e:
                print(f"FilesystemBrowser: Error loading config: {e}")
        
        return default_config

    def _get_subdirs(self, path):
        """Fetch subdirectories for the given path and update attribute."""
        _dbg(f"_get_subdirs called for path='{path}' (type={type(path).__name__})")
        print(f"FilesystemBrowser: _get_subdirs called for {path}")
        result = {"path": path, "dirs": []}

        # On Windows, the virtual root "/" should list available drive letters
        if sys.platform == "win32" and (path == "/" or path == "\\"):
            import string
            dirs = []
            for letter in string.ascii_uppercase:
                drive = f"{letter}:\\"
                if os.path.exists(drive):
                    dirs.append({"name": f"{letter}:", "path": drive.replace("\\", "/")})
            result["dirs"] = dirs
            result["timestamp"] = time.time()
            self.directory_query_result.set_value(json.dumps(result))
            return

        try:
            if os.path.exists(path) and os.path.isdir(path):
                dirs = []
                with os.scandir(path) as it:
                    for entry in it:
                        # Check ignore dirs (names)
                        if entry.name in self.ignore_dirs or entry.name.startswith('.'):
                            continue

                        # Check root ignore dirs (paths, normalized)
                        if os.path.normpath(entry.path) in self.root_ignore_dirs:
                            continue

                        if entry.is_dir():
                            dirs.append({
                                "name": entry.name,
                                "path": entry.path.replace("\\", "/")
                            })
                # Sort alphabetically
                dirs.sort(key=lambda x: x["name"].lower())
                result["dirs"] = dirs
                print(f"FilesystemBrowser: Found {len(dirs)} subdirs in {path}")
        except Exception as e:
            print(f"Error getting subdirs for {path}: {e}")

        result["timestamp"] = time.time()
        self.directory_query_result.set_value(json.dumps(result))

    def load_file(self, path):
        """Logic to load file into xstudio."""
        # Handle directory navigation
        if os.path.isdir(path):
            self.current_path_attr.set_value(path)
            self._add_to_history(path)
            self.start_search(path)
            return

        try:
            valid_playlist = None
            
            # 1. Try Selected Containers
            try:
                selection = self.connection.api.session.selected_containers
                for item in selection:
                    if isinstance(item, Playlist) and item.name != "Preview":
                        valid_playlist = item
                        self.last_used_playlist_uuid = item.uuid
                        print(f"Targeting Selected Playlist: {item.name}")
                        break
            except Exception:
                pass
            
            # 2. Try Cached Playlist (Sticky)
            if not valid_playlist and hasattr(self, 'last_used_playlist_uuid'):
                try:
                    target_uuid_str = str(self.last_used_playlist_uuid)
                    for p in self.connection.api.session.playlists:
                        if str(p.uuid) == target_uuid_str and p.name != "Preview":
                            valid_playlist = p
                            print(f"Targeting Cached Playlist: {p.name}")
                            break
                except:
                    pass

            # 3. Try Viewed Container
            if not valid_playlist:
                try:
                    viewed = self.connection.api.session.viewed_container
                    if isinstance(viewed, Playlist) and viewed.name != "Preview":
                        valid_playlist = viewed
                        self.last_used_playlist_uuid = viewed.uuid
                        print(f"Targeting Viewed Playlist: {viewed.name}")
                except Exception:
                    pass

            # 4. Fallback to first non-preview playlist
            if not valid_playlist:
                 playlists = [p for p in self.connection.api.session.playlists if p.name != "Preview"]
                 if playlists:
                     valid_playlist = playlists[0]
                     # print(f"Targeting First Playlist (Fallback): {valid_playlist.name}")
                 else:
                     self.connection.api.session.create_playlist("Filesystem Import")
                     valid_playlist = [p for p in self.connection.api.session.playlists if p.name != "Preview"][0]
                 # Update cache to this fallback
                 self.last_used_playlist_uuid = valid_playlist.uuid
            
            # If we were in preview mode, switch back to the original playlist
            if self.preview_playlist_uuid is not None:
                if self.original_playlist_uuid is not None:
                    orig_uuid_str = str(self.original_playlist_uuid)
                    for p in self.connection.api.session.playlists:
                        if str(p.uuid) == orig_uuid_str:
                            valid_playlist = p
                            print(f"Restoring to original playlist from preview: {p.name}")
                            break
                
                # Capture the preview uuid to delete later
                self.pending_preview_deletion_uuid = self.preview_playlist_uuid
                
                self.original_playlist_uuid = None
                self.preview_playlist_uuid = None

            playlist = valid_playlist

            # Guard: if resolved container is not a real Playlist (e.g. Timeline,
            # Subset, ContactSheet), fall back to the first actual Playlist in the
            # session.  Only Playlist.add_media() accepts string paths.
            if playlist is not None and not isinstance(playlist, Playlist):
                print(f"Container '{playlist.name}' is not a Playlist, searching for fallback...")
                fallback = [p for p in self.connection.api.session.playlists
                            if isinstance(p, Playlist) and p.name != "Preview"]
                if fallback:
                    playlist = fallback[0]
                    self.last_used_playlist_uuid = playlist.uuid
                    print(f"Fell back to Playlist: {playlist.name}")
                else:
                    self.connection.api.session.create_playlist("Filesystem Import")
                    playlist = [p for p in self.connection.api.session.playlists
                                if isinstance(p, Playlist) and p.name != "Preview"][0]
                    self.last_used_playlist_uuid = playlist.uuid
                    print(f"Created fallback Playlist: {playlist.name}")

            # --- Duplicate Check Logic: Local Cache ---
            if not hasattr(self, 'playlist_path_cache'):
                self.playlist_path_cache = {} # Dict[uuid_str, set(paths)]

            pl_uuid = str(playlist.uuid)
            if pl_uuid not in self.playlist_path_cache:
                self.playlist_path_cache[pl_uuid] = set()
            
            # Check if media already exists in playlist
            existing_media = None
            try:
                # Force refresh of media list?? No direct method, accessing .media should request it.
                current_media_list = playlist.media
                
                # Normalize input path: absolute + normpath
                tgt_path = os.path.normpath(os.path.abspath(path))
                
                print(f"Checking for duplicates of: {tgt_path}")
                
                for m in current_media_list:
                    try:
                        ms = m.media_source()
                        mr = ms.media_reference
                        if mr:
                            # URI path might include file:// scheme or be absolute
                            u = mr.uri()
                            mp = u.path()
                            if mp:
                                # Also abspath/normpath the existing media path
                                mp_norm = os.path.normpath(os.path.abspath(mp))
                                # print(f"  Existing: {mp_norm}")
                                if mp_norm == tgt_path:
                                    existing_media = m
                                    print("  -> Match found!")
                                    break
                    except:
                        continue
            except Exception as e:
                print(f"Dup check error: {e}")


            if existing_media:
                media = existing_media
                print(f"Media already exists: {path}")
            elif tgt_path in self.playlist_path_cache[pl_uuid]:
                 # In cache but not in media list yet (pending)
                 print(f"Skipping duplicate (pending load): {path}")
                 return
            else:
                # --- Sequence Handling ---
                loaded_as_sequence = False
                if fileseq_available:
                    try:
                        seq = fileseq.FileSequence(path)
                        if len(seq) > 1:
                            # It's a sequence!
                            # Construct xstudio-compatible sequence string with Explicit Range:
                            # /path/to/prefix_{:04d}.ext=1001-1050
                            
                            dirname = seq.dirname()
                            basename = seq.basename() # e.g. 'shot_' or 'shot.'
                            
                            # Calculate padding width from '####' or '@@@@@'
                            pad_str = seq.padding()
                            if pad_str == '#':
                                pad_len = 4
                            else:
                                pad_len = len(pad_str) if pad_str else 0
                            
                            # Construct brace pattern e.g. {:04d}
                            # If no padding, just empty brace? No, xstudio expects {:0Nd} usually.
                            # But fileseq handling > 1 implies padding.
                            
                            brace_padding = f"{{:0{pad_len}d}}" if pad_len > 0 else ""
                            
                            frames = str(seq.frameSet()) # e.g. 1001-1050
                            ext = seq.extension() # e.g. .exr
                            
                            # Normalize basename: sometimes fileseq puts the whole thing in basename.
                            # But typical usage: dirname + basename + padded_part + ext
                            
                            # Construct the special path for xstudio parsing
                            # IMPORTANT: xstudio regex expects: ^(.*\{.+\}.*?)(=([-0-9x,]+))?$
                            # So we put the brace pattern in the path, and the range at end.
                            
                            seq_path = f"{dirname}{basename}{brace_padding}{ext}={frames}"
                            
                            # playlist.add_media(path) calls parse_posix_path internally 
                            # which handles this pattern.
                            media = playlist.add_media(seq_path)
                            loaded_as_sequence = True
                            
                    except Exception as e:
                        print(f"Sequence load error: {e}")

                if not loaded_as_sequence:
                    media = playlist.add_media(path)
                    print(f"Loaded File: {path}")
                else:
                    print(f"Loaded Sequence: {seq_path}")
                # Add to cache immediately
                self.playlist_path_cache[pl_uuid].add(tgt_path)

            # Force the viewport to display the playlist (parent of the media)
            # We can't set the media directly as source if we want to use the playlist's playhead logic effectively 
            # (and avoid "create_playhead_atom" errors on MediaActor).
            self.connection.api.session.set_on_screen_source(playlist)
            
            # also try setting the selected/viewed container to force UI update
            try:
                self.connection.api.session.viewed_container = playlist
            except:
                pass
            
            # Select the media in the playlist's playhead selection
            # This ensures the playhead jumps to/plays this specific media
            if hasattr(playlist, 'playhead_selection'):
                playlist.playhead_selection.set_selection([media.uuid])

            # Start playback
            try:
                # Use the playlist's playhead to control playback
                playlist.playhead.playing = True
            except:
                pass

            # Final cleanup of the Preview playlist if we have one pending
            if hasattr(self, 'pending_preview_deletion_uuid') and self.pending_preview_deletion_uuid:
                try:
                    prev_uuid = self.pending_preview_deletion_uuid
                    self.pending_preview_deletion_uuid = None
                    
                    _dbg(f"Attempting to delete Preview playlist node for actor: {prev_uuid}")
                    
                    # We need the tree node UUID, not the actor UUID
                    tree = self.connection.api.session.playlist_tree
                    cuuid = self._find_container_uuid(tree, prev_uuid)
                    
                    if cuuid:
                        _dbg(f"Found tree node UUID: {cuuid}, calling remove_container")
                        res = self.connection.api.session.remove_container(cuuid)
                        _dbg(f"Deletion result: {res}")
                        print(f"FilesystemBrowser: Deleted Preview playlist (Node: {cuuid})")
                    else:
                        _dbg(f"Could not find tree node UUID for {prev_uuid}")
                        # Fallback to old method just in case, though likely to fail
                        for p in self.connection.api.session.playlists:
                            if str(p.uuid) == str(prev_uuid):
                                self.connection.api.session.remove_container(p)
                                break
                except Exception as e:
                    _dbg(f"Final cleanup error: {e}")
                    print(f"Error in final preview cleanup: {e}")

        except Exception as e:
            print(f"Error loading file: {e}")
            import traceback
            traceback.print_exc()

    def _load_multiple_files(self, paths):
        """Load multiple files into the current playlist."""
        print(f"FilesystemBrowser: Loading {len(paths)} files")
        for path in paths:
            try:
                self.load_file(path)
            except Exception as e:
                print(f"Error loading file {path}: {e}")


    def apply_filters(self):
        """Re-run filtering logic on the current results cache."""
        try:
            with self.results_lock:
                 results = list(self.current_scan_results)
            
            # Offload heavy filtering if list is huge? 
            # For now, do it in main thread or worker? 
            # Safe to do in main thread if count < 100k?
            # Better to spawn a thread if we want UI responsiveness.
            
            # Doing it synchronously for now, but catching errors
            self._apply_filters_logic(results)
        except Exception as e:
            print(f"Error applying filters: {e}")

    def _apply_filters_logic(self, results):
        import os
        # Use cached values if available (from worker), else fetch live (UI update)
        if hasattr(self, 'cached_filter_time'):
            filter_time = self.cached_filter_time
        else:
            filter_time = self.filter_time_attr.value() if hasattr(self, 'filter_time_attr') else "Any"

        if hasattr(self, 'cached_filter_version'):
            filter_version = self.cached_filter_version
        else:
            filter_version = self.filter_version_attr.value() if hasattr(self, 'filter_version_attr') else "All Versions"

        print(f"Applying filters: Time={filter_time}, Version={filter_version}, Count={len(results)}")

        # Separate directories and files
        dirs = []
        files = []
        for r in results:
            if r.get("is_folder") or r.get("type") == "Folder":
                dirs.append(r)
            else:
                files.append(r)

        # 1. Apply Time Filter (to files only?)
        # User wants to see directories "even if there isnt data in them".
        # So we probably shouldn't filter directories by time unless requested.
        # Let's Apply Time Filter ONLY to files for now.
        if filter_time != "Any":
            now = time.time()
            cutoff = 0
            if filter_time == "Last 1 day":
                cutoff = now - 86400
            elif filter_time == "Last 2 days":
                cutoff = now - 2 * 86400
            elif filter_time == "Last 1 week":
                cutoff = now - 7 * 86400
            elif filter_time == "Last 1 month":
                cutoff = now - 30 * 86400
            
            if cutoff > 0:
                files = [r for r in files if r.get("date", 0) >= cutoff]

        # 2. Apply Version Filter with Grouping (Files only)
        grouped_results = {}
        for r in files:
            grp = r.get("version_group")
            if grp:
                grouped_results.setdefault(grp, []).append(r)
            else:
                grouped_results.setdefault(id(r), [r])
        
        filtered_files = []
        
        for grp, items in grouped_results.items():
            if len(items) <= 1:
                filtered_files.extend(items)
                continue
                
            items.sort(key=lambda x: x.get("version", 0), reverse=True)
            
            if filter_version == "Latest Version":
                filtered_files.extend(items[:1])
            elif filter_version == "Latest 2 Versions":
                filtered_files.extend(items[:2])
            else:
                filtered_files.extend(items)
        

        # Combine: Keep all discovered directories to facilitate browsing,
        # and combine with filtered files.
        final_results = dirs + filtered_files
        
        # Resort by name for display
        final_results.sort(key=lambda x: x["name"])

        # Serialize
        json_str = json.dumps(final_results)
        
        self.files_attr.set_value(json_str)

    def _on_filter_changed(self, attribute, role):
        from xstudio.core import AttributeRole
        if role == AttributeRole.Value:
            # Re-apply filters on cached results
            threading.Thread(target=self.apply_filters).start()


    def _replace_current_media(self, path):
        try:
            print(f"Replacing current media with: {path}")
            # 1. Identify valid playlist (use same logic as load_file or simplify)
            # For replace, we usually mean the "active" playlist/viewed one.
            playlist = None
            try:
                viewed = self.connection.api.session.viewed_container
                if hasattr(viewed, 'add_media'):
                    playlist = viewed
            except:
                pass
                
            if not playlist:
                # Fallback to selection
                try:
                    selection = self.connection.api.session.selected_containers
                    if selection and hasattr(selection[0], 'add_media'):
                        playlist = selection[0]
                except:
                    pass
            
            if not playlist:
                print("No active playlist found for replace.")
                return

            self.connection.api.session.set_on_screen_source(playlist)

            # 2. Add new media
            # Use same helpers as load_file for sequences? 
            # Ideally load_file should be refactored to return the media object.
            # For now, duplicate simple add logic or internal helper.
            # Let's use simple add for now to save complexity, or better, 
            # we need sequence logic.
            # Refactor load_file is risky mid-flight. 
            # I will assume path is safe or reuse the sequence logic block?
            # Let's extract sequence loading to a helper `_add_media_to_playlist(playlist, path)`
            
            new_media = self._add_media_to_playlist(playlist, path)
            if not new_media:
                return

            # 3. Find currently selected/playing components to remove
            # We want to remove the item that playhead is focusing on? 
            # Or just the selection?
            # "Replaces the media in the current viewport" implies the one being watched.
            
            items_to_remove = []
            if hasattr(playlist, 'playhead_selection'):
                 # Get what is currently selected/playing
                 # selected_sources returns list of Media objects
                 current_selection = playlist.playhead_selection.selected_sources
                 if current_selection:
                     items_to_remove = current_selection
                 
            # 4. Select new media
            if hasattr(playlist, 'playhead_selection'):
                playlist.playhead_selection.set_selection([new_media.uuid])
                
            # 5. Move new media to position of old media?
            # playlist.move_media(new_media, before=old_media_uuid)
            if items_to_remove:
                # Move before the first removed item
                try:
                    playlist.move_media(new_media, before=items_to_remove[0].uuid)
                except Exception as e:
                    print(f"Move error: {e}")
                
            # 6. Remove old media
            for m in items_to_remove:
                try:
                    playlist.remove_media(m)
                except Exception as e:
                    print(f"Remove error: {e}")

            # 7. Play
            if hasattr(playlist, 'playhead'):
                playlist.playhead.playing = True

        except Exception as e:
            print(f"Replace error: {e}")
            import traceback
            traceback.print_exc()

    def _compare_with_current_media(self, path):
        try:
            print(f"Comparing current media with: {path}")
            # 1. Identify valid playlist
            playlist = None
            try:
                viewed = self.connection.api.session.viewed_container
                if hasattr(viewed, 'add_media'):
                    playlist = viewed
            except:
                pass
            
            if not playlist:
                print("No active playlist found for compare.")
                return

            self.connection.api.session.set_on_screen_source(playlist)

            # 2. Add new media
            new_media = self._add_media_to_playlist(playlist, path)
            if not new_media:
                return

            # 3. Get current selection and append new media
            new_selection = []
            if hasattr(playlist, 'playhead_selection'):
                 current_m = playlist.playhead_selection.selected_sources
                 for m in current_m:
                     new_selection.append(m.uuid)
            
            new_selection.append(new_media.uuid)
            
            # 4. Set selection
            if hasattr(playlist, 'playhead_selection'):
                playlist.playhead_selection.set_selection(new_selection)
                
            # 5. Set Compare Mode
            if hasattr(playlist, 'playhead'):
                # Check for AB mode availability? 
                # Assuming "A/B" string is correct based on other plugins/docs
                playlist.playhead.compare_mode = "A/B"
                playlist.playhead.playing = True

        except Exception as e:
             print(f"Compare error: {e}")
             import traceback
             traceback.print_exc()

    def _compare_items(self, paths):
        """Load multiple files into the current playlist for comparison."""
        try:
            print(f"Compare items: loading {len(paths)} files")
            for path in paths:
                try:
                    self.load_file(path)
                except Exception as e:
                    print(f"Error loading {path} for compare: {e}")
            print(f"Loaded {len(paths)} files. Use the Compare button in the viewport toolbar to switch compare modes.")
        except Exception as e:
            print(f"Compare items error: {e}")
            import traceback
            traceback.print_exc()

    def _add_to_timeline(self, paths):
        """Add files as clips on a Timeline."""
        try:
            # --- Find a real Playlist ---
            valid_playlist = None

            try:
                for item in self.connection.api.session.selected_containers:
                    if isinstance(item, Playlist) and item.name != "Preview":
                        valid_playlist = item
                        break
            except Exception:
                pass

            if not valid_playlist:
                try:
                    viewed = self.connection.api.session.viewed_container
                    if isinstance(viewed, Playlist) and viewed.name != "Preview":
                        valid_playlist = viewed
                except Exception:
                    pass

            if not valid_playlist:
                playlists = [p for p in self.connection.api.session.playlists if p.name != "Preview"]
                if playlists:
                    valid_playlist = playlists[0]
                else:
                    self.connection.api.session.create_playlist("Filesystem Import")
                    valid_playlist = [p for p in self.connection.api.session.playlists if p.name != "Preview"][0]

            # --- Find or create a Timeline ---
            timeline = None

            try:
                viewed = self.connection.api.session.viewed_container
                if isinstance(viewed, Timeline):
                    timeline = viewed
            except Exception:
                pass

            if not timeline:
                try:
                    for container in valid_playlist.containers:
                        if isinstance(container, Timeline):
                            timeline = container
                            break
                except Exception:
                    pass

            if not timeline:
                _uuid, timeline = valid_playlist.create_timeline(name="Timeline", with_tracks=True)

            # --- Get the first video track ---
            video_tracks = timeline.video_tracks
            if video_tracks:
                video_track = video_tracks[0]
            else:
                video_track = timeline.insert_video_track()

            # --- Add each file as a clip ---
            added = 0
            for path in paths:
                try:
                    media = valid_playlist.add_media(path)
                    timeline.add_media(media)
                    video_track.insert_clip(media, index=-1)
                    added += 1
                except Exception as e:
                    print(f"Error adding clip {path}: {e}")

            print(f"Added {added}/{len(paths)} clip(s) to timeline.")

        except Exception as e:
            print(f"Add to timeline error: {e}")
            import traceback
            traceback.print_exc()

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

    def _request_thumbnail(self, path):
        """Queue an async thumbnail fetch if not already cached or pending."""
        if path in self._thumbnail_cache:
            # Already done — push the cached URI back to UI immediately
            self._update_file_thumbnail(path, self._thumbnail_cache[path])
            return
        with self._thumb_lock:
            if path not in self._thumb_pending:
                self._thumb_pending.add(path)
                self._thumb_queue.put(path)

    def _thumb_worker_loop(self):
        """Daemon worker pulling thumbnail requests from the queue."""
        while True:
            path = self._thumb_queue.get()
            try:
                self._generate_thumbnail(path)
            except Exception as e:
                _dbg(f"WORKER_ERR: {e}")
            finally:
                with self._thumb_lock:
                    self._thumb_pending.discard(path)
                self._thumb_queue.task_done()

    def _resolve_sequence_frame(self, path):
        """Given a fileseq path string, return (concrete_file_path, frame_number).
        For single files, returns (path, 0)."""
        if not fileseq_available:
            return path, 0
        try:
            seq = fileseq.FileSequence(path)
            frames = list(seq.frameSet())
            if len(frames) > 1:
                mid = frames[len(frames) // 2]
                return seq.frame(mid), mid
            elif len(frames) == 1:
                return seq.frame(frames[0]), frames[0]
        except Exception:
            pass
        return path, 0

    def _generate_thumbnail(self, path):
        """Generate a thumbnail JPEG using ffmpeg subprocess."""
        if not self._ffmpeg_bin:
            _dbg(f"GEN_SKIP (no ffmpeg): {path}")
            return

        target_file, _frame = self._resolve_sequence_frame(path)
        _dbg(f"GEN_START: {path} -> {target_file}")

        if not os.path.exists(target_file):
            _dbg(f"GEN_MISSING: {target_file}")
            return

        out_file = os.path.join(self._temp_dir, f"{_uuid.uuid4().hex}.jpg")

        env = os.environ.copy()
        if self._ffmpeg_dyld and sys.platform == "darwin":
            existing = env.get("DYLD_LIBRARY_PATH", "")
            env["DYLD_LIBRARY_PATH"] = (
                self._ffmpeg_dyld + (":" + existing if existing else "")
            )

        cmd = [
            self._ffmpeg_bin,
            "-y",               # overwrite output file
            "-i", target_file,
            "-vf", "scale=150:-1,format=rgb24",
            "-frames:v", "1",
            "-update", "1",     # allow single-image output
            out_file,
        ]

        _dbg(f"GEN_CMD: {' '.join(cmd)}")
        try:
            result = subprocess.run(
                cmd, env=env,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=30
            )
            if result.returncode == 0 and os.path.exists(out_file):
                thumb_uri = pathlib.Path(out_file).as_uri()
                _dbg(f"GEN_OK: {thumb_uri}")
                self._thumbnail_cache[path] = thumb_uri
                self._update_file_thumbnail(path, thumb_uri)
            else:
                stderr = result.stderr.decode("utf-8", errors="replace")[-500:]
                _dbg(f"GEN_FAIL (rc={result.returncode}): {stderr}")
        except subprocess.TimeoutExpired:
            _dbg(f"GEN_TIMEOUT: {target_file}")
        except Exception as exc:
            _dbg(f"GEN_EXCEPTION: {exc}")

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


    def _add_media_to_playlist(self, playlist, path):
        """Helper to add media handling sequences."""
        import os
        try:
             tgt_path = os.path.normpath(os.path.abspath(path))
             
             # Check for sequence
             if fileseq_available:
                 try:
                    seq = fileseq.FileSequence(path)
                    if len(seq) > 1:
                        dirname = seq.dirname()
                        basename = seq.basename()
                        pad_str = seq.padding()
                        pad_len = len(pad_str) if pad_str else 0
                        brace_padding = f"{{:0{pad_len}d}}" if pad_len > 0 else ""
                        frames = str(seq.frameSet())
                        ext = seq.extension()
                        seq_path = f"{dirname}{basename}{brace_padding}{ext}={frames}"
                        return playlist.add_media(seq_path)
                 except:
                    pass
             
             return playlist.add_media(path)
        except Exception as e:
            print(f"Add media error: {e}")
            return None

    def _find_container_uuid(self, tree, target_value_uuid):
        """Recursively find the tree node UUID for a given playlist actor UUID."""
        if hasattr(tree, 'value_uuid') and str(tree.value_uuid) == str(target_value_uuid):
            return tree.uuid
        if hasattr(tree, 'children'):
            for child in tree.children:
                res = self._find_container_uuid(child, target_value_uuid)
                if res:
                    return res
        return None

    def _preview_file(self, path):
        """Load a file into the transient Preview playlist."""
        try:
            print(f"FilesystemBrowser: Previewing {path}")
            
            # If we are not already in preview mode, capture the current playlist context
            if self.preview_playlist_uuid is None:
                self.original_playlist_uuid = None
                try:
                    viewed = self.connection.api.session.viewed_container
                    if hasattr(viewed, 'add_media') and viewed.name != "Preview":
                        self.original_playlist_uuid = viewed.uuid
                        print(f"FilesystemBrowser: Saving original playlist {viewed.name}")
                except Exception as e:
                    print(f"FilesystemBrowser: Could not get viewed container: {e}")
                
            # Attempt to capture the exact frame number we are currently looking at
            current_frame = None
            try:
                # Need to use viewport playhead or session playhead to find logical frame
                # Or try the playlist's playhead
                viewed = self.connection.api.session.viewed_container
                if hasattr(viewed, 'playhead'):
                    current_frame = viewed.playhead.position
                    print(f"FilesystemBrowser: Captured frame sync position: {current_frame}")
            except Exception as e:
                print(f"FilesystemBrowser: Could not capture playhead position: {e}")

            # Find or Create the 'Preview' playlist
            preview_playlist = None
            for p in self.connection.api.session.playlists:
                if p.name == "Preview":
                    preview_playlist = p
                    break
            
            if not preview_playlist:
                self.connection.api.session.create_playlist("Preview")
                for p in self.connection.api.session.playlists:
                    if p.name == "Preview":
                        preview_playlist = p
                        break
            
            if not preview_playlist:
                print("FilesystemBrowser: Could not create or find Preview playlist")
                return
                
            self.preview_playlist_uuid = preview_playlist.uuid

            # Clear the remote preview playlist
            for m in list(preview_playlist.media):
                preview_playlist.remove_media(m)
                
            # Add the new media
            media = self._add_media_to_playlist(preview_playlist, path)
            if not media:
                 return

            # Force the viewport to display the preview playlist
            self.connection.api.session.set_on_screen_source(preview_playlist)
            
            # also try setting the selected/viewed container to force UI update
            try:
                # XStudio python API may support setting viewed_container or selected_containers
                # This ensures the session panel highlights the preview playlist
                self.connection.api.session.viewed_container = preview_playlist
            except:
                pass
            
            # Select the media
            if hasattr(preview_playlist, 'playhead_selection'):
                preview_playlist.playhead_selection.set_selection([media.uuid])

            # Restore the frame number if we have one
            if hasattr(preview_playlist, 'playhead'):
                if current_frame is not None:
                    try:
                        preview_playlist.playhead.position = current_frame
                        print(f"FilesystemBrowser: Restored frame position: {current_frame}")
                    except Exception as e:
                        print(f"FilesystemBrowser: Error restoring frame: {e}")
                
                # pause on load for preview
                preview_playlist.playhead.playing = False
                
        except Exception as e:
            print(f"FilesystemBrowser Preview error: {e}")
            import traceback
            traceback.print_exc()

def create_plugin_instance(connection):
    return FilesystemBrowserPlugin(connection)
