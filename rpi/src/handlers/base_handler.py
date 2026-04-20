from src.bus.message import Message
from src.car_state import SharedState


class BaseHandler:
    """
    All node handlers inherit from this.
    Implement handle_message() to parse incoming data into state.
    """

    def handle_message(self, message: Message, state: SharedState) -> None:
        raise NotImplementedError(
            f"{self.__class__.__name__} must implement handle_message()"
        )