"""
demod.py — Numpy-only demodulators for WFM, AM, and NFM.

All functions share the same signature:
    demodulate(iq: np.ndarray, sample_rate: int) -> np.ndarray (float32, –1…1)

The returned audio is at AUDIO_RATE (12 000 Hz) and is ready to hand
directly to sounddevice.

Pipeline per mode
─────────────────
Input: raw IQ at CAPTURE_RATE (240 000 sps, complex64)

WFM  (broadcast FM, ±75 kHz deviation):
  1. Decimate 5×  → 48 ksps
  2. FM discriminator (angle diff)
  3. Decimate 4×  → 12 ksps  (audio out)
  4. Clip + normalise

AM (airband, envelope detection):
  1. Decimate 5×  → 48 ksps
  2. |IQ|  → envelope
  3. DC-block (HPF ~10 Hz)
  4. Decimate 4×  → 12 ksps
  5. Normalise

NFM (marine VHF, ±5 kHz deviation):
  1. Decimate 5×  → 48 ksps
  2. FM discriminator (angle diff)
  3. Low-pass FIR at 4 kHz   (hand-rolled, 31-tap)
  4. Decimate 4×  → 12 ksps
  5. Normalise
"""

import numpy as np

CAPTURE_RATE = 240_000   # sps fed in from the receiver thread
AUDIO_RATE   =  12_000   # sps going out to sounddevice


# ── Low-pass filter kernels ───────────────────────────────────────────────────

def _make_lpf(cutoff: float, sample_rate: float, n_taps: int = 31) -> np.ndarray:
    """
    Windowed-sinc low-pass filter (Hann window).
    cutoff and sample_rate in the same unit.
    """
    fc = cutoff / sample_rate
    t  = np.arange(n_taps) - (n_taps - 1) / 2
    h  = 2 * fc * np.sinc(2 * fc * t)
    h *= np.hanning(n_taps)
    return (h / h.sum()).astype(np.float32)


# Pre-built kernels (built once at import time, reused every block)
_LPF_48K_4K  = _make_lpf(4_000,  48_000, 31)   # NFM audio bandlimit
_LPF_48K_15K = _make_lpf(15_000, 48_000, 31)   # WFM deemphasis (rough)


# ── Decimation helper ─────────────────────────────────────────────────────────

def _decimate(x: np.ndarray, factor: int) -> np.ndarray:
    """Simple polyphase decimation — trim to multiple of factor then slice."""
    n = (len(x) // factor) * factor
    return x[:n].reshape(-1, factor).mean(axis=1)


# ── FM discriminator ──────────────────────────────────────────────────────────

def _fm_discriminate(iq: np.ndarray) -> np.ndarray:
    """
    Instantaneous frequency via conjugate product method.
    Output range is ±π per sample — caller should scale if needed.
    """
    conj_prod = iq[1:] * np.conj(iq[:-1])
    return np.angle(conj_prod).astype(np.float32)


# ── Public demodulators ───────────────────────────────────────────────────────

def wfm_mono(iq: np.ndarray, _sample_rate: int = CAPTURE_RATE) -> np.ndarray:
    """
    Wide FM monaural demodulator.
    Returns float32 audio at AUDIO_RATE, normalised to ±1.
    """
    # 1. Decimate 5× → 48 ksps
    i5 = _decimate(iq, 5)

    # 2. FM discriminator
    audio = _fm_discriminate(i5)

    # 3. Rough de-emphasis (low-pass keeps the warm bass, rolls off treble)
    audio = np.convolve(audio, _LPF_48K_15K, mode='same')

    # 4. Decimate 4× → 12 ksps
    audio = _decimate(audio, 4)

    # 5. Clip aggressive peaks then normalise
    audio = np.clip(audio, -1.5, 1.5)
    peak  = np.max(np.abs(audio))
    if peak > 0.01:
        audio /= peak
    return audio.astype(np.float32)


def am(iq: np.ndarray, _sample_rate: int = CAPTURE_RATE) -> np.ndarray:
    """
    AM envelope detector — suitable for aviation (airband) AM.
    Returns float32 audio at AUDIO_RATE, normalised to ±1.
    """
    # 1. Decimate 5× → 48 ksps
    i5 = _decimate(iq, 5)

    # 2. Envelope = magnitude of IQ
    env = np.abs(i5).astype(np.float32)

    # 3. DC block — subtract a slow-moving mean (IIR approximation)
    alpha = 0.995
    filtered = np.zeros_like(env)
    dc = env[0]
    for idx in range(len(env)):
        dc = alpha * dc + (1 - alpha) * env[idx]
        filtered[idx] = env[idx] - dc

    # 4. Decimate 4× → 12 ksps
    audio = _decimate(filtered, 4)

    # 5. Normalise
    peak = np.max(np.abs(audio))
    if peak > 0.01:
        audio /= peak
    return audio.astype(np.float32)


def nfm(iq: np.ndarray, _sample_rate: int = CAPTURE_RATE) -> np.ndarray:
    """
    Narrow FM demodulator — marine VHF, PMR.
    Returns float32 audio at AUDIO_RATE, normalised to ±1.
    """
    # 1. Decimate 5× → 48 ksps
    i5 = _decimate(iq, 5)

    # 2. FM discriminator
    disc = _fm_discriminate(i5)

    # 3. Audio low-pass at 4 kHz (NFM channel is ±5 kHz)
    audio = np.convolve(disc, _LPF_48K_4K, mode='same')

    # 4. Decimate 4× → 12 ksps
    audio = _decimate(audio, 4)

    # 5. Normalise
    peak = np.max(np.abs(audio))
    if peak > 0.01:
        audio /= peak
    return audio.astype(np.float32)


# ── Dispatcher ────────────────────────────────────────────────────────────────

_DEMOD_FNS = {
    "wfm": wfm_mono,
    "am":  am,
    "nfm": nfm,
}


def demodulate(iq: np.ndarray, mode: str,
               sample_rate: int = CAPTURE_RATE) -> np.ndarray:
    """
    Dispatch to the correct demodulator.
    Raises ValueError for unknown modes.
    """
    fn = _DEMOD_FNS.get(mode)
    if fn is None:
        raise ValueError(f"Unknown demodulation mode: '{mode}'. "
                         f"Valid: {list(_DEMOD_FNS)}")
    return fn(iq, sample_rate)