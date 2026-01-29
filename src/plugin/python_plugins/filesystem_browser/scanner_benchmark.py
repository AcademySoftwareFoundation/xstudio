# This is a test of the scanner. Its not part of the plugin, but
# its used to test the performance of the scanner against real world data.

import os
import shutil
import tempfile
import unittest
import json
from scanner import FileScanner
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("path", help="Path to scan")
parser.add_argument("--threads", type=int, default=4, help="Number of threads to use")
args = parser.parse_args()

if not os.path.exists(args.path):
    print(f"Path {args.path} does not exist")
    exit(1)

scanner = FileScanner(config={"thread_count": args.threads})

def callback(results, progress_info):
    print(progress_info, len(results))

scanner.scan(args.path, callback=callback)

