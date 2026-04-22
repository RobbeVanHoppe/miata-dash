from src.bus.message import Message, MessageType
from src.handlers.base_handler import BaseHandler
from src.car_state import SharedState


class GpsHandler(BaseHandler):

    def handle_message(self, message: Message, state: SharedState) -> None:
        if message.type == MessageType.TYPE_DATA:
            self._parse_nav(message.payload, state)
        elif message.type == MessageType.TYPE_INFO:
            self._parse_ext(message.payload, state)
        elif message.type == MessageType.TYPE_EVENT:
            self._handle_event(message.payload, state)
        elif message.type == MessageType.TYPE_ERROR:
            state.update(gps_valid=False)

    def _parse_nav(self, payload: str, state: SharedState) -> None:
        # Format: "lat,lon,speed,sats"
        # Example: "51.5074,-0.1278,60,8"
        try:
            parts = payload.split(",")
            state.update(
                gps_valid=True,
                gps_lat=float(parts[0]),
                gps_lon=float(parts[1]),
                gps_speed=float(parts[2]),
                gps_sats=int(parts[3]),
            )
        except (IndexError, ValueError) as e:
            print(f"[GpsHandler] Failed to parse nav payload '{payload}': {e}")
            state.update(gps_valid=False)

    def _parse_ext(self, payload: str, state: SharedState) -> None:
        # Format: "A:<meters>"  e.g. "A:50"
        try:
            if payload.startswith("A:"):
                state.update(gps_alt=float(payload[2:]))
        except ValueError as e:
            print(f"[GpsHandler] Failed to parse ext payload '{payload}': {e}")

    def _handle_event(self, payload: str, state: SharedState) -> None:
        if payload == "AVG_BTN":
            state.update(avg_btn_event=True)