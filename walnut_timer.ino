#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#define DISPLAY_ADDRESS (0x70)
#define BRIGHTNESS (15)

Adafruit_7segment seg_display = Adafruit_7segment();

void setup()
{
    seg_display.begin(DISPLAY_ADDRESS);
    seg_display.setBrightness(BRIGHTNESS);
}

void loop()
{
    seg_display.writeDisplay();
    delay(500);
}
