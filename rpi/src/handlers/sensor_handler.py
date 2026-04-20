from rpi.src.bus.message import Message, MessageType
from rpi.src.handlers.base_handler import BaseHandler
from rpi.src.car_state import SharedState


class SensorHandler(BaseHandler):

    def handle_message(self, message: Message, state: SharedState) -> None:
        if message.type == MessageType.TYPE_DATA:
            self._parse(message.payload, state)

    def _parse(self, payload: str, state: SharedState) -> None:
        # Expected format from Nano: "L:%d,B:%d,U:%d,R:%d,W:%d,O:%.1f"
        try:
            parts = dict(p.split(":") for p in payload.split(","))
            state.update(
                lights_on=bool(int(parts["L"])),
                beam_on=bool(int(parts["B"])),
                lights_up=bool(int(parts["U"])),
                rpm=int(parts["R"]),
                water_temp_c=int(parts["W"]),
                oil_pressure=float(parts["O"]),
            )
        except (KeyError, ValueError) as e:
            print(f"[SensorHandler] Failed to parse payload '{payload}': {e}")