
import os
import shutil
import tempfile
import unittest
import json
from scanner import FileScanner

class TestFileScanner(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        
    def tearDown(self):
        shutil.rmtree(self.test_dir)
        
    def create_file(self, filename, content="test"):
        path = os.path.join(self.test_dir, filename)
        with open(path, "w") as f:
            f.write(content)
        return path

    def test_basic_scan(self):
        self.create_file("test.mov")
        self.create_file("test.png")
        self.create_file("ignore.txt") # not in extensions
        
        scanner = FileScanner()
        results = scanner.scan(self.test_dir)
        
        names = [r["name"] for r in results]
        self.assertIn("test.mov", names)
        self.assertIn("test.png", names)
        self.assertNotIn("ignore.txt", names)

    def test_version_grouping(self):
        # Create versions
        self.create_file("shot_v01.mov")
        self.create_file("shot_v02.mov") # Newer
        self.create_file("other_v01.mov")
        
        scanner = FileScanner()
        results = scanner.scan(self.test_dir)
        
        names = [r["name"] for r in results]
        print(f"DEBUG: Found files: {names}")
        
        # Check flags
        shot_v02 = next((r for r in results if r["name"] == "shot_v02.mov"), None)
        shot_v01 = next((r for r in results if r["name"] == "shot_v01.mov"), None)
        
        if not shot_v02:
             self.fail(f"shot_v02.mov not found in {names}")
        
        self.assertTrue(shot_v02.get("is_latest_version"))
        self.assertFalse(shot_v01.get("is_latest_version"))
        
        self.assertEqual(shot_v02.get("version"), 2)
        self.assertEqual(shot_v01.get("version"), 1)
        self.assertEqual(shot_v02.get("version_group"), shot_v01.get("version_group"))

    def test_ignore_dirs(self):
        os.makedirs(os.path.join(self.test_dir, ".git"))
        self.create_file(".git/config")
        
        scanner = FileScanner()
        results = scanner.scan(self.test_dir)
        self.assertEqual(len(results), 0)

    def test_callback(self):
        self.create_file("test.mov")
        os.makedirs(os.path.join(self.test_dir, "subdir"))
        self.create_file("subdir/test2.mov")
        
        scanner = FileScanner()
        
        callback_data = []
        def cb(items, progress):
            callback_data.append((items, progress))
            
        results = scanner.scan(self.test_dir, callback=cb)
        
        self.assertTrue(len(callback_data) > 0)
        # Check if we got progress updates
        progress_values = [d[1]["progress"] for d in callback_data]
        self.assertIn(100, progress_values)
        self.assertEqual(len(results), 2)

    def test_exclusion(self):
        # Create files that LOOK like a sequence but have excluded extension
        self.create_file("clip.1001.mov")
        self.create_file("clip.1002.mov")
        
        # And some that SHOULD be a sequence
        self.create_file("render.1001.exr")
        self.create_file("render.1002.exr")
        
        # Configure scanner to exclude .mov (default)
        scanner = FileScanner() # Defaults include .mov in non_sequence_extensions
        results = scanner.scan(self.test_dir)
        
        names = [r["name"] for r in results]
        
        # .mov should be individual
        self.assertIn("clip.1001.mov", names)
        self.assertIn("clip.1002.mov", names)
        
        # .exr should be a sequence
        # The name might be "render.####.exr" or similar depending on how fileseq formats it
        # Let's check for the sequence type or name pattern
        seq_items = [r for r in results if r["type"] == "Sequence"]
        self.assertTrue(any("render" in r["name"] for r in seq_items))
        
        mov_items = [r for r in results if "clip" in r["name"]]
        for item in mov_items:
            self.assertEqual(item["type"], "File")
