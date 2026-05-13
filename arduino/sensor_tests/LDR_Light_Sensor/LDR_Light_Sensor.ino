#include <Arduino.h>

/*
  LDR light sensor standalone test for ESP32 DevKit V1

  Wiring used by the main project:
    ESP32 3V3 -> LDR -> GPIO34 -> 10k resistor -> GND

  With this divider, ADC value increases when it is bright
  and decreases when it is dark.

  Serial Monitor: 115200 baud
*/

const uint8_t PIN_LDR = 34;       // ADC1 input-only pin
const int DARK_THRESHOLD = 1600;  // Calibrate after checking your room values.
const uint32_t READ_INTERVAL_MS = 500;

uint32_t lastReadAt = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);

  Serial.println("LDR light sensor test started");
  Serial.println("Format: LDR raw, estimated state");
}

void loop() {
  if (millis() - lastReadAt < READ_INTERVAL_MS) {
    return;
  }
  lastReadAt = millis();

  int ldrRaw = analogRead(PIN_LDR);
  bool isDark = ldrRaw < DARK_THRESHOLD;

  Serial.print("LDR: ");
  Serial.print(ldrRaw);
  Serial.print(", State: ");
  Serial.println(isDark ? "Dark" : "Bright");
}
