"""
audio_out.py — ALSA audio sink via sounddevice.

Routes demodulated PCM audio to the Pi's 3.5mm jack (or whatever
ALSA device is the system default).  Runs a background thread that
drains a queue of float32 blocks.

Usage
─────
    sink = AudioSink(sample_rate=12_000)
    sink.start()
    sink.write(audio_block)   # float32 numpy array, values –1…1
    sink.stop()
"""

import threading
import queue
import numpy as np


class AudioSink:
    """
    Non-blocking audio sink.  write() is safe to call from any thread.
    Audio is played by a dedicated output thread to keep latency low.
    """

    QUEUE_MAXSIZE = 8      # blocks; 8 × 1024 samples @ 12 ksps ≈ 0.7 s buffer
    BLOCK_SIZE    = 1024   # frames per output chunk

    def __init__(self, sample_rate: int = 12_000, device=None):
        self._rate    = sample_rate
        self._device  = device     # None = ALSA default
        self._q: queue.Queue = queue.Queue(maxsize=self.QUEUE_MAXSIZE)
        self._thread: threading.Thread | None = None
        self._running = False

    # ── Public API ────────────────────────────────────────────────────────────

    def start(self) -> None:
        """Open the audio stream and start the playback thread."""
        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True,
                                        name="AudioSink")
        self._thread.start()

    def stop(self) -> None:
        """Signal the playback thread to stop and wait for it."""
        self._running = False
        self._q.put(None)   # sentinel to unblock the thread
        if self._thread:
            self._thread.join(timeout=2)
        self._thread = None

    def write(self, audio: np.ndarray) -> None:
        """
        Enqueue an audio block.  Drops the block silently if the queue is
        full (prevents DSP thread from stalling on a slow audio device).
        """
        try:
            self._q.put_nowait(audio.astype(np.float32))
        except queue.Full:
            pass   # drop — better than blocking the receiver thread

    def flush(self) -> None:
        """Discard buffered audio (call on retune to kill old audio)."""
        while not self._q.empty():
            try:
                self._q.get_nowait()
            except queue.Empty:
                break

    # ── Internal ──────────────────────────────────────────────────────────────

    def _loop(self) -> None:
        """
        Playback thread body.

        sounddevice is imported here (not at module level) so the rest of
        the codebase can import audio_out without PortAudio being installed
        — useful for running tests on a dev machine without audio hardware.
        """
        try:
            import sounddevice as sd
        except ImportError:
            print("[AudioSink] sounddevice not installed — audio disabled")
            return
        except Exception as e:
            print(f"[AudioSink] Failed to import sounddevice: {e}")
            return

        try:
            with sd.OutputStream(
                    samplerate=self._rate,
                    channels=1,
                    dtype='float32',
                    blocksize=self.BLOCK_SIZE,
                    device=self._device,
            ) as stream:
                print(f"[AudioSink] Opened ALSA stream @ {self._rate} Hz")
                while self._running:
                    block = self._q.get()
                    if block is None:           # stop sentinel
                        break
                    stream.write(block)
        except Exception as e:
            print(f"[AudioSink] Stream error: {e}")