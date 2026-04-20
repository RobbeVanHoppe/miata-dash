from src.bus.message import Message, MessageType
from src.handlers.base_handler import BaseHandler
from src.car_state import SharedState


class ImuHandler(BaseHandler):

    def handle_message(self, message: Message, state: SharedState) -> None:
        if message.type == MessageType.TYPE_DATA:
            self._parse(message.payload, state)

    def _parse(self, payload: str, state: SharedState) -> None:
        # Format: "x,y,z"
        # Example: "0.02,0.98,0.15"
        try:
            parts = payload.split(",")
            state.update(
                imu_x=float(parts[0]),
                imu_y=float(parts[1]),
                imu_z=float(parts[2]),
            )
        except (IndexError, ValueError) as e:
            print(f"[ImuHandler] Failed to parse payload '{payload}': {e}")