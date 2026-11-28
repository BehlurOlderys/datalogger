import logging

import serial
import serial.tools.list_ports
import json
from time import sleep
import parse
import ctypes
import struct


frame_parser = parse.compile(
    "I={frame_index:03d}: [{time_diff:06d}] "
    "G=({gx:+05d}, {gy:+05d}, {gz:+05d}), "
    "A=({ax:+05d}, {ay:+05d}, {az:+05d}) "
    "T={temperature:04d}, P={pressure:08d}"
)
# TODO: this should be generated and synced to data_structures.h!

MAX_FRAMES_PER_PAGE = 12

class DataFrame(ctypes.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("deltaTime", ctypes.c_uint16),
        ("temperature", ctypes.c_int16),
        ("pressure", ctypes.c_uint32),
        ("gyroAndAcc", ctypes.c_int16 * 6)
    ]

    def __str__(self):
        imu = list(self.gyroAndAcc)
        return (
            f"dt={self.deltaTime:04}ms | "
            f"temp={self.temperature:04} | "
            f"press={self.pressure:08} | "
            f"imu={imu}"
        )

class PageHeader(ctypes.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("magicNumber", ctypes.c_uint32),
        ("timestamp", ctypes.c_uint32),
        ("pageIndex", ctypes.c_uint32),
        ("nextFrameIndex", ctypes.c_uint16),
        ("crc16", ctypes.c_uint16)
    ]

    def __str__(self):
        return (
            f"Page #{self.pageIndex} "
            f"(magic=0x{self.magicNumber:08X}, "
            f"ts={self.timestamp}, "
            f"frames={self.nextFrameIndex}/12, "
            f"crc=0x{self.crc16:04X})"
        )

class FlashPage(ctypes.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("header", PageHeader),
        ("frames", DataFrame * MAX_FRAMES_PER_PAGE),
    ]

    def __str__(self):
        lines = [f"=== {self.header} ==="]
        for i in range(0, MAX_FRAMES_PER_PAGE):
            lines.append(f"  [{i:02d}] {self.frames[i]}")

        return "\n".join(lines)


# FTDI Vendor ID
FTDI_VID = 0x0403
STM_VID = 0x0483
STLINKv2_PID = 0x374B
SOP_MARKER = "***SOP***"
EOP_MARKER = "***EOP***"

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

class ResponseNotFound(Exception):
    """Expected response not found"""
    pass


def wait_for_a_text_on_serial(serial_instance: serial.Serial, expected_text: str, max_retries: int=10) -> None:
    for try_index in range(0, max_retries):
        try:
            recv_line = serial_instance.readline().decode('utf-8').strip()
        except UnicodeDecodeError as e:
            logging.warning(f"Typical decoding error: we read something not meant to be read. Skipping. Retries left: {max_retries - 1 - try_index}/{max_retries}")
            continue
        logging.debug(f"received: {recv_line}")
        if recv_line == expected_text:
            return
        if expected_text in recv_line:
            logging.warning(f"Not pure match: {recv_line}, but optimistically passing")
            return
        else:
            logging.warning(f"Timeout on serial read line, retries left: {max_retries - 1 - try_index}/{max_retries}")
    raise ResponseNotFound(f"Not found {expected_text}")


def read_text(serial_instance: serial.Serial):
    serial_instance.write(b'T')
    wait_for_a_text_on_serial(serial_instance=serial_instance, expected_text="MODE=UART_TXT", max_retries=100)
    serial_instance.write(b'L')
    wait_for_a_text_on_serial(serial_instance=serial_instance, expected_text="MEAS=48")
    while True:
        line = serial_instance.readline().decode('utf-8').strip()
        if SOP_MARKER in line:
            logging.debug("Start of page!")
            continue
        elif EOP_MARKER in line:
            logging.debug("End of page!")
            continue
        else:
            res = frame_parser.parse(line)
            if res:
                logging.info(res.named)
            else:
                logging.warning(f"Couldn't parse {line}, skipping...")

def read_binary(serial_instance: serial.Serial):
    MAGIC_BYTES = struct.pack("<I", 0x1ACFFC1D)  # Replace with your magic number
    PACKET_SIZE = ctypes.sizeof(FlashPage)  # Your ctypes.LittleEndianStructure

    logging.debug(f"I hope this reads 256: {PACKET_SIZE}")
    in_buffer = bytearray()
    serial_instance.write(b'B')
    wait_for_a_text_on_serial(serial_instance=serial_instance, expected_text="MODE=UART_BIN", max_retries=100)
    serial_instance.write(b'L')
    wait_for_a_text_on_serial(serial_instance=serial_instance, expected_text="MEAS=48")

    while True:
        waiting = serial_instance.in_waiting
        chunk = serial_instance.read(waiting if waiting > 0 else 1)
        if not chunk:
            continue
        in_buffer.extend(chunk)
        while len(in_buffer) >= PACKET_SIZE:
            logging.debug(f"Current buffer length: {len(in_buffer)}")
            idx = in_buffer.find(MAGIC_BYTES)
            if idx == -1:
                logging.debug("Dropping buffer all but last 3 bytes")
                del in_buffer[:-3]
                break

            if idx > 0:
                logging.debug("Dropping buffer all up to where magic number was found ({idx})")
                del in_buffer[:idx]
                if len(in_buffer) < PACKET_SIZE: #so we need to read more
                    break

            candidate = in_buffer[:PACKET_SIZE]

            # if self._verify_crc(candidate):
            #     # Valid packet! Remove it from buffer and return struct
            #     del self.buf[:PACKET_SIZE]
            page = FlashPage.from_buffer_copy(candidate)
            logging.info(f"Successfully read page:\n{page}")
            logging.debug(f"Dropping {PACKET_SIZE} from buffer after successful readout")
            del in_buffer[:PACKET_SIZE]
            # else:
            #     # Corrupted packet or false magic sync:
            #     # Drop ONLY the 4 magic bytes and keep scanning from byte 4
            #     del self.buf[:4]




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
            # for i in range(0, 10):
            #     rubbish = ser.readline()
            #     logging.debug(rubbish)
            #     if not rubbish:
            #         break
            # logging.info("Serial port is open and ready")
            # read_text(serial_instance=ser)
            read_binary(serial_instance=ser)


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

