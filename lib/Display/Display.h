#ifndef DISPLAY_H
#define DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class display {
public:
void begin();
void logo();
void showdata(
    float moisture,
    float temperature,
    uint16_t ec,
    float ph,
    uint16_t nitrogen,
    uint16_t phosphorus,
    uint16_t potassium,
    uint16_t percent_pin
);
void error();

private:
};







#endif
