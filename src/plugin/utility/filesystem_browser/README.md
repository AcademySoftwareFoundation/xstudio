# Filesystem Browser Plugin for xStudio

A high-performance, multi-threaded filesystem browser for xStudio, designed to handle large directories and image sequences efficiently.

## Features

- **Fast Multi-threaded Scanning**: Uses a thread pool and BFS algorithm to scan directories quickly without freezing the UI.
- **Image Sequence Detection**: Automatically detects and groups file sequences (e.g., `shot_001.1001.exr` -> `shot_001.####.exr`). Supports exclusion of specific extensions (e.g., `.mov`, `.mp4`) via configuration.
- **Smart Filtering**:
  - **Text Filter**: Supports "AND" logic (space-separated terms). E.g., `comp exr` finds files matchings both "comp" and "exr".
  - **Time Filter**: Filter by modification time (Last 1 day, 1 week, etc.).
  - **Version Filter**: Filter to show only the latest version or latest 2 versions of a shot.
- **Navigation**:
  - Native Directory Picker integration.
  - Path completion/suggestions.
  - History tracking (via sticky attributes).
- **Playback Integration**:
  - **Double-Click**: Loads media and immediately starts playback using the playlist's playhead logic.
  - **Context Menu**: 
    - **Replace**: Replaces the currently viewed media with the selected item.
    - **Compare with**: Loads the selected item and sets up an A/B comparison with the current media.

## Usage

1.  **Open the Browser**: 
    - Go to `View` -> `Panels` -> `Filesystem Browser`.
    - Or use the hotkey **'B'**.
2.  **Navigation**:
    - Enter a path in the text field or click the folder icon to browse.
    - **Double-click** a folder to navigate into it.
    - **Quick Access (▼)**: Click the arrow next to the path field to open the Quick Access list.
      - **History**: Shows recently visited directories.
      - **Pinned**: Shows your pinned locations for easy access.
      - **Pinning**: Click the "Pin" icon (📌) next to any item to pin or unpin it. Pinned items appear at the top in gold.

## Configuration

### Environment Variables

- `XSTUDIO_BROWSER_PINS`: Pre-define a list of pinned directories.
  - Format: JSON list of objects or simple path string (colon-separated on Unix, semicolon on Windows).
  - Example (JSON): `'[{"name": "Show", "path": "/jobs/show"}, "/home/user"]'`
  - Example (Simple): `/jobs/show:/home/user`

3.  **Loading Media**:
    - **Double-click** a file/sequence to load it into the current or new playlist.
    - **Right-click** for advanced actions (Replace, Compare).

## Logic & Performance

- **Scanning**: The scanner runs in a background thread, reporting partial results to the UI to keep it responsive. 
- **Sequences**: Uses the `fileseq` library (for robust sequence parsing.

## Testing

A benchmark script is included to test the scanner performance:

```bash
python scanner_benchmark.py --threads 2 /shots/MYSHOW/MYSHOT
```

This allows you to test the scanning performance at different thread speeds for the specified directory.

```bash
python test_scanner.py
```
Unit test for scanner.

