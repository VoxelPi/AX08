from dataclasses import dataclass
import hashlib
from serial import Serial

@dataclass
class AX08ProgramInfo:
    name: str
    hash: str


class AX08ComputerConnection:
    port: Serial

    def __init__(self, port: Serial) -> None:
        self.port = port

    def close(self):
        self.port.close()

@dataclass
class AX08Computer:
    port_id: str
    port_baud: int = 115200
    port_flow_control: bool = True

    def __enter__(self) -> AX08ComputerConnection:
        port = Serial(port = self.port_id, baudrate=self.port_baud, rtscts=self.port_flow_control)
        self._connection = AX08ComputerConnection(port=port)
        return self._connection

    def __exit__(self, exception_type, exception_value, exception_traceback):
        self._connection.close()
