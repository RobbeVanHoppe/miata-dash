"""
presets.py — Band plans and factory presets for the Miata SDR receiver.

Each preset carries everything the receiver needs to cold-start on a band:
  freq_hz   — centre frequency in Hz
  mode      — 'wfm' | 'am' | 'nfm'
  squelch   — initial squelch level (0 = open, positive = threshold in dB)
  label     — human-readable name shown in the UI
  bandwidth — informational only (DSP uses hard-coded rates per mode)
"""

from dataclasses import dataclass


@dataclass
class Preset:
    label: str
    freq_hz: int
    mode: str           # 'wfm' | 'am' | 'nfm'
    squelch: float      # dB threshold; 0 = squelch off
    bandwidth_hz: int   # informational


PRESETS: list[Preset] = [
    # ── FM Broadcast ─────────────────────────────────────────────────────────
    Preset("FM 88.0", 88_000_000,  "wfm", 0,    200_000),
    Preset("FM 90.0", 90_000_000,  "wfm", 0,    200_000),
    Preset("FM 100.0",100_000_000, "wfm", 0,    200_000),

    # ── Aviation (AM, 118–137 MHz) ────────────────────────────────────────────
    Preset("Aviation Guard 121.5", 121_500_000, "am", 20, 8_000),
    Preset("Aviation 124.0",       124_000_000, "am", 20, 8_000),
    Preset("Aviation 128.0",       128_000_000, "am", 20, 8_000),

    # ── Marine VHF (NFM, 156–162 MHz) ────────────────────────────────────────
    Preset("Marine Ch16 156.8", 156_800_000, "nfm", 25, 12_500),
    Preset("Marine Ch6  156.3", 156_300_000, "nfm", 25, 12_500),
    Preset("Marine Ch22 157.1", 157_100_000, "nfm", 25, 12_500),
]

# Frequency limits of the RTL-SDR R820T2 tuner
FREQ_MIN_HZ = 24_000_000
FREQ_MAX_HZ = 1_766_000_000

# Supported modulation modes and their display names
MODES = {
    "wfm": "Wide FM",
    "am":  "AM",
    "nfm": "Narrow FM",
}

DEFAULT_PRESET = PRESETS[0]  # FM 88.0 on cold start