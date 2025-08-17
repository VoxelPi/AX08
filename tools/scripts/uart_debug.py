import hashlib
import serial
import time

# --- Configure the port ---
ser = serial.Serial(
    port="COM3",
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
ö
    m = hashlib.sha256()
    m.update(size + payload)
    hash = m.digest()

    data = hash + size + payload
    ser.write(data)

    # print(m.hexdigest())


packet_message = bytes(range(256))
# packet_message = b"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n"
packet_message = b"Hello, AX08L!\n"
send_packet(packet_message)

try:
    while True:
        data = ser.read(2048+32+4)  # Read up to 1024 bytes
        if data:
            hash_part = data[0:32]
            size_part = data[32:36]
            data_part = data[36:]

            m = hashlib.sha256()
            m.update(data[32:])
            hash_valid = hash_part == m.digest()

            size = size_part[0] + (size_part[1] << 8) + (size_part[2] << 16) + (size_part[3] << 24)
            # print(f"Received packet, size={size}, hash={hash_valid}: \"{data_part.hex()}\"")
            print(f"Received packet, size={size}, hash={hash_valid}: \"{data_part.decode("utf-8").replace("\n", "\\n")}\"")
            if not hash_valid:
                print(f"RECEIVED: {hash_part.hex()}")
                print(f"SHOULD BE: {m.digest().hex()}")

        send_packet(packet_message)
        time.sleep(1)

except KeyboardInterrupt:
    print("\nExiting.")
finally:
    ser.close()
