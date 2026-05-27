#include <Wire.h>
#include <U8x8lib.h>

U8X8_SH1106_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);

unsigned long lastUpdate = 0;
int counter = 0;

void setup() {
  Wire.begin();
  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.setPowerSave(0);

  oled.clearDisplay();
  oled.drawString(0, 0, "OLED TEST");
  oled.drawString(0, 2, "Uno I2C");
  oled.drawString(0, 3, "SDA -> A4");
  oled.drawString(0, 4, "SCL -> A5");
  oled.drawString(0, 6, "Starting...");
}

void loop() {
  oled.setPowerSave(0);

  if (millis() - lastUpdate < 1000) {
    return;
  }

  lastUpdate = millis();
  counter++;

  oled.clearDisplay();
  oled.drawString(0, 0, "OLED TEST");
  oled.drawString(0, 2, "A4 SDA / A5 SCL");
  oled.drawString(0, 4, "Display only");
  oled.setCursor(0, 6);
  oled.print("Count: ");
  oled.print(counter);
}
