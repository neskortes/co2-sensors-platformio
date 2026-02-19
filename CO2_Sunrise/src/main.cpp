#include <Arduino.h>
#include <ModbusMaster.h>

// UART2 remapped pins
static const int MB_RX_PIN = 32;   // ESP32 RX (connect to Sunrise TX)
static const int MB_TX_PIN = 33;   // ESP32 TX (connect to Sunrise RX)

static const uint8_t SUNRISE_ID = 0x68;  // default Modbus server address 104
static const uint32_t MB_BAUD = 9600;    // 9600 8N1

// Sunrise Input Registers (0-based addresses)
// IR4 address 0x03: Filtered pressure compensated CO2 (ppm)
// IR5 address 0x04: Temperature (°C x100)
static const uint16_t IR_CO2_ADDR  = 0x03;
static const uint16_t IR_TEMP_ADDR = 0x04;

HardwareSerial MBSerial(2);   // UART2
ModbusMaster node;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\nBOOT: Senseair Sunrise over UART/Modbus");

  MBSerial.begin(MB_BAUD, SERIAL_8N1, MB_RX_PIN, MB_TX_PIN);
  node.begin(SUNRISE_ID, MBSerial);

  Serial.println("Modbus started. Reading IR4(IR_CO2) and IR5(IR_TEMP)...");
}

void loop() {
  uint8_t res = node.readInputRegisters(IR_CO2_ADDR, 2);

  if (res == node.ku8MBSuccess) {
    uint16_t co2_ppm_u = node.getResponseBuffer(0);
    int16_t  t_x100    = (int16_t)node.getResponseBuffer(1);

    Serial.print("CO2: ");
    Serial.print(co2_ppm_u);
    Serial.print(" ppm | T: ");
    Serial.print((float)t_x100 / 100.0f, 2);
    Serial.println(" C");
  } else {
    Serial.print("Modbus read failed, code=");
    Serial.println(res);
    Serial.println("Check: TX/RX crossed, GND common, Modbus ID=0x68, COMSEL=UART.");
  }

  delay(2000);
}
