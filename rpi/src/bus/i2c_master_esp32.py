import time
import threading
from typing import Dict

from src.bus.message import Message, MESSAGE_SIZE, MessageNode, MessageType
from src.handlers.base_handler import BaseHandler
from src.car_state import SharedState

# Node I2C addresses - now single ESP32 at 0x08
NODE_ADDRESSES: Dict[MessageNode, int] = {
    MessageNode.NODE_ESP32: 0x08,  # Unified sensor node
}

POLL_INTERVAL = 0.1  # 100ms


class I2CMaster:
    """
    I2C Master polling a single ESP32 sensor node at 0x08.

    The ESP32 alternates between sending:
    - Sensor data: "lights,beam,up,rpm,water,oil*10"
    - GPS data:    "lat,lon,speed,sats"

    Both are routed to handlers based on payload inspection.
    """

    def __init__(self, state: SharedState, bus=None):
        self._state = state
        self._handlers: Dict[MessageNode, BaseHandler] = {}
        self._bus = bus
        self._running = False
        self._thread = None

    def register(self, node: MessageNode, handler: BaseHandler):
        """Register a handler for a node type."""
        self._handlers[node] = handler

    def start(self):
        """Open I2C bus and start polling thread."""
        if self._bus is None:
            import os
            if os.environ.get('MIATA_ENV') == 'mock':
                from src.bus.mock_i2c import MockBus
                self._bus = MockBus()
            else:
                import smbus2
                self._bus = smbus2.SMBus(1)

        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        """Stop polling and close I2C bus."""
        self._running = False
        if self._thread:
            self._thread.join()

    def send(self, node: MessageNode, message: Message):
        """
        Write a Message struct to an I2C node.
        Called from Flask to send relay commands to ESP32 at 0x08.
        """
        addr = NODE_ADDRESSES.get(node)
        if addr is None:
            raise ValueError(f'No I2C address for node {node}')
        raw = message.to_bytes()
        self._bus.write_i2c_block_data(addr, 0, list(raw))

    def _loop(self):
        """Main polling loop - repeatedly read from ESP32."""
        while self._running:
            self._poll_esp32()
            time.sleep(POLL_INTERVAL)

    def _poll_esp32(self):
        """
        Poll ESP32 at 0x08 and route data to appropriate handler.

        The ESP32 alternates between two types of messages:
        1. Sensor: "lights,beam,up,rpm,water,oil*10"
        2. GPS: "lat,lon,speed,sats"

        We detect which by counting commas and content.
        """
        addr = NODE_ADDRESSES[MessageNode.NODE_ESP32]
        try:
            raw = self._bus.read_i2c_block_data(addr, 0, MESSAGE_SIZE)
            message = Message.from_bytes(bytes(raw))

            # Route to appropriate handler based on payload
            self._route_message(message)

        except Exception as e:
            print(f'[I2CMaster] Error polling ESP32 @ 0x{addr:02X}: {e}')

    def _route_message(self, message: Message):
        """
        Determine whether message is sensor or GPS data and route accordingly.

        Heuristic:
        - Sensor data has values 0-1 in first two fields: "0,0,..." or "1,0,..."
        - GPS data has decimal points in first field: "51.5074,..."
        """
        if not message.payload or message.payload[0] == '\0':
            return

        payload = message.payload.strip()

        # Check if it looks like GPS data (contains decimal point in first field)
        if '.' in payload.split(',')[0]:
            # GPS data
            from src.handlers.gps_handler import GpsHandler
            handler = GpsHandler()
            handler.handle_message(message, self._state)
        else:
            # Sensor data
            from src.handlers.sensor_handler import SensorHandler
            handler = SensorHandler()
            handler.handle_message(message, self._state)