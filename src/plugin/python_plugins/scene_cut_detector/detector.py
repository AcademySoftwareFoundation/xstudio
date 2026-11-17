# SPDX-License-Identifier: Apache-2.0
"""Pure helpers for Scene Cut Detector."""

from urllib.parse import unquote

import numpy as np

DETECTOR_OPTIONS = ["Content (fast cuts)", "Adaptive (camera motion)"]


def uri_to_local_path(uri_str):
    """Convert an xSTUDIO file URI to a local filesystem path."""
    path_part = uri_str
    if path_part.startswith("file://"):
        path_part = path_part[len("file://"):]
        if path_part.startswith("localhost"):
            path_part = path_part[len("localhost"):]
    elif path_part.startswith("file:"):
        path_part = path_part[len("file:"):]

    path_part = unquote(path_part).lstrip("/")
    if len(path_part) > 1 and path_part[1] == ":":
        return path_part
    return "/" + path_part


def compute_cut_frames(frames, detector_name, threshold, min_scene_len):
    """Return frame numbers where consecutive frames indicate a scene cut."""
    cut_frames = []
    recent_diffs = []
    previous_frame = None
    frame_index = 0
    last_cut_frame = -min_scene_len

    for frame in frames:
        frame = np.asarray(frame, dtype=np.float32)

        if previous_frame is not None:
            diff = float(np.abs(frame - previous_frame).mean())

            if detector_name == DETECTOR_OPTIONS[1] and recent_diffs:
                rolling_mean = sum(recent_diffs) / len(recent_diffs)
                is_cut = diff > threshold and diff > 3.0 * max(rolling_mean, 1.0)
            else:
                is_cut = diff > threshold

            if is_cut and (frame_index - last_cut_frame) >= min_scene_len:
                cut_frames.append(frame_index)
                last_cut_frame = frame_index

            recent_diffs.append(diff)
            if len(recent_diffs) > 15:
                recent_diffs.pop(0)

        previous_frame = frame
        frame_index += 1

    return cut_frames
