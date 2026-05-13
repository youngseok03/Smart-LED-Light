#include <Arduino.h>

/*
  GP2Y1014AU dust sensor standalone test for ESP32 DevKit V1

  Wiring used by the main project:
    Pin 1 V-LED -> 150 ohm -> external 5V
    Pin 2 LED-GND -> GND
    Pin 3 LED -> NPN/MOSFET driver controlled by GPIO23
    Pin 4 S-GND -> ESP32 GND
    Pin 5 Vo -> voltage divider -> GPIO35
    Pin 6 Vcc -> external 5V
    220 uF capacitor between V-LED and GND is recommended.

  Vo protection divider:
    GP2Y Vo -> 10k -> GPIO35 -> 20k -> GND
    DUST_DIVIDER_SCALE = 1.5 restores the approximate sensor voltage.

  Serial Monitor: 115200 baud
*/

const uint8_t PIN_DUST_ANALOG = 35;  // ADC1 input-only pin
const uint8_t PIN_DUST_LED = 23;
const bool DUST_LED_ACTIVE_HIGH = true;

const uint32_t READ_INTERVAL_MS = 1000;
const float DUST_DIVIDER_SCALE = 1.5;
const float DUST_NO_DUST_VOLTAGE = 0.60;  // Typical baseline. Calibrate it.
const float DUST_SENSITIVITY = 0.50;      // V per 100 ug/m3, rough GP2Y101x value.

uint32_t lastReadAt = 0;

void setDustLed(bool on) {
  digitalWrite(PIN_DUST_LED, on == DUST_LED_ACTIVE_HIGH ? HIGH : LOW);
}

float readDustUgM3(int &rawAdc, float &sensorVoltage) {
  setDustLed(true);
  delayMicroseconds(280);
  rawAdc = analogRead(PIN_DUST_ANALOG);
  delayMicroseconds(40);
  setDustLed(false);
  delayMicroseconds(9680);

  float measuredVoltage = (rawAdc / 4095.0) * 3.3;
  sensorVoltage = measuredVoltage * DUST_DIVIDER_SCALE;

  float dust = (sensorVoltage - DUST_NO_DUST_VOLTAGE) / DUST_SENSITIVITY * 100.0;
  if (dust < 0.0) {
    dust = 0.0;
  }
  return dust;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_DUST_LED, OUTPUT);
  setDustLed(false);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_DUST_ANALOG, ADC_11db);

  Serial.println("GP2Y1014AU dust sensor test started");
  Serial.println("Format: raw ADC, sensor voltage, estimated dust");
}

void loop() {
  if (millis() - lastReadAt < READ_INTERVAL_MS) {
    return;
  }
  lastReadAt = millis();

  int rawAdc = 0;
  float sensorVoltage = 0.0;
  float dustUgM3 = readDustUgM3(rawAdc, sensorVoltage);

  Serial.print("Raw: ");
  Serial.print(rawAdc);
  Serial.print(", Vo: ");
  Serial.print(sensorVoltage, 3);
  Serial.print(" V, Dust: ");
  Serial.print(dustUgM3, 1);
  Serial.println(" ug/m3");
}
