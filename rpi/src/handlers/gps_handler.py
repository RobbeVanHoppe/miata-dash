from rpi.src.bus.message import Message, MessageType
from rpi.src.handlers.base_handler import BaseHandler
from rpi.src.car_state import SharedState


class GpsHandler(BaseHandler):

    def handle_message(self, message: Message, state: SharedState) -> None:
        if message.type == MessageType.TYPE_DATA:
            self._parse(message.payload, state)
        elif message.type == MessageType.TYPE_ERROR:
            state.update(gps_valid=False)

    def _parse(self, payload: str, state: SharedState) -> None:
        # Expected format: "lat,lon,speed,alt,sats"
        try:
            parts = payload.split(",")
            state.update(
                gps_valid=True,
                gps_lat=float(parts[0]),
                gps_lon=float(parts[1]),
                gps_speed=float(parts[2]),
                gps_alt=float(parts[3]),
                gps_sats=int(parts[4]),
            )
        except (IndexError, ValueError) as e:
            print(f"[GpsHandler] Failed to parse payload '{payload}': {e}")
            state.update(gps_valid=False)