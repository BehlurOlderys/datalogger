import serial
import serial.tools.list_ports
import json


# FTDI Vendor ID
FTDI_VID = 0x0403

# Load shared constants
with open("../config/constants.json", "r") as f:
    config = json.load(f)
constants = config["CONSTANTS"]

def find_ftdi_ports():
    """Find all FTDI USB serial ports by VID/PID"""
    ftdi_ports = []
    
    for port_info in serial.tools.list_ports.comports():
        if port_info.vid == FTDI_VID:
            ftdi_ports.append({
                'port': port_info.device,
                'description': port_info.description,
                'hwid': port_info.hwid,
                'vid': port_info.vid,
                'pid': port_info.pid
            })
    
    return ftdi_ports


if __name__ == '__main__':
    ports = find_ftdi_ports()
    
    if ports:
        print("Found FTDI device(s):")
        for p in ports:
            print(f"  Port: {p['port']}")
            print(f"  Description: {p['description']}")
            print(f"  VID:PID: {p['vid']:04X}:{p['pid']:04X}")
            print(f"  Hardware ID: {p['hwid']}")
            print()
    else:
        print("No FTDI devices found")
        exit(-1)

    # assume only one device:
    arduino_port = ports[0]["port"]
    baudrate = constants["SERIAL_BAUDRATE"]
    started_confirmation = constants["MSG_SUCCESSFULLY_STARTED"]

    try:
        ser = serial.Serial(arduino_port, baudrate=baudrate, timeout=1)
        print(f"✓ Serial connection opened on {arduino_port} with baudrate {baudrate}")
        
        if ser.is_open:
            print("✓ Serial port is open and ready")
        else:
            print("✗ Error: Serial port failed to open")
            exit(-2)
        
        # Read 10 messages from serial port with 0.1s timeout per read
        print("\nReading 10 messages from serial port (0.1s timeout):\n")
        ser.timeout = 3
        started = False
        max_reads = 10
        reads = 0
        while not started or reads < max_reads:
            data = ser.readline().decode('utf-8', errors='ignore').strip()
            if data:
                if not started and data == started_confirmation:
                    print(f"STARTED!")
                    started = True
                elif started:
                    print(f">>{data}")
                    reads = (reads + 1)

            else:
                print(f"(timeout - no data received)")

        print("\nThat's all, folks!")
        
    except serial.SerialException as e:
        print(f"✗ Failed to open serial port {arduino_port}: {e}")
        exit(-2)
    except Exception as e:
        print(f"✗ Unexpected error during serial connection: {e}")
        exit(-3)
    finally:
        if 'ser' in locals():
            try:
                ser.close()
                print("✓ Serial connection closed")
            except Exception as e:
                print(f"✗ Error closing serial port: {e}")

