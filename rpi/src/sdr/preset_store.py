"""
preset_store.py — Persistent preset storage backed by a JSON file on the Pi.

Factory presets (from presets.py) are always present and never written to disk.
Custom presets are stored in PRESETS_FILE and merged in at load time.

Thread-safe: all mutations go through a threading.Lock.

Schema of one preset dict:
{
    "id":        str,   # unique slug, e.g. "custom-1717000000"
    "label":     str,   # display name, e.g. "Rotterdam Marina Ch16"
    "freq_hz":   int,
    "freq_mhz":  float,
    "mode":      str,   # "wfm" | "am" | "nfm"
    "squelch":   float,
    "band":      str,   # "fm" | "airband" | "marine" | "custom"
    "custom":    bool,  # False for factory presets
}
"""

import json
import os
import threading
import time
from pathlib import Path

from src.sdr.presets import PRESETS, Preset

_DEFAULT_PATH = Path(os.environ.get(
    "MIATA_PRESETS_FILE",
    Path(__file__).parent.parent.parent / "presets.json"
))


def _band_for_freq(freq_hz: int) -> str:
    if 88_000_000 <= freq_hz <= 108_000_000:
        return "fm"
    if 118_000_000 <= freq_hz <= 137_000_000:
        return "airband"
    if 156_000_000 <= freq_hz <= 162_000_000:
        return "marine"
    return "custom"


def _factory_to_dict(p: Preset) -> dict:
    return {
        "id":       f"factory-{p.freq_hz}-{p.mode}",
        "label":    p.label,
        "freq_hz":  p.freq_hz,
        "freq_mhz": round(p.freq_hz / 1e6, 3),
        "mode":     p.mode,
        "squelch":  p.squelch,
        "band":     _band_for_freq(p.freq_hz),
        "custom":   False,
    }


_FACTORY: list[dict] = [_factory_to_dict(p) for p in PRESETS]

BAND_ORDER = ["fm", "airband", "marine", "custom"]
BAND_LABELS = {
    "fm":      "FM Broadcast",
    "airband": "Airband",
    "marine":  "Marine VHF",
    "custom":  "Custom",
}


class PresetStore:
    """Thread-safe preset store backed by a JSON file."""

    def __init__(self, path: Path = _DEFAULT_PATH):
        self._path   = path
        self._lock   = threading.Lock()
        self._custom: list[dict] = []
        self._load()

    # ── Public API ────────────────────────────────────────────────────────────

    def all(self) -> list[dict]:
        """Return factory + custom presets."""
        with self._lock:
            return _FACTORY + list(self._custom)

    def grouped(self) -> list[dict]:
        """
        Return presets grouped by band, sorted by frequency within each group.
        Shape: [{ "band": str, "label": str, "presets": [dict] }, ...]
        """
        all_p = self.all()
        groups: dict[str, list] = {b: [] for b in BAND_ORDER}
        for p in all_p:
            band = p.get("band", "custom")
            if band not in groups:
                groups[band] = []
            groups[band].append(p)
        # Sort each group by freq
        for g in groups.values():
            g.sort(key=lambda p: p["freq_hz"])
        return [
            {"band": b, "label": BAND_LABELS[b], "presets": groups[b]}
            for b in BAND_ORDER
            if groups[b]
        ]

    def get(self, preset_id: str) -> dict | None:
        for p in self.all():
            if p["id"] == preset_id:
                return p
        return None

    def add(self, label: str, freq_hz: int, mode: str,
            squelch: float = 0, band: str | None = None) -> dict:
        if mode not in ("wfm", "am", "nfm"):
            raise ValueError(f"Invalid mode '{mode}'")
        resolved_band = band or _band_for_freq(int(freq_hz))
        preset = {
            "id":       f"custom-{int(time.time() * 1000)}",
            "label":    label.strip()[:48],
            "freq_hz":  int(freq_hz),
            "freq_mhz": round(freq_hz / 1e6, 3),
            "mode":     mode,
            "squelch":  float(squelch),
            "band":     resolved_band,
            "custom":   True,
        }
        with self._lock:
            self._custom.append(preset)
            self._save()
        return preset

    def delete(self, preset_id: str) -> bool:
        with self._lock:
            before = len(self._custom)
            self._custom = [p for p in self._custom if p["id"] != preset_id]
            changed = len(self._custom) < before
            if changed:
                self._save()
        return changed

    def update_label(self, preset_id: str, label: str) -> dict | None:
        with self._lock:
            for p in self._custom:
                if p["id"] == preset_id:
                    p["label"] = label.strip()[:48]
                    self._save()
                    return dict(p)
        return None

    # ── Persistence ───────────────────────────────────────────────────────────

    def _load(self) -> None:
        if not self._path.exists():
            self._custom = []
            return
        try:
            with open(self._path, "r", encoding="utf-8") as f:
                data = json.load(f)
            self._custom = [
                p for p in data
                if isinstance(p, dict)
                   and p.get("custom") is True
                   and "freq_hz" in p and "mode" in p
            ]
        except Exception as e:
            print(f"[PresetStore] Load failed: {e}")
            self._custom = []

    def _save(self) -> None:
        """Atomic write. Must be called with _lock held."""
        try:
            self._path.parent.mkdir(parents=True, exist_ok=True)
            tmp = self._path.with_suffix(".tmp")
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(self._custom, f, indent=2)
            tmp.replace(self._path)
        except Exception as e:
            print(f"[PresetStore] Save failed: {e}")