# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Sam Richards

from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, FrameList, add_media_atom, Uuid, URI
import os
import json
import threading
import queue
import time
from datetime import datetime

# Try importing fileseq
try:
    import fileseq
    fileseq_available = True
except ImportError:
    fileseq_available = False
    print("Warning: fileseq module not found. Sequence detection will be disabled.")


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
            print(f"FilesystemBrowser: Error merging pins: {e}")

        # Connect listeners
        # Note: We need to register callbacks properly.
        # attribute_changed method handles all.
        
        # Internal state
        self.extensions = {".mov", ".exr", ".png", ".mp4"}
        self.ignore_dirs = {".git", ".svn", "__pycache__", ".DS_Store"}
        self.search_thread = None
        self.cancel_search = False
        self.results_lock = threading.Lock()  # Protects current_scan_results
        self.current_scan_results = []
        
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
            val = self.command_attr.value()
            if not val:
                return # Empty command
                
            try:
                data = json.loads(val)
                action = data.get("action")
                
                if action == "change_path":
                    new_path = data.get("path")
                    if os.path.exists(new_path) and os.path.isdir(new_path):
                         self.current_path_attr.set_value(new_path)
                         self._add_to_history(new_path)
                         self.start_search(new_path)
                    else:
                         print(f"Invalid path: {new_path}")

                elif action == "load_file":
                    file_path = data.get("path")
                    self.load_file(file_path)
                    
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

                elif action == "add_pin":
                    name = data.get("name")
                    path = data.get("path")
                    self._add_pin(name, path)

                elif action == "remove_pin":
                    path = data.get("path")
                    self._remove_pin(path)
                
                # Clear command channel
                self.command_attr.set_value("")
                
            except Exception as e:
                print(f"Command error: {e}")
                import traceback
                traceback.print_exc()
                
        elif attribute.uuid in (self.filter_time_attr.uuid, self.filter_version_attr.uuid):
            if role == AttributeRole.Value:
                self._on_filter_changed(attribute, role)

    def compute_completions(self, partial_path):
        """Minimal logic to find subdirectories matching partial path."""
        try:
            # If empty, do nothing
            if not partial_path:
                self.completions_attr.set_value("[]")
                return

            # Determine directory to scan
            if partial_path.endswith(os.path.sep):
                 directory = partial_path
                 base = ""
            else:
                 directory = os.path.dirname(partial_path)
                 base = os.path.basename(partial_path)
            
            # If directory part is empty, and we are not at root... 
            if not directory:
                 directory = "/" 

            if not os.path.exists(directory) or not os.path.isdir(directory):
                self.completions_attr.set_value("[]")
                return
                
            candidates = []
            try:
                for item in os.listdir(directory):
                    if item in self.ignore_dirs or item.startswith('.'):
                        continue
                        
                    full_p = os.path.join(directory, item)
                    if os.path.isdir(full_p):
                        # Filter by base
                        if item.startswith(base):
                            candidates.append(full_p + os.path.sep)
            except OSError:
                pass
                
            # Sort and limit
            candidates.sort()
            import json
            self.completions_attr.set_value(json.dumps(candidates[:10]))
            
        except Exception as e:
            print(f"Completion error: {e}")
            self.completions_attr.set_value("[]")


    def load_file(self, path):
        # Handle directory navigation
        if os.path.isdir(path):
            self.current_path_attr.set_value(path)
            self._add_to_history(path)
            self.start_search(path)
            return

        # Logic to load file into xstudio
        try:
            valid_playlist = None
            
            # 1. Try Selected Containers
            try:
                selection = self.connection.api.session.selected_containers
                for item in selection:
                    if hasattr(item, 'add_media'):
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
                        if str(p.uuid) == target_uuid_str:
                            valid_playlist = p
                            print(f"Targeting Cached Playlist: {p.name}")
                            break
                except:
                    pass

            # 3. Try Viewed Container
            if not valid_playlist:
                try:
                    viewed = self.connection.api.session.viewed_container
                    if hasattr(viewed, 'add_media'):
                        valid_playlist = viewed
                        self.last_used_playlist_uuid = viewed.uuid
                        print(f"Targeting Viewed Playlist: {viewed.name}")
                except Exception:
                    pass

            # 4. Fallback to first playlist
            if not valid_playlist:
                 playlists = self.connection.api.session.playlists
                 if playlists:
                     valid_playlist = playlists[0]
                     # print(f"Targeting First Playlist (Fallback): {valid_playlist.name}")
                 else:
                     self.connection.api.session.create_playlist("Filesystem Import")
                     valid_playlist = self.connection.api.session.playlists[0]
                 # Update cache to this fallback
                 self.last_used_playlist_uuid = valid_playlist.uuid
            
            playlist = valid_playlist
            
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
                            
                            print(f"Loading Sequence via Brace Pattern: {seq_path}")
                            
                            # playlist.add_media(path) calls parse_posix_path internally 
                            # which handles this pattern.
                            media = playlist.add_media(seq_path)
                            loaded_as_sequence = True
                            
                    except Exception as e:
                        print(f"Sequence load error: {e}")

                if not loaded_as_sequence:
                    media = playlist.add_media(path)
                    
                print(f"Loaded: {path}")
                # Add to cache immediately
                self.playlist_path_cache[pl_uuid].add(tgt_path)

            # Force the viewport to display the playlist (parent of the media)
            # We can't set the media directly as source if we want to use the playlist's playhead logic effectively 
            # (and avoid "create_playhead_atom" errors on MediaActor).
            self.connection.api.session.set_on_screen_source(playlist)
            
            # Select the media in the playlist's playhead selection
            # This ensures the playhead jumps to/plays this specific media
            if hasattr(playlist, 'playhead_selection'):
                playlist.playhead_selection.set_selection([media.uuid])

            # Start playback
            try:
                # Use the playlist's playhead to control playback
                if hasattr(playlist, 'playhead'):
                    playlist.playhead.playing = True
            except Exception as e:
                print(f"Playback trigger error: {e}")

        except Exception as e:
            print(f"Error loading file: {e}")


    def start_search(self, start_path):
        if self.search_thread and self.search_thread.is_alive():
            self.cancel_search = True
            if hasattr(self, 'scanner'):
                self.scanner.stop()
            self.search_thread.join()
        
        self.cancel_search = False
        self.searching_attr.set_value(True)
        self.search_thread = threading.Thread(target=self._search_worker, args=(start_path,))
        self.search_thread.daemon = True
        self.search_thread.start()

    def _search_worker(self, start_path):
        print(f"Starting search in {start_path}")
        
        from .scanner import FileScanner
        
        # Config (could be loaded from prefs)
        config = {
            "extensions": list(self.extensions),
            "ignore_dirs": list(self.ignore_dirs),
            # "version_regex": r"_v(\d+)" 
        }
        
        self.scanner = FileScanner(config)
        with self.results_lock:
            self.current_scan_results = [] # Cache results for filtering
        self.pending_scan_results = []
        self.last_update = 0
        
        def progress_callback(results, info):
            # Report progress to UI
            # We send a JSON with status string and scanned count
            scanned = info.get("scanned", 0)
            phase = info.get("phase", "")
            progress = info.get("progress", 0)
            
            # Update progress attribute
            # Because of the scanning algorithm, the progress is not linear, so we need to bias it
            # to make it feel more linear to the user.
            biased_progress = pow(progress / 100.0, 2.0)*100
            self.progress_attr.set_value(str(biased_progress))
            self.scanned_attr.set_value(str(scanned))
            #self.scanProgress.set_value(str(progress))
            
            # Handle partial results
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
            if hasattr(self, 'main_executor'):
                 self.main_executor.execute(self.searching_attr.set_value, False)
            else:
                 self.searching_attr.set_value(False)

    def apply_filters(self):
        # Filtering logic
        
        with self.results_lock:
            results = list(self.current_scan_results)
            
        self._apply_filters_logic(results)
        
    def _apply_filters_logic(self, results):
        filter_time = self.filter_time_attr.value() if hasattr(self, 'filter_time_attr') else "Any"
        filter_version = self.filter_version_attr.value() if hasattr(self, 'filter_version_attr') else "All Versions"

        # 1. Apply Time Filter
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
                results = [r for r in results if r.get("date", 0) >= cutoff]

        # 2. Apply Version Filter with Grouping
        # Group items by version_group
        grouped_results = {}
        for r in results:
            grp = r.get("version_group")
            if grp:
                grouped_results.setdefault(grp, []).append(r)
            else:
                # Use item ID as unique group so it survives
                grouped_results.setdefault(id(r), [r])
        
        final_filtered = []
        
        for grp, items in grouped_results.items():
            # If only 1 item, just take it
            if len(items) <= 1:
                final_filtered.extend(items)
                continue
                
            # Sort by version descending
            items.sort(key=lambda x: x.get("version", 0), reverse=True)
            
            if filter_version == "Latest Version":
                final_filtered.extend(items[:1])
            elif filter_version == "Latest 2 Versions":
                final_filtered.extend(items[:2])
            else:
                # All Versions
                final_filtered.extend(items)
        
        results = final_filtered
        
        # Resort by name for display
        results.sort(key=lambda x: x["name"])

        # Serialize
        json_str = json.dumps(results)
        
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


def create_plugin_instance(connection):
    return FilesystemBrowserPlugin(connection)
