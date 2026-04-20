import time
import threading
from typing import Dict

from src.bus.message import Message, MESSAGE_SIZE, MessageNode
from src.handlers.base_handler import BaseHandler
from src.car_state import SharedState

# Node I2C addresses - mirrors nodeToAddress() in C++
NODE_ADDRESSES: Dict[MessageNode, int] = {
    MessageNode.NODE_GPS_ARDUINO:     0x10,
    MessageNode.NODE_SENSOR_ARDUINO:  0x11,
    MessageNode.NODE_TM1638_ARDUINO:  0x12,
    MessageNode.NODE_MPU9250_ARDUINO: 0x13,
}

POLL_INTERVAL = 0.1  # 100ms


class I2CMaster:
    def __init__(self, state: SharedState, bus=None):
        """
        state: SharedState instance
        bus: smbus2.SMBus instance (or mock). If None, a real bus is opened.
        """
        self._state = state
        self._handlers: Dict[MessageNode, BaseHandler] = {}
        self._bus = bus
        self._running = False
        self._thread = None

    def register(self, node: MessageNode, handler: BaseHandler):
        self._handlers[node] = handler

    def start(self):
        if self._bus is None:
            import smbus2
            self._bus = smbus2.SMBus(1)  # /dev/i2c-1

        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join()

    def _loop(self):
        while self._running:
            for node, handler in self._handlers.items():
                self._poll(node, handler)
            time.sleep(POLL_INTERVAL)

    def _poll(self, node: MessageNode, handler: BaseHandler):
        addr = NODE_ADDRESSES.get(node)
        if addr is None:
            return
        try:
            raw = self._bus.read_i2c_block_data(addr, 0, MESSAGE_SIZE)
            message = Message.from_bytes(bytes(raw))
            handler.handle_message(message, self._state)
        except Exception as e:
            print(f"[I2CMaster] Error polling {node.name} @ 0x{addr:02X}: {e}")