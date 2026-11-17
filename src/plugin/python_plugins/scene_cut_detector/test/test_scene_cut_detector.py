# SPDX-License-Identifier: Apache-2.0
import sys
from pathlib import Path

# detector.py has no xstudio dependency, but it lives inside the plugin
# package (whose __init__.py does import xstudio). Importing it as a
# top-level module (rather than via the scene_cut_detector package) avoids
# pulling in that unrelated, app-only dependency just to run these tests.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import numpy as np

from detector import DETECTOR_OPTIONS, compute_cut_frames, uri_to_local_path


def test_uri_to_local_path_handles_xstudio_windows_uri():
    assert uri_to_local_path("file://localhost//C:/Users/test/clip.mov") == "C:/Users/test/clip.mov"


def test_uri_to_local_path_handles_posix_uri():
    assert uri_to_local_path("file:///tmp/clip.mov") == "/tmp/clip.mov"


def test_content_detector_finds_large_frame_change():
    frames = [
        np.zeros((2, 2), dtype=np.uint8),
        np.zeros((2, 2), dtype=np.uint8),
        np.full((2, 2), 255, dtype=np.uint8),
        np.full((2, 2), 255, dtype=np.uint8),
    ]

    assert compute_cut_frames(frames, DETECTOR_OPTIONS[0], 27.0, 1) == [2]


def test_minimum_scene_length_suppresses_nearby_cuts():
    frames = [
        np.zeros((2, 2), dtype=np.uint8),
        np.full((2, 2), 255, dtype=np.uint8),
        np.zeros((2, 2), dtype=np.uint8),
        np.full((2, 2), 255, dtype=np.uint8),
    ]

    assert compute_cut_frames(frames, DETECTOR_OPTIONS[0], 27.0, 3) == [1]


def test_constant_frames_have_no_cuts():
    frames = [np.full((2, 2), 42, dtype=np.uint8) for _ in range(10)]

    assert compute_cut_frames(frames, DETECTOR_OPTIONS[0], 27.0, 1) == []
