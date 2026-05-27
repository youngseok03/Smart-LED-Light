#include <Wire.h>
#include <U8x8lib.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define WS2812_PIN 18
#define WS2812_COUNT 30
#define SERIAL_BAUD 9600
#define OLED_COLS 16
#define DISPLAY_LINE_COUNT 4
#define SENSOR_INTERVAL_MS 2000
#define OLED_INTERVAL_MS 1200
#define DEBUG_INTERVAL_MS 1000
#define MODE_COUNT 4

DHT dht(DHT_PIN, DHT_TYPE);
U8X8_SH1106_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
Adafruit_NeoPixel strip(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

String displayLines[DISPLAY_LINE_COUNT] = {
  "USB Serial ready",
  "ESP32 + OLED",
  "DHT11 on GPIO4",
  "LED on GPIO18"
};
const char *modes[MODE_COUNT] = {"AUTO", "MANUAL", "REST", "STUDY"};

bool ledPower = true;
int ledBrightness = 40;
int modeIndex = 0;
float temperature = NAN;
float humidity = NAN;
unsigned long lastSensorRead = 0;
unsigned long lastOledUpdate = 0;
unsigned long lastDebugPrint = 0;
int oledPage = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.setPowerSave(0);
  oled.clearDisplay();
  drawPage("ESP32 USB LED", "GPIO18 WS2812", "GPIO4 DHT11", "21 SDA 22 SCL", "");

  dht.begin();
  strip.begin();
  strip.clear();
  strip.show();
  applyLedOutput();

  Serial.println("READY,ESP32_USB_SERIAL");
}

void loop() {
  readSerialData();
  readSensors();
  updateOled();
  printDebug();
}

void readSerialData() {
  if (Serial.available() <= 0) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.startsWith("CMD,POWER,")) {
    handlePowerCommand(line.substring(10));
    return;
  }

  if (line.startsWith("CMD,BRIGHT,")) {
    handleBrightnessCommand(line.substring(11));
    return;
  }

  if (line.startsWith("CMD,MODE,")) {
    handleModeCommand(line.substring(9));
    return;
  }

  if (line.startsWith("DATA,")) {
    handleDisplayData(line.substring(5));
    return;
  }

  if (line.startsWith("WEA,")) {
    handleWeatherData(line);
  }
}

void handlePowerCommand(String command) {
  command.trim();
  command.toUpperCase();
  ledPower = command == "ON" || command == "1" || command == "TRUE";
  applyLedOutput();
  drawPage("USB POWER", ledPower ? "LED ON" : "LED OFF", "Brightness " + String(ledBrightness) + "%", "Mode " + String(modes[modeIndex]), "");
  lastOledUpdate = millis();
  Serial.println(ledPower ? "ACK,POWER,ON" : "ACK,POWER,OFF");
}

void handleBrightnessCommand(String value) {
  ledBrightness = constrain(value.toInt(), 0, 100);
  ledPower = ledBrightness > 0;
  applyLedOutput();
  drawPage("USB BRIGHT", "LED " + String(ledBrightness) + "%", ledPower ? "Power ON" : "Power OFF", "Mode " + String(modes[modeIndex]), "");
  lastOledUpdate = millis();
  Serial.print("ACK,BRIGHT,");
  Serial.println(ledBrightness);
}

void handleModeCommand(String mode) {
  mode.trim();
  mode.toUpperCase();

  for (int index = 0; index < MODE_COUNT; index++) {
    if (mode == modes[index]) {
      modeIndex = index;
      applyLedOutput();
      drawPage("USB MODE", "Mode " + String(modes[modeIndex]), "LED " + String(ledBrightness) + "%", ledPower ? "Power ON" : "Power OFF", "");
      lastOledUpdate = millis();
      Serial.print("ACK,MODE,");
      Serial.println(modes[modeIndex]);
      return;
    }
  }

  Serial.println("ERR,MODE");
}

void handleDisplayData(String payload) {
  int start = 0;
  for (int row = 0; row < DISPLAY_LINE_COUNT; row++) {
    if (start > payload.length()) {
      displayLines[row] = "";
      continue;
    }

    int separator = payload.indexOf('|', start);
    if (separator == -1) {
      displayLines[row] = payload.substring(start);
      start = payload.length() + 1;
    } else {
      displayLines[row] = payload.substring(start, separator);
      start = separator + 1;
    }
    displayLines[row].trim();
  }

  drawPage("USB DATA", displayLines[0], displayLines[1], displayLines[2], displayLines[3]);
  lastOledUpdate = millis();
  Serial.println("ACK,DATA");
}

void handleWeatherData(String line) {
  int firstComma = line.indexOf(',');
  int secondComma = line.indexOf(',', firstComma + 1);
  int thirdComma = line.indexOf(',', secondComma + 1);

  if (firstComma == -1 || secondComma == -1 || thirdComma == -1) {
    Serial.println("ERR,WEA");
    return;
  }

  displayLines[0] = "Weather " + line.substring(firstComma + 1, secondComma);
  displayLines[1] = "Rain " + line.substring(secondComma + 1, thirdComma);
  displayLines[2] = "Wind " + line.substring(thirdComma + 1);
  drawPage("USB WEATHER", displayLines[0], displayLines[1], displayLines[2], displayLines[3]);
  lastOledUpdate = millis();
  Serial.println("ACK,WEA");
}

void readSensors() {
  if (millis() - lastSensorRead < SENSOR_INTERVAL_MS) {
    return;
  }

  lastSensorRead = millis();
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (!isnan(newHumidity) && !isnan(newTemperature)) {
    humidity = newHumidity;
    temperature = newTemperature;
  }
}

void updateOled() {
  if (millis() - lastOledUpdate < OLED_INTERVAL_MS) {
    return;
  }

  lastOledUpdate = millis();
  oledPage = (oledPage + 1) % 3;

  if (oledPage == 0) {
    drawPage("ESP32 USB", "LED " + String(ledPower ? "ON " : "OFF ") + String(ledBrightness) + "%", "Mode " + String(modes[modeIndex]), "Serial 9600", "");
  } else if (oledPage == 1) {
    drawPage("DHT11 GPIO4", "Temp " + sensorValueText(temperature) + "C", "Humidity " + sensorValueText(humidity) + "%", "USB bridge OK", "");
  } else {
    drawPage("Bridge DATA", displayLines[0], displayLines[1], displayLines[2], displayLines[3]);
  }
}

void printDebug() {
  if (millis() - lastDebugPrint < DEBUG_INTERVAL_MS) {
    return;
  }

  lastDebugPrint = millis();
  Serial.print("DEBUG,TEMP=");
  printDebugFloat(temperature);
  Serial.print(",HUM=");
  printDebugFloat(humidity);
  Serial.print(",LED_POWER=");
  Serial.print(ledPower ? 1 : 0);
  Serial.print(",BRIGHT=");
  Serial.print(ledBrightness);
  Serial.print(",MODE=");
  Serial.println(modes[modeIndex]);
}

void printDebugFloat(float value) {
  if (isnan(value)) {
    Serial.print("null");
  } else {
    Serial.print(value, 1);
  }
}

void applyLedOutput() {
  strip.setBrightness(ledPower ? map(ledBrightness, 0, 100, 0, 180) : 0);
  uint32_t color = ledPower ? currentLedColor() : strip.Color(0, 0, 0);

  for (int index = 0; index < WS2812_COUNT; index++) {
    strip.setPixelColor(index, color);
  }

  strip.show();
}

uint32_t currentLedColor() {
  if (modeIndex == 1) {
    return strip.Color(255, 245, 220);
  }
  if (modeIndex == 2) {
    return strip.Color(255, 120, 70);
  }
  if (modeIndex == 3) {
    return strip.Color(180, 220, 255);
  }
  return strip.Color(200, 255, 220);
}

String sensorValueText(float value) {
  if (isnan(value)) {
    return "--";
  }
  return String(value, 1);
}

String fitToOled(String text) {
  text.replace("\r", " ");
  text.replace("\n", " ");
  if (text.length() > OLED_COLS) {
    return text.substring(0, OLED_COLS);
  }
  while (text.length() < OLED_COLS) {
    text += " ";
  }
  return text;
}

void drawText(uint8_t col, uint8_t row, String text) {
  oled.drawString(col, row, fitToOled(text).c_str());
}

void drawPage(String title, String line1, String line2, String line3, String line4) {
  oled.setPowerSave(0);
  oled.clearDisplay();
  drawText(0, 0, title);
  drawText(0, 1, "----------------");
  drawText(0, 2, line1);
  drawText(0, 3, line2);
  drawText(0, 4, line3);
  drawText(0, 5, line4);
}
