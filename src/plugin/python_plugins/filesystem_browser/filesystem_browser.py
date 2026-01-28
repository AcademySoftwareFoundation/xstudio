
from xstudio.plugin import PluginBase
from xstudio.core import JsonStore
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

from PySide6.QtCore import QObject, Signal, Qt
from PySide6.QtWidgets import QApplication, QFileDialog

class MainThreadExecutor(QObject):
    execute_signal = Signal(object, object)

    def __init__(self):
        super().__init__()
        # Use simple direct connection if on same thread, otherwise queued.
        # But we specifically want to FORCE main thread execution from worker threads.
        # So we trust moveToThread + QueuedConnection.
        self.execute_signal.connect(self._execute, Qt.QueuedConnection)
        
        app = QApplication.instance()
        if app:
            self.moveToThread(app.thread())

    def _execute(self, func, args):
        try:
            func(*args)
        except Exception as e:
            print(f"MainThreadExecutor error: {e}")

    def execute(self, func, *args):
        self.execute_signal.emit(func, args)

class FilesystemBrowserPlugin(PluginBase):
    def __init__(self, connection):
        PluginBase.__init__(
            self,
            connection,
            "Filesystem Browser",
            qml_folder="qml/FilesystemBrowser.1"
        )
        
        self.main_executor = MainThreadExecutor()

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
            "Plugins|Filesystem Browser",
            0.0,
            hotkey_uuid=self.toggle_browser_action,
            callback=self.toggle_browser_from_menu
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
        
        # Internal state
        self.extensions = {".mov", ".exr", ".png", ".mp4"}
        self.search_thread = None
        self.cancel_search = False
        
        # Initial search
        self.start_search(self.current_path_attr.value())
        
    def toggle_browser_from_menu(self, menu_item=None, user_data=None):
        # Wrapper for menu callback
        # Since we are now a standard dockable panel, the user should use View -> Panels -> Filesystem Browser
        # or rely on the hotkey's default action if it maps to the view.
        # We'll just log here.
        print("Menu item clicked. The Filesystem Browser is available in the Panels menu.")
        self.toggle_browser(None, "Menu Click")

    def toggle_browser(self, converting, context):
        print(f"Toggling Filesystem Browser (Action Triggered). Context: {context}")
        # We can also verify visibility here if possible, but the Model handles it.


    def _open_browser_dialog(self, initial_path):
        """Runs on main thread to show dialog."""
        dir_path = QFileDialog.getExistingDirectory(None, "Select Directory", initial_path)
        if dir_path:
            # We can update the attribute directly here since we are on main thread (safe)
            # or use the worker thread if needed, but set_value is thread safe-ish in xstudio API usually,
            # or better yet, since we are in python plugin, set_value modifies the backend attribute.
            # The backend attribute change will trigger notification.
            # However, start_search expects to run on... wait, start_search runs a thread.
            self.current_path_attr.set_value(dir_path)
            self.start_search(dir_path)

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
                    # Update current path attribute so UI reflects it (if it didn't already)
                    # self.current_path_attr.set_value(new_path)
                    # Use set_value on current_path_attr to update UI and trigger search?
                    # No, we trigger search directly to be sure, or better:
                    if os.path.exists(new_path) and os.path.isdir(new_path):
                         self.current_path_attr.set_value(new_path)
                         self.start_search(new_path)
                    else:
                         print(f"Invalid path: {new_path}")

                elif action == "load_file":
                    file_path = data.get("path")
                    self.load_file(file_path)
                    
                elif action == "request_browser":
                    # Open native directory dialog
                    current = self.current_path_attr.value()
                    # Execute on main thread
                    if hasattr(self, 'main_executor'):
                        self.main_executor.execute(self._open_browser_dialog, current)
                    else:
                        print("Error: Main executor not available for dialog")
                        
                elif action == "complete_path":
                    partial = data.get("path", "")
                    self.compute_completions(partial)
                
                # Clear command channel
                self.command_attr.set_value("")
                
            except Exception as e:
                print(f"Command error: {e}")
                import traceback
                traceback.print_exc()

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
        # Logic to load file into xstudio
        # We need to find the playlist and add media
        try:
            # Create a playlist if none exists
            playlists = self.connection.api.session.playlists
            if not playlists:
                self.connection.api.session.create_playlist("Filesystem Import")
                playlist = self.connection.api.session.playlists[0]
            else:
                playlist = playlists[0]
            # Check if media already exists in playlist
            existing_media = None
            try:
                # playlist.media returns a list of Media objects
                current_media_list = playlist.media
                
                # Normalize path for comparison
                for m in current_media_list:
                    # m is a Media object. We need its path.
                    # Media -> MediaSource -> MediaReference -> URI -> path
                    # This chain might fail if media is invalid/loading, so try/except
                    try:
                        ms = m.media_source()
                        mr = ms.media_reference
                        if mr:
                            mr_path = mr.uri().path()
                            if mr_path == path:
                                existing_media = m
                                break
                    except:
                        continue
            except:
                pass


            if existing_media:
                media = existing_media
                print(f"Media already exists: {path}")
            else:
                media = playlist.add_media(path)
                print(f"Loaded: {path}")

            # Switch view to the playlist containing the media
            self.connection.api.session.viewed_container = playlist
            
            # Force the viewport to display this specific media
            # media object is a Container, so this should work to set it as active source
            self.connection.api.session.set_on_screen_source(media)

        except Exception as e:
            print(f"Error loading file: {e}")


    def start_search(self, start_path):
        if self.search_thread and self.search_thread.is_alive():
            self.cancel_search = True
            self.search_thread.join()
        
        self.cancel_search = False
        self.searching_attr.set_value(True) # Set immediately on start
        self.search_thread = threading.Thread(target=self._search_worker, args=(start_path,))
        self.search_thread.daemon = True
        self.search_thread.start()

    def _search_worker(self, start_path):
        print(f"Starting search in {start_path}")
        
        results = []
        
        # Breadth-first search
        q = queue.Queue()
        q.put(start_path)
        
        scanned_files = [] # List of all files found to pass to fileseq
        
        # Limit depth or count to avoid hanging forever?
        # User asked for recursive.
        
        count = 0
        max_files = 5000 # Safety limit for demo
        
        try:
            while not q.empty() and not self.cancel_search:
                current_dir = q.get()
                
                try:
                    with os.scandir(current_dir) as entries:
                        dir_entries = []
                        file_entries = []
                        
                        for entry in entries:
                            if entry.is_dir(follow_symlinks=False):
                                if not entry.name.startswith('.'): # Skip hidden dirs
                                    dir_entries.append(entry.path)
                            elif entry.is_file():
                                ext = os.path.splitext(entry.name)[1].lower()
                                if ext in self.extensions:
                                    file_entries.append(entry)
                        
                        # Add subdirs to queue (breadth-first)
                        for d in dir_entries:
                            q.put(d)
                            
                        # Add files to raw list
                        for f in file_entries:
                            scanned_files.append(f)
                            count += 1
                        
                        # Use directories as explicit entries in the UI too? 
                        # User asked for files. But usually navigating requires seeing folders.
                        # For now, let's just list files as requested + sequences.
                        # But wait, if we are doing recursive search, maybe we just show ALL matching files flattened?
                        # "resulting files should be displayed in a table like a conventional file list"
                        # "search should be by file-system level (bredth first)"
                        
                        # If we just show a flattened list of 1000s of files, it might be overwhelming.
                        # But that seems to be the request.
                        
                except PermissionError:
                    continue
                
                if count >= max_files:
                    print("Hit file limit")
                    break
            
            if self.cancel_search:
                return

            # Now process with fileseq
            final_list = []
            
            # Convert scandir entries to paths
            file_paths = [f.path for f in scanned_files]
            
            if fileseq_available and file_paths:
                sequences = fileseq.findSequencesInList(file_paths)
                for seq in sequences:
                    item = {}
                    if len(seq) > 1:
                        # fileseq object
                        # Name: just the basename with padding
                        f_name = seq.format("{basename}{padding}{extension}")
                        item["name"] = f_name
                        item["type"] = "Sequence"
                        # Frames: Show the range (e.g. 1-100)
                        item["frames"] = str(seq.frameRange())
                        
                        # Path: Construct robust path with # padding, ensuring directory is included
                        # fileseq v2 might behave differently with format so we construct manually
                        # getting the padding char count
                        pad_len = len(str(seq.end()))
                        # Check if we can get padding string from fileseq
                        try:
                             # simple heuristic if padding char is standard
                             padding_str = "#" * len(list(seq.padding())[0]) if seq.padding() else "#" * pad_len
                        except:
                             padding_str = "#" * 4 # fallback

                        # Use native fileseq formatting if possible, forcing hash style
                        # But user reported path being just "." or dir.
                        # Ideally: /path/to/seq.####.exr
                        # fileseq.format with specific template usually works. 
                        # We will try strict reconstruction.
                        item["path"] = f"{seq.dirname()}{seq.basename()}{padding_str}{seq.extension()}"
                        # Relpath
                        try:
                            # Use directory of sequence for relpath
                            seq_dir = seq.dirname()
                            item["relpath"] = os.path.relpath(seq_dir, start_path)
                        except:
                            item["relpath"] = "."
                    else:
                        # Single file (represented as a sequence of 1 by fileseq)
                        item["name"] = seq.basename() + seq.extension()
                        item["type"] = "File"
                        item["frames"] = "1"
                        item["path"] = seq[0]
                        try:
                            item["relpath"] = os.path.relpath(seq[0], start_path)
                        except:
                            item["relpath"] = "."
                        
                        # Get size
                        try:
                           item["size_str"] = f"{os.path.getsize(seq[0]) / 1024:.1f} KB"
                        except:
                           item["size_str"] = "?"

                    final_list.append(item)
            else:
                 # Fallback if no fileseq
                 for f in scanned_files:
                     item = {
                         "name": f.name,
                         "type": "File",
                         "size_str": f"{f.stat().st_size / 1024:.1f} KB",
                         "frames": "1",
                         "path": f.path,
                     }
                     try:
                         item["relpath"] = os.path.relpath(f.path, start_path)
                     except:
                         item["relpath"] = "."
                         
                     final_list.append(item)

            # Sort by name
            final_list.sort(key=lambda x: x["name"])
            
            # Prepare JSON
            json_str = json.dumps(final_list)
            
            # Update attribute in main thread via executor
            if hasattr(self, 'main_executor'):
                 self.main_executor.execute(self.files_attr.set_value, json_str)
            else:
                 self.files_attr.set_value(json_str)
                 
            print(f"Search finished, found {len(final_list)} items")
            
        except Exception as e:
            print(f"Search error: {e}")
            import traceback
            traceback.print_exc()
        finally:
            # Always ensure searching is False at the end
            if hasattr(self, 'main_executor'):
                 self.main_executor.execute(self.searching_attr.set_value, False)
            else:
                 self.searching_attr.set_value(False)

def create_plugin_instance(connection):
    return FilesystemBrowserPlugin(connection)
