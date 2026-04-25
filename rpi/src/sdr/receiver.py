"""
receiver.py — RTL-SDR sample loop, squelch, and hot-retune.

One instance of SDRReceiver should exist for the lifetime of the Flask
process.  It owns the rtlsdr device handle, runs a sample-reading thread,
pipes IQ through the demodulator, sends audio to the AudioSink, and feeds
the FFTEngine.

Hot retune
──────────
Call retune(freq_hz, mode, squelch) at any time.  The sample thread picks
up the new parameters on the next read cycle without restarting.

Squelch
───────
Squelch is implemented as a simple power gate: if the mean signal power
(dB) of a block is below the threshold, the block is not sent to audio.
Set squelch=0 to open (disable) squelch.

Thread model
────────────
  receiver thread (daemon) — reads USB, demodulates, writes audio queue
  caller thread            — calls retune() / stop() / state()
  WS thread                — calls fft.compute() independently
"""

import threading
import time
import numpy as np

from src.sdr.demod import demodulate, CAPTURE_RATE
from src.sdr.fft import FFTEngine
from src.sdr.audio_out import AudioSink
from src.sdr.presets import FREQ_MIN_HZ, FREQ_MAX_HZ, DEFAULT_PRESET

READ_SIZE    = 256 * 1024      # bytes per USB read (128 k complex samples)
SAMPLE_COUNT = READ_SIZE // 2  # complex64: 2 bytes per component


class SDRReceiver:
    """Manages one RTL-SDR dongle end-to-end."""

    def __init__(self):
        self._lock    = threading.Lock()
        self._thread: threading.Thread | None = None
        self._running = False

        # Tuner state (protected by _lock for reads, written atomically)
        self._freq_hz: int   = DEFAULT_PRESET.freq_hz
        self._mode:    str   = DEFAULT_PRESET.mode
        self._squelch: float = DEFAULT_PRESET.squelch

        # Pending retune flag — set by retune(), cleared by the sample thread
        self._retune_pending = threading.Event()

        self.fft   = FFTEngine(fft_size=1024, smoothing=0.7)
        self._sink = AudioSink(sample_rate=12_000)

    # ── Public API ────────────────────────────────────────────────────────────

    def start(self) -> None:
        """Open the dongle and start the sample loop."""
        if self._running:
            return
        self._running = True
        self._sink.start()
        self._thread = threading.Thread(target=self._loop, daemon=True,
                                        name="SDRReceiver")
        self._thread.start()
        print(f"[SDRReceiver] Started — {self._freq_hz/1e6:.3f} MHz {self._mode.upper()}")

    def stop(self) -> None:
        """Stop the sample loop and release the dongle."""
        self._running = False
        self._retune_pending.set()   # unblock any wait
        if self._thread:
            self._thread.join(timeout=3)
        self._sink.stop()
        print("[SDRReceiver] Stopped")

    def retune(self, freq_hz: int | None = None,
               mode:    str   | None = None,
               squelch: float | None = None) -> dict:
        """
        Update frequency, mode, and/or squelch without restarting the thread.
        Returns the new effective state.
        """
        with self._lock:
            if freq_hz is not None:
                self._freq_hz = max(FREQ_MIN_HZ,
                                    min(FREQ_MAX_HZ, int(freq_hz)))
            if mode is not None:
                if mode not in ("wfm", "am", "nfm"):
                    raise ValueError(f"Invalid mode '{mode}'")
                self._mode = mode
            if squelch is not None:
                self._squelch = float(squelch)

        self._retune_pending.set()
        self._sink.flush()
        self.fft.reset()
        return self.state()

    def state(self) -> dict:
        """Snapshot of current tuner state (safe to call from any thread)."""
        with self._lock:
            return {
                "running":  self._running,
                "freq_hz":  self._freq_hz,
                "freq_mhz": round(self._freq_hz / 1e6, 3),
                "mode":     self._mode,
                "squelch":  self._squelch,
            }

    # ── Sample loop ───────────────────────────────────────────────────────────

    def _loop(self) -> None:
        """Receiver thread body."""
        try:
            import rtlsdr
        except ImportError:
            print("[SDRReceiver] pyrtlsdr not installed — using mock IQ")
            self._mock_loop()
            return

        sdr = None
        try:
            sdr = rtlsdr.RtlSdr()
            self._apply_tune(sdr)
            self._retune_pending.clear()

            while self._running:
                # Hot retune if flagged
                if self._retune_pending.is_set():
                    self._apply_tune(sdr)
                    self._retune_pending.clear()

                # Read a block of raw IQ samples
                raw = sdr.read_samples(SAMPLE_COUNT)   # returns complex64
                iq  = np.array(raw, dtype=np.complex64)

                # Feed FFT (always — even if squelched)
                self.fft.push(iq)

                # Squelch gate — skip demod/audio if signal is too weak
                power_db = 10 * np.log10(np.mean(np.abs(iq) ** 2) + 1e-10)
                with self._lock:
                    sq = self._squelch
                    mode = self._mode
                if sq > 0 and power_db < sq:
                    continue

                # Demodulate
                with self._lock:
                    mode = self._mode
                try:
                    audio = demodulate(iq, mode)
                    self._sink.write(audio)
                except Exception as e:
                    print(f"[SDRReceiver] Demod error: {e}")

        except Exception as e:
            print(f"[SDRReceiver] Fatal error: {e}")
        finally:
            if sdr:
                try:
                    sdr.close()
                except Exception:
                    pass
            print("[SDRReceiver] Thread exited")

    def _apply_tune(self, sdr) -> None:
        """Push current freq/rate/gain settings to the hardware."""
        with self._lock:
            freq = self._freq_hz
        sdr.sample_rate    = CAPTURE_RATE
        sdr.center_freq    = freq
        sdr.gain           = 'auto'
        print(f"[SDRReceiver] Tuned → {freq/1e6:.3f} MHz  rate={CAPTURE_RATE/1e3:.0f}ksps")

    # ── Mock loop (no dongle) ─────────────────────────────────────────────────

    def _mock_loop(self) -> None:
        """
        Synthetic IQ source for development without a dongle.
        Produces a carrier + noise so the FFT and demod paths can be tested.
        """
        print("[SDRReceiver] Running in MOCK mode (no dongle)")
        phase = 0.0
        while self._running:
            if self._retune_pending.is_set():
                self.fft.reset()
                self._retune_pending.clear()

            t    = np.arange(SAMPLE_COUNT, dtype=np.float32) / CAPTURE_RATE
            with self._lock:
                freq_offset = 0.0   # carrier at DC (i.e. at centre freq)
            carrier = np.exp(1j * (2 * np.pi * freq_offset * t + phase)).astype(np.complex64)
            noise   = (np.random.randn(SAMPLE_COUNT) + 1j * np.random.randn(SAMPLE_COUNT)).astype(np.complex64) * 0.05
            iq      = carrier + noise
            phase   = float(np.angle(carrier[-1]))   # maintain phase continuity

            self.fft.push(iq)
            time.sleep(SAMPLE_COUNT / CAPTURE_RATE)