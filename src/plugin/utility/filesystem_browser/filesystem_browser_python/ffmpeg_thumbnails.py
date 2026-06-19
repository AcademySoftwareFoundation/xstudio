import os
import sys
import threading
import queue
import subprocess
import shutil
import pathlib
import tempfile
import uuid as _uuid
import atexit
from collections import OrderedDict

global _dgb

# Try importing fileseq
try:
    import fileseq
    fileseq_available = True
except ImportError:
    fileseq_available = False

def _find_ffmpeg():
    """Find ffmpeg binary. Checks env var, xStudio app bundle, then system PATH."""
    # 1. Explicit override
    env_path = os.environ.get("FFMPEG_PATH")
    if env_path and os.path.isfile(env_path):
        return env_path, None  # (binary, dyld_lib_path)

    # 2. xStudio app bundle (same directory as the main binary)
    exe = sys.argv[0] if sys.argv else ""
    bundle_ffmpeg = os.path.join(os.path.dirname(exe), "ffmpeg")
    if os.path.isfile(bundle_ffmpeg):
        # Bundled ffmpeg needs Frameworks dir on DYLD_LIBRARY_PATH
        frameworks = os.path.join(os.path.dirname(exe), "..", "Frameworks")
        frameworks = os.path.normpath(frameworks)
        return bundle_ffmpeg, frameworks

    # 3. System PATH
    system_ffmpeg = shutil.which("ffmpeg")
    if system_ffmpeg:
        return system_ffmpeg, None

    return None, None

class FFMpegThumbnails:

    def __init__(self, thumb_result_callback, debug_func):

        global _dbg
        _dbg = debug_func
        self.thumb_result_callback = thumb_result_callback
        # Thumbnail setup — ffmpeg-based, no xStudio actor system needed
        self._ffmpeg_bin, self._ffmpeg_dyld = _find_ffmpeg()
        if self._ffmpeg_bin:
            _dbg(f"FilesystemBrowser: using ffmpeg at {self._ffmpeg_bin}")
        else:
            _dbg("FilesystemBrowser: WARNING — ffmpeg not found, thumbnails disabled")
        self._temp_dir = tempfile.mkdtemp(prefix="xstudio_thumbs_")
        self._thumbnail_cache = OrderedDict()  # path -> file:///... thumb URI (LRU, capped)
        self._thumbnail_cache_max = 500
        atexit.register(self._cleanup)
        self._thumb_lock = threading.Lock()
        self._thumb_pending = set()  # paths currently in queue/processing
        self._thumb_queue = queue.Queue()
        # 4 worker threads — daemon so they die with the process
        self._threads = []
        for _ in range(4):
            t = threading.Thread(target=self._thumb_worker_loop, daemon=True)
            t.start()
            self._threads.append(t)

    def _cleanup(self):
        """atexit handler: shut down scanner thread pool and remove temp thumbnail dir."""
        try:
            [self._thumb_queue.put("EXIT") for i in range(0, 10)]
            for t in self._threads:
                t.join()
            shutil.rmtree(self._temp_dir, ignore_errors=True)
        except Exception:
            pass

    def request_thumbnail(self, path):
        """Queue an async thumbnail fetch if not already cached or pending."""
        if path in self._thumbnail_cache:
            # Already done — push the cached URI back to UI immediately
            self.thumb_result_callback(path, self._thumbnail_cache[path])
            return
        with self._thumb_lock:
            if path not in self._thumb_pending:
                self._thumb_pending.add(path)
                self._thumb_queue.put(path)

    def _thumb_worker_loop(self):
        """Daemon worker pulling thumbnail requests from the queue."""
        while True:
            _dbg ("QUEUE SIZE", self._thumb_queue.qsize())
            path = self._thumb_queue.get()
            if path == "EXIT":
                break
            try:
                self._generate_thumbnail(path)
            except Exception as e:
                _dbg(f"WORKER_ERR: {e}")
            finally:
                with self._thumb_lock:
                    self._thumb_pending.discard(path)
                self._thumb_queue.task_done()

    def _resolve_sequence_frame(self, path):
        """Given a fileseq path string, return (concrete_file_path, frame_number).
        For single files, returns (path, 0)."""
        if not fileseq_available:
            return path, 0
        try:
            seq = fileseq.FileSequence(path)
            frames = list(seq.frameSet())
            if len(frames) > 1:
                mid = frames[len(frames) // 2]
                return seq.frame(mid), mid
            elif len(frames) == 1:
                return seq.frame(frames[0]), frames[0]
        except Exception:
            pass
        return path, 0

    def _generate_thumbnail(self, path):
        """Generate a thumbnail JPEG using ffmpeg subprocess."""
        if not self._ffmpeg_bin:
            _dbg(f"GEN_SKIP (no ffmpeg): {path}")
            return

        target_file, _frame = self._resolve_sequence_frame(path)
        _dbg(f"GEN_START: {path} -> {target_file}")

        if not os.path.exists(target_file):
            _dbg(f"GEN_MISSING: {target_file}")
            return

        out_file = os.path.join(self._temp_dir, f"{_uuid.uuid4().hex}.jpg")

        env = os.environ.copy()
        if self._ffmpeg_dyld:
            existing = env.get("DYLD_LIBRARY_PATH", "")
            env["DYLD_LIBRARY_PATH"] = (
                self._ffmpeg_dyld + (":" + existing if existing else "")
            )

        cmd = [
            self._ffmpeg_bin,
            "-y",               # overwrite output file
            "-i", target_file,
            "-vf", "scale=150:-1,format=rgb24",
            "-frames:v", "1",
            "-update", "1",     # allow single-image output
            out_file,
        ]

        _dbg(f"GEN_CMD: {' '.join(cmd)}")
        try:
            result = subprocess.run(
                cmd, env=env,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=30
            )
            if result.returncode == 0 and os.path.exists(out_file):
                thumb_uri = pathlib.Path(out_file).as_uri()
                _dbg(f"GEN_OK: {thumb_uri}")
                # LRU eviction: if cache is at capacity, remove the oldest entry
                # and delete its temp file to reclaim disk space.
                if len(self._thumbnail_cache) >= self._thumbnail_cache_max:
                    _, evicted_uri = self._thumbnail_cache.popitem(last=False)
                    try:
                        evicted_path = pathlib.Path(evicted_uri.replace("file://", "", 1))
                        if evicted_path.exists() and evicted_path.parent.samefile(self._temp_dir):
                            evicted_path.unlink()
                    except Exception:
                        pass
                self._thumbnail_cache[path] = thumb_uri
                self.thumb_result_callback(path, thumb_uri)
            else:
                stderr = result.stderr.decode("utf-8", errors="replace")[-500:]
                _dbg(f"GEN_FAIL (rc={result.returncode}): {stderr}")
        except subprocess.TimeoutExpired:
            _dbg(f"GEN_TIMEOUT: {target_file}")
        except Exception as exc:
            _dbg(f"GEN_EXCEPTION: {exc}")

