#include <Wire.h>
#include <U8x8lib.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define WS2812_PIN 18
#define WS2812_COUNT 24
#define POWER_BUTTON_PIN 26
#define MODE_BUTTON_PIN 25
#define DUST_ADC_PIN 35
#define DUST_LED_PIN 27
#define SERIAL_BAUD 9600
#define OLED_COLS 16
#define DISPLAY_LINE_COUNT 4
#define SENSOR_INTERVAL_MS 2000
#define DUST_INTERVAL_MS 1000
#define OLED_INTERVAL_MS 1200
#define DEBUG_INTERVAL_MS 1000
#define BUTTON_DEBOUNCE_MS 45
#define LED_FADE_STEP_MS 18
#define LED_FADE_STEPS 45
#define BREATH_PERIOD_MS 3200
#define BREATH_MIN_PERCENT 18
#define MODE_COUNT 4
#define BRIGHTNESS_STEP_COUNT 6
#define DUST_CLEAN_AIR_VOLTAGE 0.6
#define DUST_UG_PER_M3_PER_VOLT 200.0

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
const int brightnessSteps[BRIGHTNESS_STEP_COUNT] = {0, 20, 40, 60, 80, 100};

bool ledPower = true;
int ledBrightness = 40;
int modeIndex = 0;
int manualRed = 255;
int manualGreen = 245;
int manualBlue = 220;
int currentRed = 0;
int currentGreen = 0;
int currentBlue = 0;
int targetRed = 200;
int targetGreen = 255;
int targetBlue = 220;
int fadeStartRed = 0;
int fadeStartGreen = 0;
int fadeStartBlue = 0;
unsigned long fadeStartMs = 0;
bool fadeActive = false;
float temperature = NAN;
float humidity = NAN;
int dustRaw = 0;
float dustPinVoltage = 0;
float dustSensorVoltage = 0;
int dustUg = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDustRead = 0;
unsigned long lastOledUpdate = 0;
unsigned long lastDebugPrint = 0;
int oledPage = 0;
bool lastPowerButtonReading = HIGH;
bool stablePowerButtonState = HIGH;
unsigned long lastPowerButtonChange = 0;
bool lastModeButtonReading = HIGH;
bool stableModeButtonState = HIGH;
unsigned long lastModeButtonChange = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.setPowerSave(0);
  oled.clearDisplay();
  drawPage("ESP32 USB LED", "GPIO18 WS2812", "GPIO4 DHT11", "21 SDA 22 SCL", "");

  dht.begin();
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);
  analogSetPinAttenuation(DUST_ADC_PIN, ADC_11db);
  strip.begin();
  strip.clear();
  strip.show();
  applyLedOutput();

  Serial.println("READY,ESP32_USB_SERIAL");
}

void loop() {
  readSerialData();
  readButtons();
  updateLedFade();
  updateBreathingEffect();
  readSensors();
  updateOled();
  printDebug();
}

void readButtons() {
  if (wasButtonPressed(POWER_BUTTON_PIN, lastPowerButtonReading, stablePowerButtonState, lastPowerButtonChange)) {
    cycleHardwareBrightness();
    startLedFade();
    drawPage("Button GPIO26", "Bright " + String(ledBrightness) + "%", ledPower ? "LED active" : "LED off", "Mode " + String(modes[modeIndex]), "");
    lastOledUpdate = millis();
    Serial.print("ACK,BUTTON_BRIGHT,");
    Serial.println(ledBrightness);
  }

  if (wasButtonPressed(MODE_BUTTON_PIN, lastModeButtonReading, stableModeButtonState, lastModeButtonChange)) {
    setModeByIndex((modeIndex + 1) % MODE_COUNT);
    drawPage("Button GPIO25", "Mode " + String(modes[modeIndex]), "LED " + String(ledBrightness) + "%", ledPower ? "Power ON" : "Power OFF", "");
    lastOledUpdate = millis();
    Serial.print("ACK,BUTTON_MODE,");
    Serial.println(modes[modeIndex]);
  }
}

void cycleHardwareBrightness() {
  int currentBrightness = ledPower ? ledBrightness : 0;
  for (int index = 0; index < BRIGHTNESS_STEP_COUNT; index++) {
    if (currentBrightness < brightnessSteps[index]) {
      ledBrightness = brightnessSteps[index];
      ledPower = ledBrightness > 0;
      return;
    }
  }

  ledBrightness = 0;
  ledPower = false;
}

bool wasButtonPressed(uint8_t pin, bool &lastReading, bool &stableState, unsigned long &lastChange) {
  bool reading = digitalRead(pin);

  if (reading != lastReading) {
    lastChange = millis();
    lastReading = reading;
  }

  if ((millis() - lastChange) > BUTTON_DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    return stableState == LOW;
  }

  return false;
}

void setModeByIndex(int nextModeIndex) {
  modeIndex = (nextModeIndex + MODE_COUNT) % MODE_COUNT;
  startLedFade();
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

  if (line.startsWith("CMD,COLOR,")) {
    handleColorCommand(line.substring(10));
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
  startLedFade();
  drawPage("USB POWER", ledPower ? "LED ON" : "LED OFF", "Brightness " + String(ledBrightness) + "%", "Mode " + String(modes[modeIndex]), "");
  lastOledUpdate = millis();
  Serial.println(ledPower ? "ACK,POWER,ON" : "ACK,POWER,OFF");
}

void handleBrightnessCommand(String value) {
  ledBrightness = constrain(value.toInt(), 0, 100);
  ledPower = ledBrightness > 0;
  startLedFade();
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
      setModeByIndex(index);
      drawPage("USB MODE", "Mode " + String(modes[modeIndex]), "LED " + String(ledBrightness) + "%", ledPower ? "Power ON" : "Power OFF", "");
      lastOledUpdate = millis();
      Serial.print("ACK,MODE,");
      Serial.println(modes[modeIndex]);
      return;
    }
  }

  Serial.println("ERR,MODE");
}

void handleColorCommand(String payload) {
  int firstComma = payload.indexOf(',');
  int secondComma = payload.indexOf(',', firstComma + 1);

  if (firstComma == -1 || secondComma == -1) {
    Serial.println("ERR,COLOR");
    return;
  }

  manualRed = constrain(payload.substring(0, firstComma).toInt(), 0, 255);
  manualGreen = constrain(payload.substring(firstComma + 1, secondComma).toInt(), 0, 255);
  manualBlue = constrain(payload.substring(secondComma + 1).toInt(), 0, 255);
  ledPower = ledBrightness > 0;
  setModeByIndex(1);
  drawPage("Manual Color", "R" + String(manualRed) + " G" + String(manualGreen), "B" + String(manualBlue), "Mode MANUAL", "");
  lastOledUpdate = millis();
  Serial.print("ACK,COLOR,");
  Serial.print(manualRed);
  Serial.print(",");
  Serial.print(manualGreen);
  Serial.print(",");
  Serial.println(manualBlue);
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
    if (millis() - lastDustRead >= DUST_INTERVAL_MS) {
      lastDustRead = millis();
      readDustSensor();
    }
    return;
  }

  lastSensorRead = millis();
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (!isnan(newHumidity) && !isnan(newTemperature)) {
    humidity = newHumidity;
    temperature = newTemperature;
  }

  if (millis() - lastDustRead >= DUST_INTERVAL_MS) {
    lastDustRead = millis();
    readDustSensor();
  }
}

void readDustSensor() {
  digitalWrite(DUST_LED_PIN, LOW);
  delayMicroseconds(280);
  dustRaw = analogRead(DUST_ADC_PIN);
  uint32_t pinMilliVolts = analogReadMilliVolts(DUST_ADC_PIN);
  delayMicroseconds(40);
  digitalWrite(DUST_LED_PIN, HIGH);
  delayMicroseconds(9680);

  dustPinVoltage = pinMilliVolts / 1000.0;
  dustSensorVoltage = dustPinVoltage;
  float density = (dustSensorVoltage - DUST_CLEAN_AIR_VOLTAGE) * DUST_UG_PER_M3_PER_VOLT;
  if (density < 0) {
    density = 0;
  }
  if (density > 500) {
    density = 500;
  }
  dustUg = (int)density;
}

void updateOled() {
  if (millis() - lastOledUpdate < OLED_INTERVAL_MS) {
    return;
  }

  lastOledUpdate = millis();
  oledPage = (oledPage + 1) % 4;

  if (oledPage == 0) {
    drawPage("ESP32 USB", "LED " + String(ledPower ? "ON " : "OFF ") + String(ledBrightness) + "%", "Mode " + String(modes[modeIndex]), "Serial 9600", "");
  } else if (oledPage == 1) {
    drawPage("DHT11 GPIO4", "Temp " + sensorValueText(temperature) + "C", "Humidity " + sensorValueText(humidity) + "%", "USB bridge OK", "");
  } else if (oledPage == 2) {
    drawPage("Dust GPIO35", "Raw " + String(dustRaw), "Vo " + String(dustSensorVoltage, 2) + "V", "Dust " + String(dustUg) + "ug", "");
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
  Serial.print(modes[modeIndex]);
  Serial.print(",RGB=");
  Serial.print(manualRed);
  Serial.print("-");
  Serial.print(manualGreen);
  Serial.print("-");
  Serial.print(manualBlue);
  Serial.print(",DUST_RAW=");
  Serial.print(dustRaw);
  Serial.print(",DUST_V=");
  Serial.print(dustSensorVoltage, 2);
  Serial.print(",DUST_UG=");
  Serial.println(dustUg);
}

void printDebugFloat(float value) {
  if (isnan(value)) {
    Serial.print("null");
  } else {
    Serial.print(value, 1);
  }
}

void applyLedOutput() {
  applyLedOutputWithBrightness(ledBrightness);
}

void applyLedOutputWithBrightness(int brightnessPercent) {
  strip.setBrightness(ledPower ? map(brightnessPercent, 0, 100, 0, 180) : 0);
  uint32_t color = strip.Color(currentRed, currentGreen, currentBlue);

  for (int index = 0; index < WS2812_COUNT; index++) {
    strip.setPixelColor(index, color);
  }

  strip.show();
}

void startLedFade() {
  fadeStartRed = currentRed;
  fadeStartGreen = currentGreen;
  fadeStartBlue = currentBlue;
  setTargetColor();
  fadeStartMs = millis();
  fadeActive = true;
  updateLedFade();
}

void updateLedFade() {
  if (!fadeActive) {
    return;
  }

  unsigned long elapsed = millis() - fadeStartMs;
  int step = min((int)(elapsed / LED_FADE_STEP_MS), LED_FADE_STEPS);

  currentRed = fadeStartRed + ((targetRed - fadeStartRed) * step) / LED_FADE_STEPS;
  currentGreen = fadeStartGreen + ((targetGreen - fadeStartGreen) * step) / LED_FADE_STEPS;
  currentBlue = fadeStartBlue + ((targetBlue - fadeStartBlue) * step) / LED_FADE_STEPS;
  applyModeLedOutput();

  if (step >= LED_FADE_STEPS) {
    fadeActive = false;
  }
}

void updateBreathingEffect() {
  if (fadeActive || !isBreathingMode()) {
    return;
  }

  applyModeLedOutput();
}

void applyModeLedOutput() {
  if (isBreathingMode()) {
    applyLedOutputWithBrightness(breathingBrightness());
    return;
  }

  applyLedOutputWithBrightness(ledBrightness);
}

bool isBreathingMode() {
  return ledPower && ledBrightness > 0 && (modeIndex == 2 || modeIndex == 3);
}

int breathingBrightness() {
  float phase = (millis() % BREATH_PERIOD_MS) / (float)BREATH_PERIOD_MS;
  float wave = (1.0 - cos(phase * TWO_PI)) * 0.5;
  int minimum = max(1, (ledBrightness * BREATH_MIN_PERCENT) / 100);
  return minimum + (int)((ledBrightness - minimum) * wave);
}

void setTargetColor() {
  if (!ledPower || ledBrightness <= 0) {
    targetRed = 0;
    targetGreen = 0;
    targetBlue = 0;
    return;
  }

  if (modeIndex == 1) {
    targetRed = manualRed;
    targetGreen = manualGreen;
    targetBlue = manualBlue;
    return;
  }
  if (modeIndex == 2) {
    targetRed = 255;
    targetGreen = 120;
    targetBlue = 70;
    return;
  }
  if (modeIndex == 3) {
    targetRed = 180;
    targetGreen = 220;
    targetBlue = 255;
    return;
  }
  targetRed = 200;
  targetGreen = 255;
  targetBlue = 220;
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
