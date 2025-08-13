import hashlib
import serial
import time

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

# Give the port a moment to initialize
time.sleep(2)

# --- Send a single raw byte ---
# Example: sending character 'A' (0x41)
# ser.write(b'A')

# print("Sent 'A'. Listening for incoming data...")

# ser.read(10000)

try:
    while True:
        ser.write(b"a")
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
        time.sleep(5)

except KeyboardInterrupt:
    print("\nExiting.")
finally:
    ser.close()
