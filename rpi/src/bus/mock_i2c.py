import struct
from typing import Any

from src.bus.message import MESSAGE_FORMAT, MessageType, MessageNode


class MockBus:
    """Simulates Arduino I2C responses for local development and testing."""

    MOCK_PAYLOADS = {
        0x11: "0,0,1,850,33,32",
        0x10: "51.4229, 5.4926,0,1",
        0x13: "0.00,0.00,0.00",
    }

    # Tracks the last command written — useful for tests
    last_write: dict = {}

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

    def write_i2c_block_data(self, addr: int, register: int, data: list) -> None:
        self.last_write = {"addr": addr, "register": register, "data": data}