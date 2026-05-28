#include <Arduino.h>
#include <ModbusMaster.h>
#include "SinkBT.h"
#include "Display.h"

#define RXD2 16
#define TXD2 17

// input
#define BUTTON_GETDATA 35
#define ADC_PIN 34
#define SAMPLE_GETDAC 20
// output
#define BUZZER 15
#define SENSOR_POWER 4

#define L_GREEN 25
#define L_YELLOW 26
#define L_RED 27

#define SENSOR_ID 1
#define MODBUS_BAUDRATE 9600

#define READ_INTERVAL_MS 1000
#define MODBUS_RETRY 3
#define BETWEEN_REQUEST_DELAY_MS 120

volatile bool getdata_flag = false;

ModbusMaster node;
SinkBT BT;
display dis;

struct SoilData
{
  float moisture;
  float temperature;
  uint16_t ec;
  float ph;
  uint16_t nitrogen;
  uint16_t phosphorus;
  uint16_t potassium;
  uint16_t percent_pin;
  bool valid;
};

SoilData soil = {0, 0, 0, 0, 0, 0, 0, 0, false};

//=============================================================
void buzzer()
{
  digitalWrite(BUZZER, HIGH);
  delay(300);
  digitalWrite(BUZZER, LOW);
}

void led(char c)
{
  switch (c)
  {
  case 67: // ASCII code "g"
    for (int i = 0; i < 3; i++)
    {
      digitalWrite(L_GREEN, HIGH);
      delay(100);
      digitalWrite(L_GREEN, LOW);
      delay(100);
    }
    break;

  case 79: // ASCII code "l"
    while (1)
    {
      digitalWrite(L_YELLOW, HIGH);
      delay(100);
      digitalWrite(L_YELLOW, LOW);
      delay(100);
    }
    break;

  case 72: // ASCII code "r"
    digitalWrite(L_RED, HIGH);
    break;
  }
}

void read_pin(SoilData &data)
{
  long sum = 0;
  for (int i = 0; i < SAMPLE_GETDAC; i++)
  {
    sum = sum + analogRead(ADC_PIN);
    delay(2);
  }
  float averageADC = sum / SAMPLE_GETDAC;
  float voltage = (averageADC / 4095.0) * 3.3;
  if (voltage > 2.5)
  {
    data.percent_pin = ((voltage - 2.5) / (3.3 - 2.5)) * 100;
  }
  else
    data.percent_pin = 0;
}

void IRAM_ATTR getdata_sensor()
{
  getdata_flag = true;
}

void clearSerial2Buffer()
{
  while (Serial2.available())
  {
    Serial2.read();
  }
}

bool modbusReadHolding(uint16_t startAddress, uint16_t quantity)
{
  for (uint8_t attempt = 1; attempt <= MODBUS_RETRY; attempt++)
  {
    clearSerial2Buffer();

    uint8_t result = node.readHoldingRegisters(startAddress, quantity);

    if (result == node.ku8MBSuccess)
    {
      return true;
    }

    Serial.print("Modbus read failed. Address: 0x");
    Serial.print(startAddress, HEX);
    Serial.print(" Quantity: ");
    Serial.print(quantity);
    Serial.print(" Attempt: ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.print(MODBUS_RETRY);
    Serial.print(" Error: ");
    Serial.println(result);

    delay(BETWEEN_REQUEST_DELAY_MS);
  }

  return false;
}

bool readMainSoilRegisters(SoilData &data)
{
  // Đọc 7 thanh ghi liên tiếp từ 0x0000
  bool ok = modbusReadHolding(0x0000, 7);

  if (!ok)
  {
    data.valid = false;
    return false;
  }

  // Đọc giá trị thô từ bộ đệm Modbus
  uint16_t moisture_raw = node.getResponseBuffer(0);
  uint16_t temp_raw = node.getResponseBuffer(1); // Lấy giá trị thô dạng uint16_t
  uint16_t ec_raw = node.getResponseBuffer(2);
  uint16_t ph_raw = node.getResponseBuffer(3);
  uint16_t n_raw = node.getResponseBuffer(4);
  uint16_t p_raw = node.getResponseBuffer(5);
  uint16_t k_raw = node.getResponseBuffer(6);

  // Ép kiểu ép buộc về số có dấu int16_t để xử lý chính xác nhiệt độ
  int16_t temp_signed = (int16_t)temp_raw;

  data.moisture = moisture_raw / 10.0;

  // Kiểm tra nếu giá trị trả về bất thường (ví dụ 0 hoặc 32767 - lỗi kết nối đầu dò nội bộ)
  if (temp_raw == 0xFFFF || temp_raw == 0x7FFF)
  {
    data.temperature = -99.9; // Giá trị báo lỗi đầu dò nhiệt độ của cảm biến
  }
  else
  {
    data.temperature = temp_signed / 10.0;
  }

  data.ec = ec_raw;

  // SỬA THEO DATASHEET: Bản này pH chỉ nhân 10 chứ không phải nhân 100
  data.ph = ph_raw / 100.0;

  data.nitrogen = n_raw;
  data.phosphorus = p_raw;
  data.potassium = k_raw;
  data.valid = true;

  return true;
}

bool readSoilSensor(SoilData &data)
{
  data.valid = false;

  bool mainOk = readMainSoilRegisters(data);

  if (!mainOk)
  {
    return false;
  }

  return true;
}

void printSoilData(const SoilData &data)
{
  Serial.println("===== SOIL DATA =====");

  if (!data.valid)
  {
    Serial.println("Sensor read failed");
    Serial.println("---------------------");
    return;
  }

  Serial.print("Moisture: ");
  Serial.print(data.moisture, 1);
  Serial.println(" %");

  Serial.print("Temperature: ");
  Serial.print(data.temperature, 1);
  Serial.println(" C");

  Serial.print("EC: ");
  Serial.print(data.ec);
  Serial.println(" uS/cm");

  Serial.print("pH: ");
  Serial.println(data.ph, 2);

  Serial.print("N: ");
  Serial.print(data.nitrogen);
  Serial.println(" mg/kg");

  Serial.print("P: ");
  Serial.print(data.phosphorus);
  Serial.println(" mg/kg");

  Serial.print("K: ");
  Serial.print(data.potassium);
  Serial.println(" mg/kg");

  Serial.println("---------------------");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER, OUTPUT);
  pinMode(SENSOR_POWER, OUTPUT);

  pinMode(L_GREEN, OUTPUT);
  pinMode(L_YELLOW, OUTPUT);
  pinMode(L_RED, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_GETDATA), getdata_sensor, FALLING);

  Serial2.setRxBufferSize(256);
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1, RXD2, TXD2);

  node.begin(SENSOR_ID, Serial2);

  Serial.println("ESP32 Soil Sensor Start - Auto RS485 Module");

  BT.begin("ESP32_SOIL_SENSOR");

  dis.begin();
  dis.logo();
}

void loop()
{
  BT.CheckConnection();

  // Khai báo các biến trạng thái tĩnh
  static bool isPowerOn = false;
  static uint32_t powerOnTime = 0;

  // 1. Khi có tín hiệu ngắt nhấn nút
  if (getdata_flag)
  {
    if (!isPowerOn)
    {
      digitalWrite(SENSOR_POWER, HIGH); // Cấp nguồn cho cảm biến
      powerOnTime = millis();           // Lưu lại thời điểm cấp nguồn
      isPowerOn = true;
      Serial.println("Đang cấp nguồn, chờ cảm biến khởi động...");
    }

    // 2. Chờ 500ms (Warm-up) để cảm biến đo đạc ổn định trước khi phát lệnh Modbus
    if (isPowerOn && (millis() - powerOnTime >= 500))
    {
      bool ok = readSoilSensor(soil);
      read_pin(soil);
      buzzer();

      if (!ok)
      {
        soil.valid = false;
      }

      printSoilData(soil);
      if (soil.valid)
      {
        dis.showdata(soil.moisture,
                     soil.temperature,
                     soil.ec,
                     soil.ph,
                     soil.nitrogen,
                     soil.phosphorus,
                     soil.potassium,
                     soil.percent_pin);
      }
      else
        dis.error();

      if (BT.hasClient())
      {
        if (soil.valid)
        {
          BT.SendSoilJson(
              soil.moisture,
              soil.temperature,
              soil.ec,
              soil.ph,
              soil.nitrogen,
              soil.phosphorus,
              soil.potassium,
              soil.percent_pin);
        }
        else
        {
          BT.SendErrorJson("sensor_read_failed");
        }
      }

      // 3. Đọc xong xuôi thì tắt nguồn cảm biến và xóa cờ hiệu
      digitalWrite(SENSOR_POWER, LOW);
      isPowerOn = false;
      getdata_flag = false; // Chỉ hạ cờ ngắt khi ĐÃ ĐỌC XONG
      Serial.println("Đã hoàn thành lượt đọc. Ngắt nguồn cảm biến.");
    }
  }
}