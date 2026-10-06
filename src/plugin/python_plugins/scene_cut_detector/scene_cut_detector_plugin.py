#!/usr/bin/env python
# SPDX-License-Identifier: Apache-2.0
"""Scene Cut Detector plugin.

Adds a "Detect Scene Cuts" command to the playlist media-item context menu
(and, as a fallback, to the currently selected Clip on an open Timeline).
It analyses the source video for hard scene cuts and builds a new Timeline
in the current Playlist where the source has been split into one sub-clip
per detected scene.

Detection is done by piping downscaled grayscale frames out of ffmpeg and
looking for large frame-to-frame differences (numpy). This avoids relying
on compiled Python video-decoding extensions (e.g. opencv-python, PyAV):
those wheels' native DLLs failed to load in testing on this machine, while
a plain ffmpeg.exe subprocess + numpy worked fine.

Requires 'ffmpeg' (and 'ffprobe') to be available on PATH, e.g.:
    winget install Gyan.FFmpeg
"""

import shutil
import subprocess
from pathlib import Path

import numpy as np

from xstudio.plugin import PluginBase
from .detector import DETECTOR_OPTIONS, compute_cut_frames, uri_to_local_path

# path to QML resources relative to this .py file
qml_folder_name = "qml/SceneCutDetector.1"

dialog_qml = """
SceneCutDetectorDialog {
}
"""

# Frames are downscaled to this size before comparison - big enough to
# catch real cuts, small enough to decode/compare quickly.
_ANALYSIS_WIDTH = 64
_ANALYSIS_HEIGHT = 36
_ANALYSIS_FRAME_BYTES = _ANALYSIS_WIDTH * _ANALYSIS_HEIGHT


class MissingFFmpegError(Exception):
    """Raised when the 'ffmpeg' executable can't be found on PATH."""


class SceneCutDetector(PluginBase):
    """Split a media item into sub-clips at each detected scene cut."""

    def __init__(self, connection):

        PluginBase.__init__(
            self,
            connection,
            name="SceneCutDetector",
            qml_folder=qml_folder_name,
        )

        # User-facing settings, remembered as preferences between sessions,
        # and exposed as a group so our settings dialog QML can bind to them
        # (same mechanism AnnotationsExporter uses for its export dialog).
        self.detector_type = self.add_attribute(
            attribute_name="Detector",
            attribute_value=DETECTOR_OPTIONS[0],
            attribute_role_data={
                "combo_box_options": DETECTOR_OPTIONS,
                "combo_box_options_enabled": [True, True],
            },
            register_as_preference=True,
        )

        self.threshold = self.add_attribute(
            attribute_name="Cut Threshold",
            attribute_value=27.0,
            register_as_preference=True,
        )

        self.min_scene_len = self.add_attribute(
            attribute_name="Minimum Scene Length (frames)",
            attribute_value=15,
            register_as_preference=True,
        )

        for attr in (self.detector_type, self.threshold, self.min_scene_len):
            attr.expose_in_ui_attrs_group("scene_cut_detector_attrs")

        # Single menu item - opens the settings dialog, which has its own
        # "Detect Scene Cuts" button to trigger the run.
        self.menu_id = self.insert_menu_item(
            menu_model_name="media_list_menu_",
            menu_text="Detect Scene Cuts...",
            menu_path="",
            menu_item_position=20.0,
            callback=self.menu_callback,
        )

        self.connect_to_ui()

    # ------------------------------------------------------------------
    # Menu handling
    # ------------------------------------------------------------------

    def menu_callback(self):
        """Open the settings dialog for the currently selected media."""

        media = self._selected_media()
        if media is None:
            self.popup_message_box(
                "Detect Scene Cuts",
                "Select a single video media item (in a Playlist, or a Clip "
                "on a Timeline) before running scene cut detection.",
            )
            return

        self.create_qml_item(dialog_qml)

    def run_from_dialog(self):
        """Called from the settings dialog's "Detect Scene Cuts" button.

        Runs synchronously (blocking the UI) - see NOTE in
        _detect_and_build_timeline for why this can't use a background
        thread. Returns [True, message] on success, or an error string,
        matching the convention AnnotationsExportDialog.qml expects.
        """

        media = self._selected_media()
        if media is None:
            return "No media selected."

        try:
            return self._detect_and_build_timeline(
                media,
                self.detector_type.value(),
                self.threshold.value(),
                int(self.min_scene_len.value()),
            )
        except Exception as exc:  # noqa: BLE001 - surfaced to the dialog
            return str(exc)

    def _selected_media(self):
        """Resolve the media the user wants to analyse.

        Looks first at the playlist media-list selection, then falls back to
        the selected Clip on the currently inspected Timeline.
        """

        selected = self.connection.api.session.selected_media
        if selected:
            return selected[0]

        container = self.connection.api.session.inspected_container
        selection = getattr(container, "selection", None)
        if selection:
            clip = selection[0]
            return getattr(clip, "media", None)

        return None

    # ------------------------------------------------------------------
    # Detection + timeline construction
    # ------------------------------------------------------------------

    def _detect_and_build_timeline(self, media, detector_name, threshold, min_scene_len):
        """Returns [True, message] on success. Raises on failure - the
        caller (run_from_dialog) turns exceptions into an error string for
        the settings dialog to display.
        """
        try:
            file_path = self._resolve_media_path(media)
            cut_frames = self._detect_cuts(file_path, detector_name, threshold, min_scene_len)

            if not cut_frames:
                return [True, "No scene cuts were detected in \"{}\".".format(media.name)]

            scene_count = self._build_timeline(media, cut_frames)

            return [
                True,
                "Created a new Timeline with {} scene(s) for \"{}\".".format(
                    scene_count, media.name
                ),
            ]
        except MissingFFmpegError:
            raise RuntimeError(
                "'ffmpeg' (and 'ffprobe') could not be found on PATH. "
                "Install it with, e.g.: winget install Gyan.FFmpeg"
            )
        except Exception:  # noqa: BLE001 - log full traceback, then re-raise
            import traceback
            traceback.print_exc()
            raise

    def _resolve_media_path(self, media):
        """Get a local filesystem path for the selected media's current source."""
        source = media.media_source()
        media_ref = source.media_reference
        if media_ref is None:
            raise RuntimeError("Selected media has no resolvable media reference.")

        file_path = uri_to_local_path(str(media_ref.uri()))
        if not Path(file_path).is_file():
            raise RuntimeError("Media file not found on disk: {}".format(file_path))

        return file_path

    def _detect_cuts(self, file_path, detector_name, threshold, min_scene_len):
        """Decode downscaled grayscale frames via ffmpeg and delegate to
        compute_cut_frames() for the actual difference/threshold logic.

        Returns a sorted list of 0-based frame numbers (in source decode
        order) where a new scene starts.
        """
        if shutil.which("ffmpeg") is None:
            raise MissingFFmpegError()

        cmd = [
            "ffmpeg",
            "-v", "error",
            "-i", file_path,
            "-vf", "scale={}:{}".format(_ANALYSIS_WIDTH, _ANALYSIS_HEIGHT),
            "-pix_fmt", "gray",
            "-f", "rawvideo",
            "-",
        ]

        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        frame_count = 0

        def read_frames():
            nonlocal frame_count
            while True:
                raw = proc.stdout.read(_ANALYSIS_FRAME_BYTES)
                if len(raw) < _ANALYSIS_FRAME_BYTES:
                    break
                frame_count += 1
                yield np.frombuffer(raw, dtype=np.uint8)

        try:
            cut_frames = compute_cut_frames(read_frames(), detector_name, threshold, min_scene_len)
        finally:
            proc.stdout.close()
            stderr = proc.stderr.read()
            proc.wait()

        if proc.returncode != 0 and frame_count == 0:
            raise RuntimeError(
                "ffmpeg failed to decode \"{}\":\n{}".format(
                    file_path, stderr.decode(errors="replace")
                )
            )

        return cut_frames

    def _build_timeline(self, media, cut_frames):
        # NOTE: track.insert_clip() (and other helpers like create_video_track,
        # insert_gap) call Connection.remote_spawn(), which only works over a
        # genuine external/remote Connection - it fails with "Failed to spawn
        # remote actor" when called via a resident plugin's own in-process
        # connection (confirmed by testing: identical code succeeds from an
        # external script using its own Connection(auto_connect=True), but
        # fails via self.connection here). So we open our own loopback
        # connection to do the actor-spawning parts of timeline construction.
        from xstudio.connection import Connection as _Connection

        helper = _Connection(auto_connect=True)

        helper_media = helper.api.session.selected_media
        if not helper_media:
            raise RuntimeError("Media selection was lost before the timeline could be built.")
        helper_media = helper_media[0]

        playlist = helper.api.session.inspected_container
        # Fall back to the first playlist if the inspected container isn't
        # a playlist (e.g. user already had a Timeline open).
        if not hasattr(playlist, "create_timeline"):
            playlist = helper.api.session.playlists[0]

        # Build the timeline with its default track(s) (with_tracks=True)
        # rather than manually spawning a Track and inserting it into the
        # stack ourselves - the latter reliably hung/crashed xstudio when
        # tested directly (a Clip couldn't be inserted into a manually
        # constructed track). A Timeline also keeps its own internal media
        # registry, so the source media must be added to the timeline (via
        # add_media) before a Clip referencing it can be inserted.
        _uuid, timeline = playlist.create_timeline(
            name="{} - Scene Cuts".format(media.name),
            with_tracks=True,
        )

        timeline.add_media(helper_media)

        track = timeline.video_tracks[0]
        clip = track.insert_clip(helper_media)

        # NOTE: use track.split(frame) (absolute track-position split), not
        # track.split_child(clip, frame). After the first split, the
        # original `clip` object only spans the first sub-range, so a later
        # (larger) absolute frame number is no longer valid relative to it -
        # that raised "Invalid frame to split on" when tested. track.split()
        # looks up the item at that absolute frame itself, so it stays
        # correct across repeated splits.
        for frame in sorted(cut_frames):
            track.split(frame)

        for index, scene_clip in enumerate(track.clips, start=1):
            scene_clip.item_name = "Scene {:03d}".format(index)

        return len(track.clips)


def create_plugin_instance(connection):
    """Create and return a plugin instance."""
    return SceneCutDetector(connection)
