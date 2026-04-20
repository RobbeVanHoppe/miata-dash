import struct
from typing import List, Any

from src.bus.message import MESSAGE_FORMAT, MessageType, MessageNode


class MockBus:
    """Simulates Arduino I2C responses for local development and testing."""

    # Each address returns a fixed fake payload matching the new positional CSV format
    MOCK_PAYLOADS = {
        0x11: "1,0,1,1500,88,32",       # Sensor:  lights,beam,up,rpm,water,oil*10
        0x10: "51.5074,-0.1278,60,8",   # GPS:     lat,lon,speed,sats
        0x13: "0.02,0.98,0.15",         # IMU:     x,y,z
    }

    def read_i2c_block_data(self, addr: int, register: int, length: int) -> list[Any]:
        payload = self.MOCK_PAYLOADS.get(addr, "0")
        payload_bytes = payload.encode("ascii")[:24].ljust(24, b"\x00")
        raw = struct.pack(
            MESSAGE_FORMAT,
            int(MessageType.TYPE_DATA),
            int(MessageNode.NODE_SENSOR_ARDUINO),
            int(MessageNode.NODE_ESP32),
            payload_bytes,
            len(payload),
        )
        return list(raw)
