#include <Wire.h>
#include <U8x8lib.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

#define DHT_PIN 7
#define DHT_TYPE DHT11
#define PIR_PIN 2
#define LDR_PIN A0
#define LED_PIN 5
#define LED_COUNT 24
#define STATUS_LED_PIN LED_BUILTIN
#define BUTTON_BRIGHTNESS_PIN 8
#define BUTTON_MODE_PIN 9
#define SERIAL_BAUD 9600
#define DISPLAY_LINE_COUNT 4
#define DISPLAY_PAGE_COUNT 3
#define OLED_COLS 16
#define BUTTON_DEBOUNCE_MS 45
#define MODE_COUNT 4
#define LDR_DARK_THRESHOLD 400
#define SENSOR_DEBUG_INTERVAL_MS 1000
#define DISPLAY_REFRESH_MS 1000

DHT dht(DHT_PIN, DHT_TYPE);
U8X8_SH1106_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

uint8_t hangulHanTop[] = {
  0x40, 0xA4, 0x15, 0x15, 0x17, 0x15, 0x15, 0xA4,
  0x44, 0x00, 0x20, 0x20, 0xFE, 0x20, 0x20, 0x00
};
uint8_t hangulHanBottom[] = {
  0x00, 0x00, 0xF9, 0x81, 0x81, 0x81, 0x81, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x83, 0x00, 0x00, 0x00
};
uint8_t hangulGeulTop[] = {
  0x00, 0x00, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
  0x82, 0x82, 0x82, 0xBE, 0x80, 0x80, 0x00, 0x00
};
uint8_t hangulGeulBottom[] = {
  0x00, 0x00, 0x74, 0x54, 0x54, 0x54, 0x54, 0x54,
  0x54, 0x54, 0x54, 0x54, 0x5C, 0x00, 0x00, 0x00
};

String apiWeather = "No API";
String apiRain = "-";
String apiWind = "-";
String aiMessage = "AI msg standby";
String displayLines[DISPLAY_LINE_COUNT] = {
  "Smart LED ready",
  "Uno R3 + OLED",
  "Waiting bridge",
  "Use web power"
};

unsigned long lastDisplay = 0;
unsigned long lastSensorDebug = 0;
int displayPage = 0;
bool ledPower = true;
int ledBrightness = 70;
bool lastBrightnessButtonReading = HIGH;
bool stableBrightnessButtonState = HIGH;
unsigned long lastBrightnessButtonChange = 0;
bool lastModeButtonReading = HIGH;
bool stableModeButtonState = HIGH;
unsigned long lastModeButtonChange = 0;
int colorModeIndex = 0;
const char *colorModes[MODE_COUNT] = {"AUTO", "MANUAL", "REST", "STUDY"};

void setup() {
  Wire.begin();
  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.setPowerSave(0);
  oled.clearDisplay();
  oled.drawString(0, 0, "SMART BOOT");
  oled.drawString(0, 2, "OLED first");
  oled.drawString(0, 4, "A4 SDA A5 SCL");
  delay(1200);

  Serial.begin(SERIAL_BAUD);
  dht.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(BUTTON_BRIGHTNESS_PIN, INPUT_PULLUP);
  pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);
  digitalWrite(STATUS_LED_PIN, LOW);
  strip.begin();
  strip.clear();
  strip.show();
  applyLedOutput();

  showKoreanSplash();
  delay(1500);
  showBootScreen();

  delay(1500);
}

void loop() {
  readSerialData();
  readButtonInputs();
  updateMotionStatusLed();
  printSensorDebug();
  keepDisplayAwake();

  if (millis() - lastDisplay >= DISPLAY_REFRESH_MS) {
    lastDisplay = millis();
    displayPage = (displayPage + 1) % DISPLAY_PAGE_COUNT;
    displayCurrentPage();
  }
}

void readSerialData() {
  if (Serial.available() <= 0) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.startsWith("CMD,LCD,")) {
    handlePowerCommand(line.substring(8));
    return;
  }

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

  if (!line.startsWith("WEA,")) {
    return;
  }

  int firstComma = line.indexOf(',');
  int secondComma = line.indexOf(',', firstComma + 1);
  int thirdComma = line.indexOf(',', secondComma + 1);

  if (firstComma == -1 || secondComma == -1 || thirdComma == -1) {
    return;
  }

  apiWeather = line.substring(firstComma + 1, secondComma);
  apiRain = line.substring(secondComma + 1, thirdComma);
  apiWind = line.substring(thirdComma + 1);
  displayLines[0] = "Weather: " + apiWeather;
  displayLines[1] = "Rain: " + apiRain + "mm";
  displayLines[2] = "Wind: " + apiWind + "m/s";
  displayLines[3] = aiMessage;
  displayApiData();
}

void handlePowerCommand(String command) {
  command.trim();

  if (command == "ON") {
    ledPower = true;
    applyLedOutput();
    drawPage("LED Power", "LED ON", "Brightness: " + String(ledBrightness) + "%", "Web/Button OK", "");
    lastDisplay = 0;
  } else if (command == "OFF") {
    ledPower = false;
    applyLedOutput();
    drawPage("LED Power", "LED OFF", "Display stays ON", "Web/Button OK", "");
    lastDisplay = 0;
  }
}

void handleBrightnessCommand(String value) {
  value.trim();
  ledBrightness = constrain(value.toInt(), 0, 100);
  ledPower = ledBrightness > 0;
  applyLedOutput();

  drawPage(
    "Brightness",
    "LED: " + String(ledBrightness) + "%",
    ledPower ? "Power: ON" : "Power: OFF",
    "WS2812 DIN: D5",
    ""
  );
  lastDisplay = 0;
}

void handleModeCommand(String mode) {
  mode.trim();
  mode.toUpperCase();

  for (int index = 0; index < MODE_COUNT; index++) {
    if (mode == colorModes[index]) {
      colorModeIndex = index;
      applyLedOutput();
      showModeStatus("Web mode");
      return;
    }
  }
}

void readButtonInputs() {
  if (wasButtonPressed(BUTTON_BRIGHTNESS_PIN, lastBrightnessButtonReading, stableBrightnessButtonState, lastBrightnessButtonChange)) {
    ledBrightness += 20;
    if (ledBrightness > 100) {
      ledBrightness = 0;
    }
    ledPower = ledBrightness > 0;
    applyLedOutput();
    drawPage(
      "Button D8",
      "LED: " + String(ledBrightness) + "%",
      ledPower ? "Power: ON" : "Power: OFF",
      "0..100 step20",
      ""
    );
    lastDisplay = 0;
  }

  if (wasButtonPressed(BUTTON_MODE_PIN, lastModeButtonReading, stableModeButtonState, lastModeButtonChange)) {
    colorModeIndex = (colorModeIndex + 1) % MODE_COUNT;
    applyLedOutput();
    showModeStatus("Button D9");
  }
}

void showModeStatus(String source) {
  drawPage(
    source,
    "Mode: " + String(colorModes[colorModeIndex]),
    "Color mode only",
    "RGB later ready",
    ""
  );
  lastDisplay = 0;
}

bool wasButtonPressed(uint8_t pin, bool &lastReading, bool &stableState, unsigned long &lastChange) {
  bool reading = digitalRead(pin);

  if (reading != lastReading) {
    lastChange = millis();
    lastReading = reading;
  }

  if ((millis() - lastChange) > BUTTON_DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      return true;
    }
  }

  return false;
}

void applyLedOutput() {
  uint32_t color = ledPower ? currentStripColor() : strip.Color(0, 0, 0);

  strip.setBrightness(ledPower ? map(ledBrightness, 0, 100, 0, 180) : 0);
  for (int index = 0; index < LED_COUNT; index++) {
    strip.setPixelColor(index, color);
  }
  strip.show();
}

uint32_t currentStripColor() {
  if (colorModeIndex == 1) {
    return strip.Color(255, 245, 220);
  }
  if (colorModeIndex == 2) {
    return strip.Color(255, 120, 70);
  }
  if (colorModeIndex == 3) {
    return strip.Color(180, 220, 255);
  }
  return strip.Color(200, 255, 220);
}

bool isRoomDark(int rawLight) {
  return rawLight < LDR_DARK_THRESHOLD;
}

bool isPirMotionDetected() {
  return digitalRead(PIR_PIN) == HIGH;
}

void updateMotionStatusLed() {
  int rawLight = analogRead(LDR_PIN);
  bool dark = isRoomDark(rawLight);
  bool motion = isPirMotionDetected();
  digitalWrite(STATUS_LED_PIN, dark && motion ? HIGH : LOW);
}

void printSensorDebug() {
  if (millis() - lastSensorDebug < SENSOR_DEBUG_INTERVAL_MS) {
    return;
  }

  lastSensorDebug = millis();
  int rawLight = analogRead(LDR_PIN);
  bool dark = isRoomDark(rawLight);
  bool motion = isPirMotionDetected();
  bool onboardLedOn = dark && motion;

  Serial.print("DEBUG,LDR=");
  Serial.print(rawLight);
  Serial.print(",DARK=");
  Serial.print(dark ? 1 : 0);
  Serial.print(",PIR=");
  Serial.print(motion ? 1 : 0);
  Serial.print(",ONBOARD_LED=");
  Serial.println(onboardLedOn ? 1 : 0);
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

  aiMessage = displayLines[DISPLAY_LINE_COUNT - 1];
  displayApiData();
}

void showBootScreen() {
  drawPage("Smart LED", displayLines[0], displayLines[1], displayLines[2], displayLines[3]);
}

void keepDisplayAwake() {
  oled.setPowerSave(0);
}

void showKoreanSplash() {
  oled.clearDisplay();
  drawText(0, 0, "Korean bitmap");
  oled.drawTile(4, 2, 2, hangulHanTop);
  oled.drawTile(4, 3, 2, hangulHanBottom);
  oled.drawTile(8, 2, 2, hangulGeulTop);
  oled.drawTile(8, 3, 2, hangulGeulBottom);
  drawText(0, 6, "Hangul once");
}

void displayCurrentPage() {
  if (displayPage == 0) {
    displayDhtData();
  } else if (displayPage == 1) {
    displayLightData();
  } else {
    displayApiData();
  }
}

void displayDhtData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    drawPage("Indoor", "DHT11: no data", "Check 5V/GND/D7", "OLED still OK", "");
    return;
  }

  drawPage(
    "Indoor",
    "Temp: " + String((int)temperature) + "C",
    "Humidity: " + String((int)humidity) + "%",
    "LED: " + String(ledBrightness) + "%",
    "Mode: " + String(colorModes[colorModeIndex])
  );
}

void displayLightData() {
  int raw = analogRead(LDR_PIN);
  int percent = map(raw, 0, 1023, 0, 100);
  bool dark = isRoomDark(raw);
  bool motion = isPirMotionDetected();
  bool onboardLedOn = dark && motion;

  drawPage(
    "Light / PIR",
    "Raw:" + String(raw) + " " + String(percent) + "%",
    dark ? "Light: Dark" : "Light: Bright",
    motion ? "PIR: Motion" : "PIR: No motion",
    onboardLedOn ? "OnLED: ON" : "OnLED: OFF"
  );
}

void displayApiData() {
  drawPage("Weather / AI", displayLines[0], displayLines[1], displayLines[2], displayLines[3]);
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
  String fitted = fitToOled(text);
  oled.drawString(col, row, fitted.c_str());
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
