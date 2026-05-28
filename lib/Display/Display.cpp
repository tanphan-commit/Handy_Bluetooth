#include "Display.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// Khai báo chân reset (dùng -1 nếu dùng chung chân reset của Arduino)
#define OLED_RESET -1
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void display::begin()
{
    // Khởi tạo giao tiếp I2C mặc định (SDA=21, SCL=22)
    Wire.begin();

    // Khởi tạo màn hình với địa chỉ I2C 0x3C
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("[DIS] Khong tim thay man hinh OLED!");
        for (;;)
            ;
    }
}

void display::logo()
{
    oled.clearDisplay();
    oled.setTextSize(3);
    oled.setTextColor(WHITE);
    oled.setCursor(11, 33);
    oled.print("IoTLab");
    oled.display();
}

void display::error()
{
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(33, 33);
    oled.print("LOI CAM BIEN");
    oled.display();
}

void display::showdata(
    float moisture,
    float temperature,
    uint16_t ec,
    float ph,
    uint16_t nitrogen,
    uint16_t phosphorus,
    uint16_t potassium,
    uint16_t percent_pin)
{
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    
    // --- CỘT TRÁI (X = 0): Các chỉ số môi trường đất ---
    oled.setCursor(0, 2); // Dòng 1
    oled.print("Do am: "); oled.print(moisture, 1); oled.print("%");

    oled.setCursor(0, 20); // Dòng 2 (Cách 16 pixel)
    oled.print("Nhiet: "); oled.print(temperature, 1); oled.print("'C");

    oled.setCursor(0, 36); // Dòng 3
    oled.print("pH:    "); oled.print(ph, 1);

    oled.setCursor(0, 52); // Dòng 4
    oled.print("EC:    "); oled.print(ec);

    // --- CỘT PHẢI (X = 95): Chỉ số dinh dưỡng NPK ---
    oled.setCursor(85, 2);
    oled.print("N: "); oled.print(nitrogen);

    oled.setCursor(85, 20);
    oled.print("P: "); oled.print(phosphorus);

    oled.setCursor(85, 36);
    oled.print("K: "); oled.print(potassium);

    oled.setCursor(75, 52);
    oled.print("Pin:"); oled.print(percent_pin);oled.print("%");



    oled.display(); 
}

