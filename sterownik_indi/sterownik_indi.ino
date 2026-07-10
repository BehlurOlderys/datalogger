#include <Wire.h>
#include <Adafruit_BMP280.h> 
#include <SPIMemory.h>
#include <DFRobot_BMI160.h>
#include "constants.h"

const int CS_PIN = 10;
const byte BMP_280_ADDRESS = 0x77; 
const byte BMI_160_ADDRESS = 0x69; 
const float PRESSURE_REFERENCE_HPA = 1013.25; 
const uint16_t BLOCK_SIZE = 256; 
uint8_t dataBuffer[BLOCK_SIZE];

bool bmp280OK = false;
bool bmi160OK = false;
bool flashOK = false;
uint32_t realCapacity = 0;

Adafruit_BMP280 bmp;
DFRobot_BMI160 bmi160;
SPIFlash w25flash(CS_PIN);

struct __attribute__((__packed__)) IMUFrame {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
};

struct __attribute__((__packed__)) ReadoutFrame{
  uint16_t pressure;     
  IMUFrame imu[4];  
};

enum class PacketType : uint8_t{
  NORMAL = 0xBA,
  UNKNOWN = 0x77
};

struct __attribute__((__packed__)) Packet256 {
  PacketType packetType;
  uint32_t packetNumber;
  uint32_t timestamp;  
  ReadoutFrame readouts[4];
  uint8_t filler[47]; // reminder to 256
};

static_assert(sizeof(IMUFrame) == 12, "IMUFrame is not 12B");
static_assert(sizeof(ReadoutFrame) == 50, "EReadoutFrame is not 50B");
static_assert(sizeof(Packet256) == 256, "Packet256 is not 256B");

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  while (!Serial);
  delay(1000);
  Serial.println("Hello, world!");

  //////////////////////// BMP 280:
  Wire.beginTransmission(BMP_280_ADDRESS);
  const byte bmpErrorCode = Wire.endTransmission();

  if (bmpErrorCode == 0 && bmp.begin(BMP_280_ADDRESS)) 
  {
    bmp280OK = true;
    // Serial.println("BMP 280 successfully initialized!");
  }

  ////////////////////// BMI 160:
  Wire.beginTransmission(BMI_160_ADDRESS);
  const byte bmiErrorCode = Wire.endTransmission();

  if (bmiErrorCode == 0 && bmi160.I2cInit(BMI_160_ADDRESS) == BMI160_OK) 
  {
    bmi160OK = true;
    // Serial.println("BMI160 successfully initialized!");
  }
  ///////////////////// W25Q64 Memory
  if (w25flash.begin()) 
  {
    flashOK = true;
    // // Serial.println(F("W25Q64 successfully initialized!"));
    // realCapacity = w25flash.getCapacity(); \
    // Serial.print(F("Available flash memory: "));
    // Serial.print(realCapacity);
    // Serial.println("B");
  }
  if (bmp280OK && flashOK && bmi160OK){
    Serial.println(MSG_SUCCESSFULLY_STARTED);
  }
  else if (!bmp280OK){
    Serial.println("BMP280Error");
  }
  else if (!flashOK){
    Serial.println("FlashError");
  }
  else if (!bmi160OK){
    Serial.println("BMI Error");
  }
}

int i=0;

void dumpBinaryMemory(uint32_t maxAddress) { 
  for (uint32_t addr = 0; addr < maxAddress; addr += BLOCK_SIZE) {
    w25flash.readByteArray(addr, dataBuffer, BLOCK_SIZE);
    Serial.write(dataBuffer, BLOCK_SIZE);
    Serial.flush(); 
  }
}

void loop() {
  if (Serial.available()){
    String receivedCommand = Serial.readStringUntil('\n'); 
    receivedCommand.trim();
    if (receivedCommand == MSG_DUMP)
    {
      Serial.println("OK");
      // dumpBinaryMemory(realCapacity);
      dumpBinaryMemory(100);
    }
    else
    {
      Serial.print("Unknown command: ");
      Serial.println(receivedCommand);
    }
  }
  // Serial.print(i);
  // Serial.print(": temp: "); 
  float temp=-6.6;
  float heig = -5.5;
  if (bmp280OK){
    heig = bmp.readAltitude(PRESSURE_REFERENCE_HPA);
    temp = bmp.readTemperature();
    
    Serial.print(temp); 
    Serial.print(", altitude: ");
    Serial.println(heig);
  }
  // else{
  //   Serial.println("nothing..");
  // }

  delay(1000);
  // i = i + 1;
}
