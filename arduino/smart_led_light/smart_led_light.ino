#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

/*
  Smart LED Light - ESP32 version

  Board target: ESP32 Dev Module / ESP32 DevKit V1
  Required libraries:
    - DHT sensor library
    - Adafruit Unified Sensor
    - Adafruit NeoPixel
    - U8g2

  Serial command examples from PC/Python:
    RGB,120,240,230
    MODE,AUTO
    MODE,MANUAL
    POWER,1
    BRIGHT,80
    WEATHER,Clear,24,45,0,Have a good day
    MSG,Take an umbrella
*/

// ---------- User settings ----------
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";
const char *AP_SSID = "SmartLight-ESP32";
const char *AP_PASSWORD = "12345678";

// ---------- Pin map ----------
const uint8_t PIN_DHT = 4;
const uint8_t PIN_NEOPIXEL = 18;
const uint8_t PIN_FOOT_LED = 19;
const uint8_t PIN_PIR = 27;
const uint8_t PIN_LDR = 34;          // ADC1 input only
const uint8_t PIN_DUST_ANALOG = 35;  // ADC1 input only
const uint8_t PIN_DUST_LED = 23;
const uint8_t PIN_BUTTON_MODE = 25;
const uint8_t PIN_BUTTON_POWER = 26;

const uint8_t I2C_SDA = 21;
const uint8_t I2C_SCL = 22;

// ---------- Hardware constants ----------
const uint8_t DHT_TYPE = DHT11;
const uint8_t OLED_I2C_ADDR = 0x3C;  // Try 0x3D if an I2C scanner finds that address.
const uint8_t OLED_WIDTH = 128;
const uint8_t OLED_HEIGHT = 64;
const uint16_t MOOD_LED_COUNT = 16;

const int DARK_THRESHOLD = 1600;       // Calibrate after checking raw LDR values.
const uint32_t FOOT_HOLD_MS = 20000;   // Keep foot light on after last motion.
const uint32_t SENSOR_MS = 2000;
const uint32_t DUST_MS = 10000;
const uint32_t DISPLAY_MS = 3000;
const uint32_t STATUS_MS = 5000;
const uint32_t DEBOUNCE_MS = 35;
const uint32_t LONG_PRESS_MS = 800;

const uint8_t FOOT_PWM_CHANNEL = 0;
const uint16_t FOOT_PWM_FREQ = 5000;
const uint8_t FOOT_PWM_BITS = 8;
const uint8_t FOOT_PWM_VALUE = 90;
const bool DUST_LED_ACTIVE_HIGH = true;  // true when GPIO drives NPN/MOSFET gate/base.

// GP2Y1014AU output is scaled through a 10k/20k divider in the wiring guide.
// Sensor output voltage before divider = measured ESP32 voltage * 1.5.
const float DUST_DIVIDER_SCALE = 1.5;
const float DUST_NO_DUST_VOLTAGE = 0.60;  // Typical. Calibrate for your sensor.
const float DUST_SENSITIVITY = 0.50;      // V per 100 ug/m3, rough GP2Y101x value.

DHT dht(PIN_DHT, DHT_TYPE);
// DIS070012/YwRobot 1.3" OLED examples use the SH1106 128x64 driver.
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel moodStrip(MOOD_LED_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
WebServer server(80);

enum Mode {
  MODE_AUTO,
  MODE_MANUAL
};

struct ButtonState {
  uint8_t pin;
  bool stablePressed;
  bool lastReading;
  bool longHandled;
  uint32_t lastChange;
  uint32_t pressedAt;
};

struct WeatherState {
  String summary;
  float outdoorTemp;
  int outdoorHumidity;
  float rainMm;
  String message;
  bool hasData;
};

struct SensorState {
  float tempC;
  float humidity;
  float dustUgM3;
  int ldrRaw;
  bool dark;
  bool motion;
};

struct LightState {
  bool powerOn;
  Mode mode;
  uint8_t brightness;
  uint8_t manualIndex;
  uint8_t targetR;
  uint8_t targetG;
  uint8_t targetB;
};

ButtonState modeButton = {PIN_BUTTON_MODE, false, false, false, 0, 0};
ButtonState powerButton = {PIN_BUTTON_POWER, false, false, false, 0, 0};
WeatherState weather = {"No weather", NAN, -1, 0.0, "Ready", false};
SensorState sensors = {NAN, NAN, 0.0, 0, false, false};
LightState light = {true, MODE_AUTO, 70, 0, 210, 255, 220};

const uint8_t manualPalette[][3] = {
  {255, 240, 210},
  {210, 255, 220},
  {200, 235, 255},
  {120, 240, 230},
  {255, 180, 220},
  {255, 255, 255},
};
const uint8_t manualPaletteSize = sizeof(manualPalette) / sizeof(manualPalette[0]);

uint32_t lastSensorAt = 0;
uint32_t lastDustAt = 0;
uint32_t lastDisplayAt = 0;
uint32_t lastStatusAt = 0;
uint32_t lastMotionAt = 0;
uint8_t displayPage = 0;
String serialLine;

String modeName() {
  return light.mode == MODE_AUTO ? "AUTO" : "MANUAL";
}

void setupFootPwm() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_FOOT_LED, FOOT_PWM_FREQ, FOOT_PWM_BITS);
#else
  ledcSetup(FOOT_PWM_CHANNEL, FOOT_PWM_FREQ, FOOT_PWM_BITS);
  ledcAttachPin(PIN_FOOT_LED, FOOT_PWM_CHANNEL);
#endif
}

void writeFootPwm(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_FOOT_LED, duty);
#else
  ledcWrite(FOOT_PWM_CHANNEL, duty);
#endif
}

void setDustLed(bool on) {
  digitalWrite(PIN_DUST_LED, on == DUST_LED_ACTIVE_HIGH ? HIGH : LOW);
}

String ipText() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return WiFi.softAPIP().toString();
  }
  return WiFi.localIP().toString();
}

String cleanDisplayText(String text) {
  text.replace("\r", " ");
  text.replace("\n", " ");
  return text;
}

String clippedForOled(String text, uint8_t maxWidthPx) {
  text = cleanDisplayText(text);
  while (text.length() > 0 && oled.getStrWidth(text.c_str()) > maxWidthPx) {
    text.remove(text.length() - 1);
  }
  return text;
}

void oledText(int16_t x, int16_t y, const String &text, uint8_t maxWidthPx = OLED_WIDTH) {
  String clipped = clippedForOled(text, maxWidthPx);
  oled.drawStr(x, y, clipped.c_str());
}

void oledHeader(const String &title) {
  oled.setFont(u8g2_font_6x10_tf);
  oledText(0, 9, title);
  oled.drawHLine(0, 12, OLED_WIDTH);
}

void oledLine(uint8_t row, const String &text) {
  const uint8_t yPositions[] = {24, 36, 48, OLED_HEIGHT - 4};
  if (row >= sizeof(yPositions) / sizeof(yPositions[0])) {
    return;
  }
  oledText(0, yPositions[row], text);
}

void drawDisplayMessage(const String &title, const String &line1, const String &line2 = "", const String &line3 = "") {
  oled.clearBuffer();
  oledHeader(title);
  oledLine(0, line1);
  oledLine(1, line2);
  oledLine(2, line3);
  oled.sendBuffer();
}

void setMoodColor(uint8_t r, uint8_t g, uint8_t b) {
  light.targetR = r;
  light.targetG = g;
  light.targetB = b;
}

void applyMoodLight() {
  uint8_t r = light.powerOn ? (uint16_t(light.targetR) * light.brightness / 100) : 0;
  uint8_t g = light.powerOn ? (uint16_t(light.targetG) * light.brightness / 100) : 0;
  uint8_t b = light.powerOn ? (uint16_t(light.targetB) * light.brightness / 100) : 0;
  uint32_t color = moodStrip.Color(r, g, b);

  for (uint16_t i = 0; i < MOOD_LED_COUNT; i++) {
    moodStrip.setPixelColor(i, color);
  }
  moodStrip.show();
}

void selectManualColor(uint8_t index) {
  light.manualIndex = index % manualPaletteSize;
  setMoodColor(
    manualPalette[light.manualIndex][0],
    manualPalette[light.manualIndex][1],
    manualPalette[light.manualIndex][2]);
  applyMoodLight();
}

void selectAutoColor() {
  if (weather.hasData) {
    String weatherKey = weather.summary;
    weatherKey.toUpperCase();
    if (weather.rainMm > 0.0 || weatherKey.indexOf("RAIN") >= 0) {
      setMoodColor(200, 255, 220);
    } else if (weatherKey.indexOf("SNOW") >= 0) {
      setMoodColor(255, 240, 210);
    } else if (weatherKey.indexOf("CLOUD") >= 0) {
      setMoodColor(210, 240, 255);
    } else if (!isnan(weather.outdoorTemp) && weather.outdoorTemp >= 30.0) {
      setMoodColor(120, 240, 230);
    } else if (!isnan(weather.outdoorTemp) && weather.outdoorTemp <= 5.0) {
      setMoodColor(255, 235, 205);
    } else {
      setMoodColor(210, 255, 220);
    }
  } else if (!isnan(sensors.tempC) && sensors.tempC >= 30.0) {
    setMoodColor(120, 240, 230);
  } else if (!isnan(sensors.humidity) && sensors.humidity >= 75.0) {
    setMoodColor(210, 240, 255);
  } else {
    setMoodColor(240, 240, 240);
  }
  applyMoodLight();
}

void updateMoodByMode() {
  if (light.mode == MODE_AUTO) {
    selectAutoColor();
  } else {
    selectManualColor(light.manualIndex);
  }
}

void updateFootLight() {
  sensors.ldrRaw = analogRead(PIN_LDR);
  sensors.dark = sensors.ldrRaw < DARK_THRESHOLD;
  sensors.motion = digitalRead(PIN_PIR) == HIGH;

  if (sensors.dark && sensors.motion) {
    lastMotionAt = millis();
  }

  bool shouldLight = sensors.dark && (millis() - lastMotionAt <= FOOT_HOLD_MS);
  writeFootPwm(shouldLight ? FOOT_PWM_VALUE : 0);
}

void readDhtSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) {
    sensors.tempC = t;
  }
  if (!isnan(h)) {
    sensors.humidity = h;
  }
}

float readDustSensor() {
  setDustLed(true);
  delayMicroseconds(280);
  int raw = analogRead(PIN_DUST_ANALOG);
  delayMicroseconds(40);
  setDustLed(false);
  delayMicroseconds(9680);

  float measuredVoltage = (raw / 4095.0) * 3.3;
  float sensorVoltage = measuredVoltage * DUST_DIVIDER_SCALE;
  float dust = (sensorVoltage - DUST_NO_DUST_VOLTAGE) / DUST_SENSITIVITY * 100.0;
  if (dust < 0.0) {
    dust = 0.0;
  }
  return dust;
}

void updateSensors() {
  uint32_t now = millis();
  updateFootLight();

  if (now - lastSensorAt >= SENSOR_MS) {
    lastSensorAt = now;
    readDhtSensor();
    if (light.mode == MODE_AUTO && !weather.hasData) {
      updateMoodByMode();
    }
  }

  if (now - lastDustAt >= DUST_MS) {
    lastDustAt = now;
    sensors.dustUgM3 = readDustSensor();
  }
}

bool updateButton(ButtonState &button, bool &shortPressed, bool &longPressed) {
  shortPressed = false;
  longPressed = false;

  bool reading = digitalRead(button.pin) == LOW;
  uint32_t now = millis();

  if (reading != button.lastReading) {
    button.lastReading = reading;
    button.lastChange = now;
  }

  if (now - button.lastChange < DEBOUNCE_MS) {
    return false;
  }

  if (reading != button.stablePressed) {
    button.stablePressed = reading;
    if (button.stablePressed) {
      button.pressedAt = now;
      button.longHandled = false;
    } else if (!button.longHandled) {
      shortPressed = true;
      return true;
    }
  }

  if (button.stablePressed && !button.longHandled && now - button.pressedAt >= LONG_PRESS_MS) {
    button.longHandled = true;
    longPressed = true;
    return true;
  }

  return false;
}

void handleButtons() {
  bool shortPressed = false;
  bool longPressed = false;

  if (updateButton(modeButton, shortPressed, longPressed)) {
    if (longPressed) {
      light.mode = (light.mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
    } else {
      light.mode = MODE_MANUAL;
      light.manualIndex = (light.manualIndex + 1) % manualPaletteSize;
    }
    updateMoodByMode();
  }

  if (updateButton(powerButton, shortPressed, longPressed)) {
    if (longPressed) {
      const uint8_t levels[] = {25, 50, 75, 100};
      uint8_t next = 0;
      for (uint8_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        if (light.brightness < levels[i]) {
          next = i;
          break;
        }
      }
      if (light.brightness >= 100) {
        next = 0;
      }
      light.brightness = levels[next];
    } else {
      light.powerOn = !light.powerOn;
    }
    applyMoodLight();
  }
}

void drawDisplay() {
  oled.clearBuffer();

  if (displayPage == 0) {
    String temp = isnan(sensors.tempC) ? "--" : String(sensors.tempC, 1);
    String hum = isnan(sensors.humidity) ? "--" : String(sensors.humidity, 0);
    oledHeader("Indoor / Air");
    oledLine(0, "Temp " + temp + "C  Hum " + hum + "%");
    oledLine(1, "Dust " + String(sensors.dustUgM3, 0) + " ug/m3");
    oledLine(2, "LDR " + String(sensors.ldrRaw) + (sensors.dark ? " Dark" : " Bright"));
    oledLine(3, sensors.motion ? "Motion detected" : "No motion");
  } else if (displayPage == 1) {
    oledHeader("Weather");
    oledLine(0, weather.summary);
    if (weather.hasData) {
      oledLine(1, "Out " + String(weather.outdoorTemp, 1) + "C  " + String(weather.outdoorHumidity) + "%");
      oledLine(2, "Rain " + String(weather.rainMm, 1) + "mm");
    } else {
      oledLine(1, "Waiting PC/API");
      oledLine(2, "Serial or Web cmd");
    }
    oledLine(3, weather.message);
  } else {
    oledHeader("Mood / Network");
    oledLine(0, modeName() + String(light.powerOn ? " ON" : " OFF"));
    oledLine(1, "Bright " + String(light.brightness) + "%");
    oledLine(2, "RGB " + String(light.targetR) + "," + String(light.targetG) + "," + String(light.targetB));
    oledLine(3, "IP " + ipText());
  }

  oled.sendBuffer();
}

void updateDisplay() {
  if (millis() - lastDisplayAt < DISPLAY_MS) {
    return;
  }
  lastDisplayAt = millis();
  displayPage = (displayPage + 1) % 3;
  drawDisplay();
}

void printStatus() {
  Serial.print("SENSOR,");
  Serial.print(isnan(sensors.tempC) ? -999 : sensors.tempC, 1);
  Serial.print(",");
  Serial.print(isnan(sensors.humidity) ? -1 : sensors.humidity, 0);
  Serial.print(",");
  Serial.print(sensors.dustUgM3, 0);
  Serial.print(",");
  Serial.print(sensors.ldrRaw);
  Serial.print(",");
  Serial.print(sensors.dark ? 1 : 0);
  Serial.print(",");
  Serial.print(sensors.motion ? 1 : 0);
  Serial.print(",");
  Serial.print(modeName());
  Serial.print(",");
  Serial.print(light.powerOn ? 1 : 0);
  Serial.print(",");
  Serial.print(light.brightness);
  Serial.print(",");
  Serial.print(light.targetR);
  Serial.print(",");
  Serial.print(light.targetG);
  Serial.print(",");
  Serial.println(light.targetB);
}

void updateSerialStatus() {
  if (millis() - lastStatusAt >= STATUS_MS) {
    lastStatusAt = millis();
    printStatus();
  }
}

String token(String line, uint8_t index) {
  int start = 0;
  uint8_t current = 0;
  line.trim();
  while (current < index) {
    int comma = line.indexOf(',', start);
    if (comma < 0) {
      return "";
    }
    start = comma + 1;
    current++;
  }
  int end = line.indexOf(',', start);
  if (end < 0) {
    end = line.length();
  }
  String out = line.substring(start, end);
  out.trim();
  return out;
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  String cmd = token(line, 0);
  cmd.toUpperCase();

  if (cmd == "RGB") {
    uint8_t r = constrain(token(line, 1).toInt(), 0, 255);
    uint8_t g = constrain(token(line, 2).toInt(), 0, 255);
    uint8_t b = constrain(token(line, 3).toInt(), 0, 255);
    light.mode = MODE_AUTO;
    weather.hasData = true;
    setMoodColor(r, g, b);
    applyMoodLight();
    Serial.println("OK,RGB");
  } else if (cmd == "MODE") {
    String value = token(line, 1);
    value.toUpperCase();
    light.mode = value == "MANUAL" ? MODE_MANUAL : MODE_AUTO;
    updateMoodByMode();
    Serial.println("OK,MODE");
  } else if (cmd == "POWER") {
    light.powerOn = token(line, 1).toInt() != 0;
    applyMoodLight();
    Serial.println("OK,POWER");
  } else if (cmd == "BRIGHT") {
    light.brightness = constrain(token(line, 1).toInt(), 0, 100);
    applyMoodLight();
    Serial.println("OK,BRIGHT");
  } else if (cmd == "MSG") {
    weather.message = token(line, 1);
    drawDisplay();
    Serial.println("OK,MSG");
  } else if (cmd == "WEATHER") {
    weather.summary = token(line, 1);
    weather.outdoorTemp = token(line, 2).toFloat();
    weather.outdoorHumidity = token(line, 3).toInt();
    weather.rainMm = token(line, 4).toFloat();
    weather.message = token(line, 5);
    weather.hasData = true;
    if (light.mode == MODE_AUTO) {
      updateMoodByMode();
    }
    drawDisplay();
    Serial.println("OK,WEATHER");
  } else if (cmd == "STATUS") {
    printStatus();
  } else {
    Serial.println("ERR,UNKNOWN_COMMAND");
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(serialLine);
      serialLine = "";
    } else if (c != '\r') {
      serialLine += c;
      if (serialLine.length() > 160) {
        serialLine = "";
        Serial.println("ERR,LINE_TOO_LONG");
      }
    }
  }
}

String statusJson() {
  String json = "{";
  json += "\"tempC\":" + String(isnan(sensors.tempC) ? -999 : sensors.tempC, 1) + ",";
  json += "\"humidity\":" + String(isnan(sensors.humidity) ? -1 : sensors.humidity, 0) + ",";
  json += "\"dustUgM3\":" + String(sensors.dustUgM3, 0) + ",";
  json += "\"ldrRaw\":" + String(sensors.ldrRaw) + ",";
  json += "\"dark\":" + String(sensors.dark ? "true" : "false") + ",";
  json += "\"motion\":" + String(sensors.motion ? "true" : "false") + ",";
  json += "\"power\":" + String(light.powerOn ? "true" : "false") + ",";
  json += "\"mode\":\"" + modeName() + "\",";
  json += "\"brightness\":" + String(light.brightness) + ",";
  json += "\"r\":" + String(light.targetR) + ",";
  json += "\"g\":" + String(light.targetG) + ",";
  json += "\"b\":" + String(light.targetB) + ",";
  json += "\"weather\":\"" + weather.summary + "\",";
  json += "\"message\":\"" + weather.message + "\"";
  json += "}";
  return json;
}

void handleRoot() {
  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Smart LED Light</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:24px;background:#f6f7f9;color:#18202a}";
  html += "main{max-width:680px;margin:auto}section{background:white;border:1px solid #d9dde5;border-radius:8px;padding:16px;margin:12px 0}";
  html += "button,input{font-size:16px;margin:4px;padding:8px}code{background:#eef1f6;padding:2px 5px;border-radius:4px}</style></head><body><main>";
  html += "<h1>Smart LED Light</h1>";
  html += "<section><h2>Status</h2><pre id='status'>Loading...</pre></section>";
  html += "<section><h2>Control</h2>";
  html += "<button onclick=\"send('/api/power?value=1')\">Power On</button>";
  html += "<button onclick=\"send('/api/power?value=0')\">Power Off</button>";
  html += "<button onclick=\"send('/api/mode?value=AUTO')\">Auto</button>";
  html += "<button onclick=\"send('/api/mode?value=MANUAL')\">Manual</button><br>";
  html += "Brightness <input id='br' type='range' min='0' max='100' value='70' oninput=\"send('/api/brightness?value='+this.value)\"><br>";
  html += "Color <input id='color' type='color' value='#d2ffdc' onchange='setColor(this.value)'>";
  html += "</section>";
  html += "<script>";
  html += "async function refresh(){let r=await fetch('/api/status');document.getElementById('status').textContent=JSON.stringify(await r.json(),null,2)}";
  html += "async function send(u){await fetch(u);refresh()}";
  html += "function setColor(hex){let r=parseInt(hex.slice(1,3),16),g=parseInt(hex.slice(3,5),16),b=parseInt(hex.slice(5,7),16);send(`/api/color?r=${r}&g=${g}&b=${b}`)}";
  html += "setInterval(refresh,2000);refresh();</script></main></body></html>";
  server.send(200, "text/html", html);
}

void handleApiStatus() {
  server.send(200, "application/json", statusJson());
}

void handleApiPower() {
  light.powerOn = server.arg("value").toInt() != 0;
  applyMoodLight();
  server.send(200, "application/json", statusJson());
}

void handleApiMode() {
  String value = server.arg("value");
  value.toUpperCase();
  light.mode = value == "MANUAL" ? MODE_MANUAL : MODE_AUTO;
  updateMoodByMode();
  server.send(200, "application/json", statusJson());
}

void handleApiBrightness() {
  light.brightness = constrain(server.arg("value").toInt(), 0, 100);
  applyMoodLight();
  server.send(200, "application/json", statusJson());
}

void handleApiColor() {
  light.mode = MODE_MANUAL;
  setMoodColor(
    constrain(server.arg("r").toInt(), 0, 255),
    constrain(server.arg("g").toInt(), 0, 255),
    constrain(server.arg("b").toInt(), 0, 255));
  applyMoodLight();
  server.send(200, "application/json", statusJson());
}

void handleApiWeather() {
  weather.summary = server.arg("summary");
  weather.outdoorTemp = server.arg("temp").toFloat();
  weather.outdoorHumidity = server.arg("humidity").toInt();
  weather.rainMm = server.arg("rain").toFloat();
  weather.message = server.arg("msg");
  weather.hasData = true;
  if (light.mode == MODE_AUTO) {
    updateMoodByMode();
  }
  drawDisplay();
  server.send(200, "application/json", statusJson());
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", handleApiStatus);
  server.on("/api/power", handleApiPower);
  server.on("/api/mode", handleApiMode);
  server.on("/api/brightness", handleApiBrightness);
  server.on("/api/color", handleApiColor);
  server.on("/api/weather", handleApiWeather);
  server.begin();
}

void setupWifi() {
  if (String(WIFI_SSID).length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 10000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.localIP());
      return;
    }
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP started: ");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_DUST_LED, OUTPUT);
  pinMode(PIN_BUTTON_MODE, INPUT_PULLUP);
  pinMode(PIN_BUTTON_POWER, INPUT_PULLUP);
  setDustLed(false);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);
  analogSetPinAttenuation(PIN_DUST_ANALOG, ADC_11db);

  setupFootPwm();
  writeFootPwm(0);

  Wire.begin(I2C_SDA, I2C_SCL);
  oled.setI2CAddress(OLED_I2C_ADDR << 1);
  oled.begin();
  drawDisplayMessage("Smart LED ESP32", "Starting...");

  dht.begin();
  moodStrip.begin();
  moodStrip.show();
  selectAutoColor();

  setupWifi();
  setupWebServer();

  drawDisplayMessage("Smart LED ESP32", "Web " + ipText(), "Serial 115200");
  lastDisplayAt = millis();
  Serial.println("READY,SMART_LED_ESP32");
}

void loop() {
  server.handleClient();
  readSerialCommands();
  handleButtons();
  updateSensors();
  updateDisplay();
  updateSerialStatus();
}
