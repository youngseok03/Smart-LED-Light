#include <Arduino.h>
#include <DHT.h>

/*
  DHT11 standalone test for ESP32 DevKit V1

  Wiring:
    DHT11 VCC  -> ESP32 3V3
    DHT11 GND  -> ESP32 GND
    DHT11 DATA -> GPIO4
    10k pull-up resistor between DATA and 3V3 is recommended.

  Serial Monitor: 115200 baud
*/

const uint8_t PIN_DHT = 4;
const uint8_t DHT_TYPE = DHT11;
const uint32_t READ_INTERVAL_MS = 2000;

DHT dht(PIN_DHT, DHT_TYPE);
uint32_t lastReadAt = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  dht.begin();
  Serial.println("DHT11 test started");
}

void loop() {
  if (millis() - lastReadAt < READ_INTERVAL_MS) {
    return;
  }
  lastReadAt = millis();

  float temperatureC = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperatureC) || isnan(humidity)) {
    Serial.println("DHT11 read failed");
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperatureC, 1);
  Serial.print(" C, Humidity: ");
  Serial.print(humidity, 0);
  Serial.println(" %");
}
