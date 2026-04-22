from dataclasses import dataclass, asdict
import threading


@dataclass
class CarState:
    # Lights
    lights_on: bool = False
    beam_on: bool = False
    lights_up: bool = False

    # Engine
    rpm: int = 0
    water_temp_c: int = 0
    oil_pressure: float = 0.0

    # Brakes
    parking_brake_on: bool = False

    # GPS
    gps_valid: bool = False
    gps_lat: float = 0.0
    gps_lon: float = 0.0
    gps_speed: float = 0.0
    gps_alt: float = 0.0
    gps_sats: int = 0
    avg_btn_event: bool = False  # True for one poll cycle when the physical button fires

    # IMU
    imu_x: float = 0.0
    imu_y: float = 0.0
    imu_z: float = 0.0

    def to_dict(self) -> dict:
        return asdict(self)


class SharedState:
    """Thread-safe wrapper around CarState."""

    def __init__(self):
        self._state = CarState()
        self._lock = threading.Lock()

    def update(self, **kwargs):
        with self._lock:
            for key, value in kwargs.items():
                if hasattr(self._state, key):
                    setattr(self._state, key, value)

    def snapshot(self) -> dict:
        with self._lock:
            return self._state.to_dict()