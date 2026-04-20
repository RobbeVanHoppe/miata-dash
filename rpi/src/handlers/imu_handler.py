from src.bus.message import Message, MessageType
from src.handlers.base_handler import BaseHandler
from src.car_state import SharedState


class ImuHandler(BaseHandler):

    def handle_message(self, message: Message, state: SharedState) -> None:
        if message.type == MessageType.TYPE_DATA:
            self._parse(message.payload, state)

    def _parse(self, payload: str, state: SharedState) -> None:
        # Expected format: "x,y,z"
        try:
            x, y, z = payload.split(",")
            state.update(
                imu_x=float(x),
                imu_y=float(y),
                imu_z=float(z),
            )
        except (ValueError, TypeError) as e:
            print(f"[ImuHandler] Failed to parse payload '{payload}': {e}")