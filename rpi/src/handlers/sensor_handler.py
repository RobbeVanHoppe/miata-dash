from src.bus.message import Message, MessageType
from src.handlers.base_handler import BaseHandler
from src.car_state import SharedState


class SensorHandler(BaseHandler):

    def handle_message(self, message: Message, state: SharedState) -> None:
        if message.type == MessageType.TYPE_DATA:
            self._parse(message.payload, state)

    def _parse(self, payload: str, state: SharedState) -> None:
        # Format: "lights,beam,up,rpm,water,oil*10"
        # Example: "1,0,1,1500,88,32"
        try:
            parts = payload.split(",")
            state.update(
                lights_on=bool(int(parts[0])),
                beam_on=bool(int(parts[1])),
                lights_up=bool(int(parts[2])),
                rpm=int(parts[3]),
                water_temp_c=int(parts[4]),
                oil_pressure=int(parts[5]) / 10.0,
            )
        except (IndexError, ValueError) as e:
            print(f"[SensorHandler] Failed to parse payload '{payload}': {e}")