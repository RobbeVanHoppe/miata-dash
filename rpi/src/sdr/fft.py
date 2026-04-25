"""
fft.py — Rolling power spectrum for the dashboard waterfall.

Accumulates IQ samples into a ring buffer, computes an FFT of the
latest N samples on demand, and returns magnitude bins in dBFS.

Usage
─────
    fft = FFTEngine(fft_size=1024)
    fft.push(iq_block)          # call from the receiver thread
    bins = fft.compute()        # call from the WebSocket thread (~10 Hz)
    # bins is a list[float] of length fft_size, values in dBFS (–120…0)
"""

import numpy as np
from collections import deque


class FFTEngine:
    """Thread-safe(-ish) rolling FFT.

    The receiver thread pushes IQ blocks; the WS thread pulls computed
    bins.  We use a simple deque as the ring buffer — Python's GIL makes
    deque.append / deque rotation atomic enough for our purposes.
    """

    def __init__(self, fft_size: int = 1024, smoothing: float = 0.7):
        """
        fft_size   — number of FFT bins (power of two works best)
        smoothing  — exponential moving average coefficient (0 = no smoothing,
                     0.9 = very smooth/laggy).  0.7 is a good UI default.
        """
        self._size      = fft_size
        self._smoothing = smoothing
        self._buf: deque = deque(maxlen=fft_size)
        self._prev: np.ndarray | None = None
        self._window: np.ndarray = np.hanning(fft_size).astype(np.float32)

    def push(self, iq: np.ndarray) -> None:
        """Append IQ samples to the ring buffer. Thread-safe via GIL."""
        self._buf.extend(iq)

    def compute(self) -> list[float]:
        """
        Compute power spectrum from the current ring buffer contents.
        Returns fft_size magnitude bins in dBFS, frequency-ordered
        (i.e. DC in the centre, lower sideband left, upper right).
        """
        buf = self._buf
        n   = self._size

        if len(buf) < n:
            # Not enough samples yet — return noise floor
            return [-120.0] * n

        # Grab the latest n samples as a numpy array
        samples = np.array(list(buf)[-n:], dtype=np.complex64)

        # Apply Hann window
        samples *= self._window

        # FFT → power spectrum in dBFS
        spectrum = np.fft.fft(samples, n=n)
        spectrum  = np.fft.fftshift(spectrum)          # DC centred
        power_db  = 20 * np.log10(np.abs(spectrum) / n + 1e-10)

        # Smooth over time to avoid jitter
        if self._prev is not None:
            power_db = (self._smoothing * self._prev
                        + (1 - self._smoothing) * power_db)
        self._prev = power_db

        return power_db.tolist()

    def reset(self) -> None:
        """Clear the buffer (called on retune to drop stale samples)."""
        self._buf.clear()
        self._prev = None