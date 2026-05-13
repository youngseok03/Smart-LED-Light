#include <Arduino.h>

/*
  PIR motion sensor standalone test for ESP32 DevKit V1

  Wiring:
    PIR VCC -> module-rated supply, usually 5V or 3V3
    PIR GND -> ESP32 GND
    PIR OUT -> GPIO27

  Check the module output voltage. ESP32 GPIO input must not exceed 3.3V.
  Serial Monitor: 115200 baud
*/

const uint8_t PIN_PIR = 27;
bool lastMotion = false;
uint32_t lastPrintAt = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_PIR, INPUT);
  Serial.println("PIR motion sensor test started");
}

void loop() {
  bool motion = digitalRead(PIN_PIR) == HIGH;

  if (motion != lastMotion) {
    lastMotion = motion;
    Serial.println(motion ? "Motion detected" : "Motion cleared");
  }

  if (millis() - lastPrintAt >= 1000) {
    lastPrintAt = millis();
    Serial.print("PIR raw: ");
    Serial.println(motion ? 1 : 0);
  }
}
