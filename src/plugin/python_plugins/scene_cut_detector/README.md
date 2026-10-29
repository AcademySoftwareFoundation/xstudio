# Scene Cut Detector

This experimental Python plugin detects hard scene cuts in a selected video and creates a new xSTUDIO Timeline with one sub-clip per detected scene.

## Requirements

- A Windows xSTUDIO build with the Python plugin support enabled.
- `ffmpeg.exe` available on `PATH`.
- `numpy` available to xSTUDIO's bundled Python environment.

The plugin intentionally uses an `ffmpeg` subprocess instead of `opencv-python` or PyAV. This keeps the runtime dependency small and avoids relying on compiled Python video-decoding wheels.

## Usage

1. Start the development build with `build/run_xstudio.bat`.
2. Add a video to a Playlist and select it.
3. Right-click the media item and choose **Detect Scene Cuts...**.
4. Choose a detector mode, threshold, and minimum scene length.
5. Click **Detect Scene Cuts**.

The result is a new Timeline in the inspected Playlist. The source media is registered on that Timeline and split at the detected frame numbers.

## Parameters

- **Content (fast cuts)** compares each frame with the previous frame using a fixed mean-pixel-difference threshold.
- **Adaptive (camera motion)** compares a frame against a rolling average of recent differences, which can help when the source contains camera movement.
- **Cut Threshold** controls how large a frame change must be. Lower values detect more changes; higher values reduce false positives.
- **Minimum Scene Length (frames)** prevents cuts from being placed too close together.

Detection currently runs synchronously, so the xSTUDIO UI may be unresponsive while a long source is scanned. This is a known limitation of the first version.

## Development

The pure detection and URI parsing helpers are in `detector.py`. They can be tested without a running xSTUDIO session:

```powershell
python -m pytest src/plugin/python_plugins/scene_cut_detector/test/test_scene_cut_detector.py
```

For a development build, redeploy the Python plugin after editing:

```powershell
cmake --build build --target COPY_PY_PLUGIN_scene_cut_detector
```

Then restart `build/run_xstudio.bat -n`.

## Review status

This is an initial contribution under review. Known follow-up work includes background processing with a thread-safe xSTUDIO API boundary, richer detector options, timeline/audio handling, and packaging/runtime discovery of `ffmpeg`.
