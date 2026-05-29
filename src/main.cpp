#include <Arduino.h>
#include <ModbusMaster.h>
#include "SinkBT.h"
#include "Display.h"

// ================= CONSTANTS & DEFINES =================
// Cấu hình Pin
#define RXD2 16
#define TXD2 17
#define BUTTON_GETDATA 35
#define ADC_PIN 34
#define BUZZER 15
#define SENSOR_POWER 4
#define L_GREEN 25
#define L_YELLOW 26
#define L_RED 27

// Cấu hình Modbus & Cảm biến
#define SENSOR_ID 1
#define MODBUS_BAUDRATE 9600
#define READ_INTERVAL_MS 1000
#define MODBUS_RETRY 3
#define BETWEEN_REQUEST_DELAY_MS 120
#define SENSOR_WARMUP_MS 500

// Thông số ADC (Tránh Magic Numbers)
const int SAMPLE_GETADC = 20;
const float ADC_RESOLUTION = 4095.0;
const float SYSTEM_VOLTAGE = 3.3;
float R1 = 10000.0; // 10k ohms
float R2 = 10000.0; // 10k ohms
const float BAT_MIN = 2.5;
const float BAT_MAX = 4.2;

// ================= GLOBALS & STRUCTS =================
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

ModbusMaster node;
SinkBT BT;
display dis;

// Cờ hiệu và trạng thái hệ thống
volatile bool isGetDataRequested = false;

// Quản lý thời gian UI
uint32_t buzzerGreenTimer = 0;
bool isBuzzerGreenActive = false;
uint32_t lastYellowToggleTime = 0;
bool yellowLedState = LOW;

// Quản lý trạng thái đọc cảm biến
bool isSensorPowerOn = false;
uint32_t sensorPowerOnTime = 0;

// ================= INTERRUPTS =================
void IRAM_ATTR onGetDataPressed()
{
  isGetDataRequested = true;
}

// ================= HARDWARE & SENSOR FUNCTIONS =================
void readBatteryPercent(SoilData &data)
{
  long sum_mV = 0;
  for (int i = 0; i < SAMPLE_GETADC; i++)
  {
    // Dùng hàm này thay cho analogRead() để triệt tiêu sai số phần cứng
    sum_mV += analogReadMilliVolts(ADC_PIN);
    delay(2);
  }

  // Lấy trung bình giá trị millivolt
  float average_mV = (float)sum_mV / SAMPLE_GETADC;

  // 1. Tính điện áp TẠI CHÂN ADC (Đổi từ mV sang V)
  float adcVoltage = average_mV / 1000.0;

  // 2. TÍNH NGƯỢC RA ĐIỆN ÁP CỦA PIN
  float batVoltage = adcVoltage * ((R1 + R2) / R2);

  // 3. TÍNH PHẦN TRĂM PIN BẰNG HÀM MAP()
  // Nhân 100 để đổi float thành int (VD: 4.2V -> 420, 2.5V -> 250)
  int batVoltageInt = batVoltage * 100;
  int batMinInt = BAT_MIN * 100;
  int batMaxInt = BAT_MAX * 100;

  // Dùng map() để ánh xạ khoảng điện áp (250-420) sang khoảng phần trăm (0-100)
  int percent = map(batVoltageInt, batMinInt, batMaxInt, 0, 100);

  // Dùng constrain() để khóa cứng giá trị không cho vượt ra ngoài 0 - 100
  // (Nếu pin sạc đầy lên 4.25V thì map() sẽ ra 102%, constrain sẽ ép nó về 100%)
  data.percent_pin = constrain(percent, 0, 100);
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
      return true;

    Serial.printf("Modbus read failed. Addr: 0x%X, Qty: %d, Attempt: %d/%d, Err: %d\n",
                  startAddress, quantity, attempt, MODBUS_RETRY, result);
    delay(BETWEEN_REQUEST_DELAY_MS);
  }
  return false;
}

bool readSoilRegisters(SoilData &data)
{
  if (!modbusReadHolding(0x0000, 7))
  {
    data.valid = false;
    return false;
  }

  uint16_t moisture_raw = node.getResponseBuffer(0);
  uint16_t temp_raw = node.getResponseBuffer(1);
  uint16_t ec_raw = node.getResponseBuffer(2);
  uint16_t ph_raw = node.getResponseBuffer(3);

  data.moisture = moisture_raw / 10.0;

  if (temp_raw == 0xFFFF || temp_raw == 0x7FFF)
  {
    data.temperature = -99.9;
  }
  else
  {
    data.temperature = (int16_t)temp_raw / 10.0;
  }

  data.ec = ec_raw;
  data.ph = ph_raw / 100.0; // Dựa theo Datasheet
  data.nitrogen = node.getResponseBuffer(4);
  data.phosphorus = node.getResponseBuffer(5);
  data.potassium = node.getResponseBuffer(6);
  data.valid = true;

  return true;
}

void printSoilData(const SoilData &data)
{
  Serial.println("\n===== SOIL DATA =====");
  if (!data.valid)
  {
    Serial.println("Sensor read failed\n---------------------");
    return;
  }
  Serial.printf("Moisture: %.1f %%\n", data.moisture);
  Serial.printf("Temperature: %.1f C\n", data.temperature);
  Serial.printf("EC: %d uS/cm\n", data.ec);
  Serial.printf("pH: %.2f\n", data.ph);
  Serial.printf("N: %d mg/kg\n", data.nitrogen);
  Serial.printf("P: %d mg/kg\n", data.phosphorus);
  Serial.printf("K: %d mg/kg\n", data.potassium);
  Serial.println("---------------------");
}

// ================= UI & INDICATOR FUNCTIONS =================
void triggerSuccessIndicators()
{
  digitalWrite(BUZZER, HIGH);
  digitalWrite(L_GREEN, HIGH);
  buzzerGreenTimer = millis();
  isBuzzerGreenActive = true;
}

void handleIndicators()
{
  // 1. Xử lý đèn vàng (Bluetooth)
  if (BT.hasClient())
  {
    digitalWrite(L_YELLOW, HIGH);
  }
  else
  {
    if (millis() - lastYellowToggleTime > 300)
    {
      lastYellowToggleTime = millis();
      yellowLedState = !yellowLedState;
      digitalWrite(L_YELLOW, yellowLedState);
    }
  }

  // 2. Xử lý tắt còi & đèn xanh tự động (sau 300ms)
  if (isBuzzerGreenActive && (millis() - buzzerGreenTimer >= 300))
  {
    digitalWrite(BUZZER, LOW);
    digitalWrite(L_GREEN, LOW);
    isBuzzerGreenActive = false;
  }
}

// ================= TASK MANAGERS =================
void processSensorData()
{
  readSoilRegisters(soil);
  readBatteryPercent(soil);

  triggerSuccessIndicators();
  printSoilData(soil);

  // Cập nhật màn hình
  if (soil.valid)
  {
    dis.showdata(soil.moisture, soil.temperature, soil.ec, soil.ph,
                 soil.nitrogen, soil.phosphorus, soil.potassium, soil.percent_pin);
  }
  else
  {
    dis.error();
  }

  // Gửi Bluetooth
  if (BT.hasClient())
  {
    if (soil.valid)
    {
      BT.SendSoilJson(soil.moisture, soil.temperature, soil.ec, soil.ph,
                      soil.nitrogen, soil.phosphorus, soil.potassium, soil.percent_pin);
    }
    else
    {
      BT.SendErrorJson("sensor_read_failed");
    }
  }
}

void handleSensorTask()
{
  if (!isGetDataRequested)
    return;

  // Bước 1: Cấp nguồn nếu chưa cấp
  if (!isSensorPowerOn)
  {
    digitalWrite(SENSOR_POWER, HIGH);
    sensorPowerOnTime = millis();
    isSensorPowerOn = true;
    Serial.println("Powering sensor, warming up...");
    return; // Thoát hàm để vòng loop tiếp tục chạy, không dùng delay()
  }

  // Bước 2: Chờ khởi động đủ thời gian
  if (millis() - sensorPowerOnTime >= SENSOR_WARMUP_MS)
  {
    processSensorData(); // Thực hiện đọc và xử lý dữ liệu

    // Bước 3: Thu dọn
    digitalWrite(SENSOR_POWER, LOW);
    isSensorPowerOn = false;
    isGetDataRequested = false; // Reset cờ ngắt sau khi hoàn thành toàn bộ
    Serial.println("Reading complete. Sensor powered off.");
  }
}

// ================= MAIN PROGRAM =================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER, OUTPUT);
  pinMode(SENSOR_POWER, OUTPUT);
  pinMode(L_GREEN, OUTPUT);
  pinMode(L_YELLOW, OUTPUT);
  pinMode(L_RED, OUTPUT);

  // Bật đèn đỏ mặc định báo có điện
  digitalWrite(L_RED, HIGH);

  attachInterrupt(digitalPinToInterrupt(BUTTON_GETDATA), onGetDataPressed, FALLING);

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

  handleIndicators(); // Chạy ngầm các tác vụ đèn/còi
  handleSensorTask(); // State machine đọc cảm biến
}