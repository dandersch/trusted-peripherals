#!/usr/bin/python
import serial
import time

# Define the serial port and baud rate
ser = serial.Serial('/dev/ttyACM0', baudrate=9600)

# Open the serial port
#ser.open()

# Initialize variables
start_time = time.time()
byte_count = 0

exit()

try:
    while True:
        # Read data from the serial port
        data = ser.read(1)  # Read one byte (adjust as needed)

        print(data, end="")

        # Check for the magic number "EXEC" to start measuring
        if data == b'C':
            magic_number = ser.read(1)  # Read the remaining "XEC"
            if magic_number == b'I':
                start_time = time.time()  # Start measuring
                print("Magic number received. Measurement started.")

        # Check for the end of transmission or other conditions to stop
        if not data:
            break

        # Update byte count
        byte_count += len(data)

    # Calculate elapsed time
    end_time = time.time()
    elapsed_time = end_time - start_time

    # Calculate transmission rate in bytes per second (Bps)
    transmission_rate_bps = byte_count / elapsed_time

    # Print the results
    print(f"Total Bytes Received: {byte_count} bytes")
    print(f"Elapsed Time: {elapsed_time} seconds")
    print(f"Transmission Rate: {transmission_rate_bps:.2f} Bps")

except KeyboardInterrupt:
    print("Measurement interrupted.")

finally:
    # Close the serial port
    ser.close()
