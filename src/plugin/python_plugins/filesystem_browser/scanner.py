# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Sam Richards

import os
import re
import threading
import queue
import time
import pwd
import json
from concurrent.futures import ThreadPoolExecutor

try:
    import fileseq
except ImportError:
    fileseq = None

class FileScanner:
    def __init__(self, config=None):
        self.config = config or {}
        self.extensions = set(self.config.get("extensions", [".mov", ".exr", ".png", ".mp4", ".jpg", ".jpeg", ".dpx", ".tiff", ".tif"]))
        self.ignore_dirs = set(self.config.get("ignore_dirs", [".git", ".svn", "__pycache__"]))
        self.non_sequence_extensions = set(self.config.get("non_sequence_extensions", [".mov", ".mp4"]))
        self.version_regex = re.compile(self.config.get("version_regex", r"_v(\d+)"))
        self.max_workers = self.config.get("thread_count", 4)
        self.max_depth = self.config.get("max_depth", 6)
        
        self.cancel_event = threading.Event()
        self.executor = ThreadPoolExecutor(max_workers=self.max_workers)
        


    def get_owner(self, uid):
        try:
            return pwd.getpwuid(uid).pw_name
        except KeyError:
            return str(uid)

    def format_size_str(self, size_bytes):
        if size_bytes == 0:
            return "0 B"
        
        size_name = ("B", "KB", "MB", "GB", "TB", "PB")
        i = 0
        p = float(size_bytes)
        
        while i < len(size_name) - 1 and p >= 1024:
            p /= 1024.0
            i += 1
            
        return f"{p:.2f} {size_name[i]}"

    def scan(self, start_path, callback=None):
        """
        Scans from start_path using BFS and weighted progress.
        callback(results, progress_info) is called periodically.
        """
        self.cancel_event.clear()
        
        from collections import deque
        from concurrent.futures import wait, FIRST_COMPLETED

        # Queue of (path, weight, depth)
        queue = deque([(start_path, 1.0, 0)])
        
        # Futures set
        futures = set()
        
        # Results accumulator
        all_items = []
        
        # Progress tracking
        total_progress = 0.0
        scanned_count = 0
        last_update = time.time()
        
        # Scanned paths tracking
        recent_scanned_dirs = []
        
        # Helper to schedule
        def schedule_next():
            while queue and len(futures) < self.max_workers:
                path, weight, depth = queue.popleft()
                # Submit task
                futures.add(self.executor.submit(self._scan_and_process_worker, path, start_path, weight, depth))

        schedule_next()
        
        while (futures or queue) and not self.cancel_event.is_set():
            # Wait for some work to complete
            done, _ = wait(futures, timeout=0.05, return_when=FIRST_COMPLETED)
            
            for f in done:
                futures.remove(f)
                try:
                    subdirs, items, weight, depth, scanned_path = f.result()
                    
                    # Accumulate results
                    if items:
                        all_items.extend(items)
                        scanned_count += len(items)
                    
                    recent_scanned_dirs.append(scanned_path)
                        
                    if callback and items:
                        # Send partial results
                        # Note: We send empty list for items here if we want to batch them? 
                        # Original code sent items immediately.
                        callback(items, {"scanned": scanned_count, "progress": total_progress * 100, "phase": "scanning", "scanned_dirs": []})
                    
                    # Distribute weight or complete it
                    if subdirs and depth < self.max_depth:
                        if len(subdirs) > 0:
                            child_weight = weight / len(subdirs)
                            for d in subdirs:
                                queue.append((d, child_weight, depth + 1))
                    else:
                        # Leaf node (in terms of dirs or recursion limit), this weight is done
                        total_progress += weight
                        
                except Exception as e:
                    print(f"Scan error: {e}")
            
            # Schedule more
            schedule_next()
            
            # Periodic Progress update
            if time.time() - last_update > 0.2:
                 if callback:
                     callback([], {
                         "scanned": scanned_count, 
                         "progress": min(100, int(total_progress * 100)), 
                         "phase": "scanning",
                         "scanned_dirs": list(recent_scanned_dirs)
                     })
                     recent_scanned_dirs = []
                 last_update = time.time()

        if self.cancel_event.is_set():
            for f in futures:
                f.cancel()
            return all_items # Return what we have
            
        # Final update
        if callback:
            callback([], {"scanned": scanned_count, "progress": 100, "phase": "complete", "scanned_dirs": list(recent_scanned_dirs)})
            
        return all_items

    def _scan_and_process_worker(self, path, root_path, weight, depth):
        """
        Scans a directory, processes files therein, returns (subdirs, items, weight, depth, path).
        """
        subdirs = []
        raw_files = []
        
        if self.cancel_event.is_set():
            return [], [], weight, depth, path

        try:
            with os.scandir(path) as entries:
                for entry in entries:
                    if self.cancel_event.is_set():
                        break
                    
                    if entry.is_dir(follow_symlinks=False):
                        if entry.name not in self.ignore_dirs and not entry.name.startswith('.'):
                            subdirs.append(entry.path)
                            # Also add directory as an item
                            try:
                                raw_files.append((entry.path, entry.name, entry.stat(), True)) # True for is_dir
                            except OSError:
                                pass
                    elif entry.is_file():
                        ext = os.path.splitext(entry.name)[1].lower()
                        if ext in self.extensions:
                            try:
                                raw_files.append((entry.path, entry.name, entry.stat(), False)) # False for is_dir
                            except OSError:
                                pass
        except OSError:
            pass
            
        # Process files immediately
        items = self._process_files(raw_files, root_path)
        return subdirs, items, weight, depth, path

    def _process_files(self, raw_files, start_path):
        """
        raw_files: list of (full_path, basename, stat_obj, is_dir)
        """
        path_map = {f[0]: (f[1], f[2]) for f in raw_files} # path -> (name, stat)
        
        final_items = []
        sequence_candidate_paths = []
        
        # Split into sequence candidates and singles
        for p, name, st, is_dir in raw_files:
            if is_dir:
                 final_items.append(self._make_item(p, name, st, start_path, is_directory=True))
                 continue
                 
            ext = os.path.splitext(name)[1].lower()
            if ext in self.non_sequence_extensions:
                # Treat strictly as single file
                final_items.append(self._make_item(p, name, st, start_path))
            else:
                sequence_candidate_paths.append(p)
            
        # Use fileseq to find sequences among candidates
        # Import moved to top level or check self.HAS_FILESEQ? 
        # The file has 'try: import fileseq ...' at top level
        
        sequences = []
        if fileseq and sequence_candidate_paths:
            try:
                sequences = fileseq.findSequencesInList(sequence_candidate_paths)
            except Exception as e:
                sequences = [] # Fallback?

        if not fileseq and sequence_candidate_paths:
             # Fallback: Treat all as singles
             for p in sequence_candidate_paths:
                 info = path_map.get(p)
                 if info:
                    final_items.append(self._make_item(p, info[0], info[1], start_path))

        for seq in sequences:
            # Check if we should explode this sequence (if it's actually versioned files matching config)
            explode = False
            
            # If length is 1, it's virtually a single file, but fileseq wraps it.
            # If length > 1, check if it matches version regex but shouldn't?
            # Existing logic:
            if len(seq) > 1:
                try:
                    # If the basename doesn't match version regex, but one file does??
                    # This logic seems to prevent detecting a sequence if the naming is ambiguous?
                    # Let's keep existing logic but careful.
                    # Actually, if len > 1, it IS a sequence usually.
                    pass
                except Exception as e:
                    pass
            
            if len(seq) == 1:
                 # Treat as single file
                 str_p = str(seq[0])
                 info = path_map.get(str_p)
                 if info:
                     final_items.append(self._make_item(str_p, info[0], info[1], start_path))
                 continue

            # It's a sequence
            max_mtime = 0
            total_size = 0
            valid_seq = True
            
            # Calculate stats
            for p in seq:
                info = path_map.get(str(p))
                if info:
                    st = info[1]
                    if st.st_mtime > max_mtime:
                        max_mtime = st.st_mtime
                    total_size += st.st_size
                else:
                    # Should not happen as we built candidates from map
                    pass
            
            # Retrieve owner from first
            first_path = str(seq[0])
            first_info = path_map.get(first_path)
            owner = self.get_owner(first_info[1].st_uid) if first_info else "?"
            
            # Format name
            try:
                pad = seq.padding()
                if pad == '#': pad = "@@@@"
                elif '#' in pad: pad = "@" * len(pad)
                elif not pad: pad = "@@@@" # Default?
            except:
                pad = "@@@@"
                
            name = f"{seq.basename()}{pad}{seq.extension()}"
                        
            # Create item
            # Use abspath for seq path? 
            # fileseq string representation might be relative if input was relative?
            # input was 'p' from raw_files which is full path.
            
            # fileseq.FileSequence string conversion gives the sequence string (path-#.ext).
            # We want that as 'path'?
            # xstudio expects 'path' to be loadable.
            
            item = {
                "name": name,
                "path": str(seq), # Sequence string path
                "relpath": os.path.relpath(first_path, start_path), # Relative path of ONE file? Or sequence?
                # relpath is used for tree building.
                # If we use first_path, detailed logic might split it.
                # But we want the sequence to appear in the folder.
                # So we should use relation of the FOLDER containing the sequence.
                # relpath logic in QML splits by /.
                # If path is /foo/bar/seq.####.exr. relpath = bar/seq.####.exr.
                # parts = [bar, seq...].
                # This works.
                "type": "Sequence",
                "frames": str(seq.frameRange()),
                "size": total_size,
                "size_str": self.format_size_str(total_size),
                "date": max_mtime,
                "owner": owner,
                "extension": seq.extension(),
                "is_sequence": True,
                "is_folder": False
            }
            # Fix relpath to be based on the abstract sequence path if possible?
            # actually `str(seq)` gives the sequence path.
            # `os.path.relpath(str(seq), start_path)` should work.
            item["relpath"] = os.path.relpath(str(seq), start_path)
            
            final_items.append(item)

        return self._group_versions(final_items)

    def _make_item(self, path, name, st, start_path, is_directory=False):
        return {
            "name": name,
            "path": path,
            "relpath": os.path.relpath(path, start_path),
            "type": "Folder" if is_directory else "File",
            "frames": "" if is_directory else "1",
            "size": 0 if is_directory else st.st_size,
            "size_str": "" if is_directory else self.format_size_str(st.st_size),
            "date": st.st_mtime,
            "owner": self.get_owner(st.st_uid),
            "extension": "" if is_directory else os.path.splitext(name)[1],
            "is_sequence": False,
            "is_folder": is_directory
        }

    def _group_versions(self, items):
        # items is a list of dicts.
        # We want to identify items that are versions of the same thing.
        # Regex: _v(\d+)
        
        # Key: (prefix, suffix) -> [item1, item2, ...]
        groups = {}
        ungrouped = []
        
        for item in items:
            name = item["name"]
            # Apply regex
            match = self.version_regex.search(name)
            if match:
                # Found a version
                v_str = match.group(1)
                v_num = int(v_str)
                
                # remove the version string from name to get the key
                # e.g. shot_v01.exr -> shot_.exr (or similar)
                # We replace the FULL match _v01 with a placeholder or empty
                
                # We need to handle where it is.
                # If we have shot_v1.exr and shot_v2.exr -> Key: shot_.exr
                span = match.span()
                prefix = name[:span[0]]
                suffix = name[span[1]:]
                key = (prefix, suffix)
                
                if key not in groups:
                    groups[key] = []
                
                # Attach version info to item
                item["version"] = v_num
                groups[key].append(item)
            else:
                ungrouped.append(item)
        
        # Now process groups
        # If config says to group, we return a hybrid list
        # We assume we just annotate them for now, or do we structure them?
        # The user said: "group files of a similar basename... by removing version string"
        # "Filter only the highest version"
        
        # If we just adding metadata, we can just return the flat list but with "version_group_id" or something.
        # But for the UI to show "Latest Version", it needs to know which ones are older.
        
        # Let's add "latest_in_group" flag to items?
        # And "group_key".
        
        final_output = list(ungrouped)
        
        for key, group_items in groups.items():
            # Sort by version
            group_items.sort(key=lambda x: x["version"], reverse=True)
            
            # Highest version
            for i, item in enumerate(group_items):
                item["is_latest_version"] = (i == 0)
                item["version_rank"] = i # 0-indexed rank (0 is latest)
                item["version_group"] = str(key)
                final_output.append(item)
                
        # Sort by name
        final_output.sort(key=lambda x: x["name"])
        return final_output

    def stop(self):
        self.cancel_event.set()
