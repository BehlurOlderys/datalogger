import logging

import serial
import serial.tools.list_ports
import json


# FTDI Vendor ID
FTDI_VID = 0x0403
STM_VID = 0x0483
STLINKv2_PID = 0x374B

# Load shared constants
with open("../config/constants.json", "r") as f:
    config = json.load(f)
constants = config["CONSTANTS"]



def find_ports(vid: int, pid: int | None = None):
    """Find all matching serial ports by VID/PID"""
    logging.debug(f"Looking for VID={vid}, PID={pid}")
    matching_ports = []
    
    for port_info in serial.tools.list_ports.comports():
        logging.debug(f"Device: {port_info.device}, #HWID: {port_info.hwid}, #VID: {port_info.vid}, #PID: {port_info.pid}")

        if port_info.vid == vid and pid is not None and port_info.pid == pid:
            matching_ports.append({
                'port': port_info.device,
                'description': port_info.description,
                'hwid': port_info.hwid,
                'vid': port_info.vid,
                'pid': port_info.pid
            })
    
    return matching_ports


if __name__ == '__main__':
    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%H:%M:%S",
    )
    ports = find_ports(vid = STM_VID, pid = STLINKv2_PID)
    
    if ports:
        logging.info("Found device(s):")
        for p in ports:
            logging.info(f"  Port: {p['port']}")
            logging.info(f"  Description: {p['description']}")
            logging.info(f"  VID:PID: {p['vid']:04X}:{p['pid']:04X}")
            logging.info(f"  Hardware ID: {p['hwid']}")
    else:
        logging.info("No devices found")
        exit(-1)

    # assume only one device:
    arduino_port = ports[0]["port"]
    baudrate = constants["SERIAL_BAUDRATE"]

    try:
        ser = serial.Serial(arduino_port, baudrate=baudrate, timeout=1)
        logging.info(f"Serial connection opened on {arduino_port} with baudrate {baudrate}")
        
        if ser.is_open:
            for i in range(0, 10):
                rubbish = ser.readline()
                logging.debug(rubbish)
                if not rubbish:
                    break
            logging.info("Serial port is open and ready")
            ser.write(b'A')
            for i in range(0, 10):
                line = ser.readline()
                logging.debug(f"received: {line}")
                if "ACK\r\n"str(line) == 'ACK':
                    logging.info("It responds!")

        else:
            logging.info("Error: Serial port failed to open")
            exit(-2)

        logging.info("\nThat's all, folks!")
        
    except serial.SerialException as e:
        logging.info(f"Failed to open serial port {arduino_port}: {e}")
        exit(-2)
    except Exception as e:
        logging.info(f"Unexpected error during serial connection: {e}")
        exit(-3)
    finally:
        if 'ser' in locals():
            try:
                ser.close()
                logging.info("Serial connection closed")
            except Exception as e:
                logging.info(f"Error closing serial port: {e}")

