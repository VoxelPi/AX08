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

# Give the port a moment to initialize
time.sleep(2)

# --- Send a single raw byte ---
# Example: sending character 'A' (0x41)
# ser.write(b'A')

# print("Sent 'A'. Listening for incoming data...")

# ser.read(10000)


def send_packet(payload: bytes):
    size = len(payload).to_bytes(4, byteorder='little', signed=False)
    if len(payload) > 2048:
        print("TO LONG")

    m = hashlib.sha256()
    m.update(size + payload)
    hash = m.digest()

    data = hash + size + payload
    ser.write(data)

    # print(m.hexdigest())
# send_packet(b"L")
send_packet(b"Hello, AX08L!\n")
# send_packet(b"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n")

try:
    while True:
        print("TEST")
        data = ser.read(2048+32+4)  # Read up to 1024 bytes
        if data:
            # print(f"Received ({len(data)} bytes): {data}")

            hash_part = data[0:32]
            size_part = data[32:36]
            data_part = data[36:]

            m = hashlib.sha256()
            m.update(data[32:])

            size = size_part[0] + (size_part[1] << 8) + (size_part[2] << 16) + (size_part[3] << 24)
            print(f"Received packet, size={size}, hash={hash_part == m.digest()}: \"{data_part.decode("utf-8")}\"")
            print(f"RECEIVED: {hash_part.hex()}")
            print(f"SHOULD BE: {m.digest().hex()}")

        send_packet(b"L")
        time.sleep(5)

except KeyboardInterrupt:
    print("\nExiting.")
finally:
    ser.close()
