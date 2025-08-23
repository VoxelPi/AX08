import hashlib
import serial

# --- Configure the port ---
ser = serial.Serial(
    port="COM7",
    baudrate=115200,      # Change if needed
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    xonxoff=False,      # Disable software flow control
    rtscts=True,        # Enable hardware flow control
    timeout=0.1         # Non-blocking read
)

def send_packet(payload: bytes):
    size = len(payload).to_bytes(4, byteorder='little', signed=False)
    if len(payload) > 2048:
        print("TO LONG")

    m = hashlib.sha256()
    m.update(size + payload)
    hash = m.digest()

    data = hash + size + payload
    ser.write(data)

packet_message = b"\1" # Send the id of the query info packet.
send_packet(packet_message)

try:
    data = ser.read(2048+32+4)  # Read up to 1024 bytes
    if data:
        hash_part = data[0:32]
        size_part = data[32:36]
        data_part = data[36:]

        size = int.from_bytes(size_part, byteorder="little", signed=False)
        data_part = data_part[:size]

        m = hashlib.sha256()
        m.update(size_part + data_part)
        hash_valid = hash_part == m.digest()

        if not hash_valid:
            print(f"Received packet, size={size}, hash={hash_valid}: \"{data_part.hex()}\"")
            print(f"RECEIVED: {hash_part.hex()}")
            print(f"SHOULD BE: {m.digest().hex()}")

        buffer = data_part
        packet_type_id = int.from_bytes(buffer[:1], byteorder="little", signed=False)
        buffer = buffer[1:]
        if packet_type_id == 1:
            protocol_version = int.from_bytes(buffer[:4], byteorder="little", signed=False)
            buffer = buffer[4:]

            version_length = int.from_bytes(buffer[:2], byteorder="little", signed=False)
            buffer = buffer[2:]
            version = buffer[:version_length].decode("utf-8")
            buffer = buffer[version_length:]

            git_version_length = int.from_bytes(buffer[:2], byteorder="little", signed=False)
            buffer = buffer[2:]
            git_version = buffer[:git_version_length].decode("utf-8")
            buffer = buffer[git_version_length:]

            print("Bridge information:")
            print(f"  - protocol version: {protocol_version}")
            print(f"  - version:          \"{version}\"")
            print(f"  - git version:      \"{git_version}\"")
        else:
            print(f"Received packet, size={size}, hash={hash_valid}: \"{data_part.hex()}\"")
            print(f"  - unknown packet type {packet_type_id}")

finally:
    ser.close()
