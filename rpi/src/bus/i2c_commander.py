import threading
from src.bus.message import Message, MessageType, MessageNode, MESSAGE_SIZE
from src.bus.i2c_master import NODE_ADDRESSES

SENSOR_ADDR = NODE_ADDRESSES[MessageNode.NODE_SENSOR_ARDUINO]  # 0x11

# All valid commands — mirrors processCommand() in sensorForwarder.cpp
VALID_COMMANDS = {
    # Lighting
    "LIGHTS_ON", "LIGHTS_OFF",
    "BEAM_ON",   "BEAM_OFF",
    # Retractor
    "RETRACT_UP", "RETRACT_DOWN",
    # ACC bus
    "ACC_ON", "ACC_OFF",
    # Interior
    "INTERIOR_ON", "INTERIOR_OFF",
}


class I2CCommander:
    """Sends TYPE_COMMAND messages to the sensor Arduino over I2C."""

    def __init__(self, bus=None):
        self._bus = bus
        self._lock = threading.Lock()

    def start(self, bus=None):
        """Attach to an already-opened smbus2 instance or open one."""
        if bus is not None:
            self._bus = bus
        elif self._bus is None:
            import os
            if os.environ.get("MIATA_ENV") == "mock":
                from src.bus.mock_i2c import MockBus
                self._bus = MockBus()
            else:
                import smbus2
                self._bus = smbus2.SMBus(1)

    def send(self, command: str) -> bool:
        """
        Send a command string to the sensor Arduino.
        Returns True on success, False on invalid command or I2C error.
        """
        if command not in VALID_COMMANDS:
            print(f"[I2CCommander] Unknown command: '{command}'")
            return False

        msg = Message(
            type=MessageType.TYPE_COMMAND,
            source=MessageNode.NODE_ESP32,
            destination=MessageNode.NODE_SENSOR_ARDUINO,
            payload=command,
            length=len(command),
        )

        with self._lock:
            try:
                raw = list(msg.to_bytes())
                self._bus.write_i2c_block_data(SENSOR_ADDR, 0, raw)
                return True
            except Exception as e:
                print(f"[I2CCommander] Failed to send '{command}': {e}")
                return False