#include <Arduino.h>
#include <ModbusMaster.h>

#define RXD2 16
#define TXD2 17
int count = 0;
ModbusMaster node;

void setup() {
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Địa chỉ cảm biến Modbus, thường là 1
  node.begin(1, Serial2);

  Serial.println("ESP32 RS485 Modbus test start");
  
}

void loop() {
  // Đọc 7 thanh ghi từ địa chỉ 0x0000
  uint8_t result = node.readHoldingRegisters(0x0000, 7);

  if (result == node.ku8MBSuccess) {
    Serial.println("Read success:");

    for (int i = 0; i < 7; i++) {
      uint16_t value = node.getResponseBuffer(i);

      Serial.print("Register ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(value);
    }
  } else {
    Serial.print("Read failed. Error code: ");
    Serial.println(result);
  }

  Serial.println("------------------");
  
  count += 1;
  Serial.print("Count:");
  Serial.println(count);
  delay(1000);
}