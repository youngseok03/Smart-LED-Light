#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <U8x8lib.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define RING_PIN 18
#define RING_COUNT 24
#define LED_BAR_PIN 19
#define LED_BAR_COUNT 30
#define DUST_ADC_PIN 35
#define DUST_LED_PIN 23
#define BUTTON_BRIGHTNESS_PIN 26
#define BUTTON_MODE_PIN 25

#define SENSOR_INTERVAL_MS 2000
#define DUST_INTERVAL_MS 1000
#define OLED_INTERVAL_MS 1500
#define WIFI_CONNECT_TIMEOUT_MS 12000
#define BUTTON_DEBOUNCE_MS 45
#define BRIGHTNESS_STEP_COUNT 6
#define DUST_LED_ACTIVE_HIGH true
#define DEFAULT_BRIGHTNESS 40

const char *apSSID = "ESP32-Setup";

WebServer server(80);
U8X8_SH1106_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_NeoPixel ring(RING_COUNT, RING_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ledBar(LED_BAR_COUNT, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);
Preferences preferences;

String scanResultsHTML = "";
String activeMode = "AUTO";
const String modes[] = {"AUTO", "MANUAL", "REST", "STUDY"};
const uint8_t brightnessSteps[BRIGHTNESS_STEP_COUNT] = {0, 20, 40, 60, 80, 100};
bool ledPower = true;
int ledBrightness = DEFAULT_BRIGHTNESS;
int modeIndex = 0;
int brightnessIndex = 2;
int manualRed = 255;
int manualGreen = 245;
int manualBlue = 220;
float temperature = NAN;
float humidity = NAN;
int dustRaw = 0;
float dustPinVoltage = 0;
float dustSensorVoltage = 0;
int dustUg = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDustRead = 0;
unsigned long lastOledUpdate = 0;
int oledPage = 0;
bool lastBrightnessButtonReading = HIGH;
bool stableBrightnessButtonState = HIGH;
unsigned long lastBrightnessButtonChange = 0;
bool lastModeButtonReading = HIGH;
bool stableModeButtonState = HIGH;
unsigned long lastModeButtonChange = 0;

void clearSavedWiFi() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true, true);
  delay(300);
}

bool loadSavedWiFi(String &ssid, String &password) {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  return ssid.length() > 0;
}

void saveWiFi(String ssid, String password) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
}

bool connectToWiFi(String ssid, String password, unsigned long timeoutMs) {
  Serial.print("공유기에 연결 중: ");
  Serial.println(ssid);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("와이파이 연결 성공!");
    Serial.print("할당받은 IP 주소: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("와이파이 연결 실패. 설정 포털을 엽니다.");
  return false;
}

String htmlHeader(String title) {
  String html = "<!doctype html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>" + title + "</title>";
  html += "<style>";
  html += "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:24px;background:#10151f;color:#eef3ff}";
  html += "a,button,input,select{font-size:18px}button,input,select{box-sizing:border-box;width:100%;padding:12px;margin:8px 0;border-radius:8px;border:0}";
  html += "button,input[type=submit]{background:#2f8df5;color:white;font-weight:700}.card{background:#1b2433;padding:16px;border-radius:10px;margin:12px 0}";
  html += ".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}.muted{color:#9aa8bd}";
  html += "</style></head><body>";
  return html;
}

String htmlFooter() {
  return "</body></html>";
}

void handleRoot() {
  if (WiFi.status() != WL_CONNECTED) {
    handleSetupPage();
    return;
  }

  String html = htmlHeader("ESP32 Smart LED");
  html += "<h2>ESP32 Smart LED</h2>";
  html += "<p class='muted'>IP: " + WiFi.localIP().toString() + "</p>";
  html += "<div class='card'>";
  html += "<h3>Control</h3>";
  html += "<div class='row'>";
  html += "<form action='/power' method='POST'><input type='hidden' name='power' value='on'><button>ON</button></form>";
  html += "<form action='/power' method='POST'><input type='hidden' name='power' value='off'><button>OFF</button></form>";
  html += "</div>";
  html += "<form action='/brightness' method='POST'>";
  html += "<label>Brightness: " + String(ledBrightness) + "%</label>";
  html += "<input type='range' name='brightness' min='0' max='100' value='" + String(ledBrightness) + "' oninput='this.nextElementSibling.value=this.value'>";
  html += "<output>" + String(ledBrightness) + "</output>";
  html += "<input type='submit' value='Set brightness'></form>";
  html += "<form action='/mode' method='POST'><select name='mode'>";
  addModeOption(html, "AUTO");
  addModeOption(html, "MANUAL");
  addModeOption(html, "REST");
  addModeOption(html, "STUDY");
  html += "</select><input type='submit' value='Set mode'></form>";
  html += "</div>";

  html += "<div class='card'><h3>Sensors</h3>";
  html += "<p>DHT11: " + sensorText() + "</p>";
  html += "<p>Dust: " + String(dustUg) + " ug/m3</p>";
  html += "<p>Dust raw: " + String(dustRaw) + " / Vo: " + String(dustSensorVoltage, 2) + "V</p>";
  html += "</div>";
  html += "<p><a style='color:#8fc3ff' href='/setup'>Wi-Fi setup</a> · <a style='color:#8fc3ff' href='/forget'>Forget Wi-Fi</a> · <a style='color:#8fc3ff' href='/state'>JSON state</a></p>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void addModeOption(String &html, String mode) {
  html += "<option value='" + mode + "'";
  if (activeMode == mode) {
    html += " selected";
  }
  html += ">" + mode + "</option>";
}

void handleSetupPage() {
  String html = htmlHeader("ESP32 Wi-Fi Setup");
  html += "<h2>ESP32 와이파이 설정 포털</h2>";
  html += "<form action='/connect' method='POST'>";
  html += "<label>주변 와이파이 선택:</label>";
  html += "<select name='ssid'>" + scanResultsHTML + "</select>";
  html += "<label>비밀번호 입력:</label>";
  html += "<input type='password' name='password' placeholder='Password'>";
  html += "<input type='submit' value='연결하기'>";
  html += "</form>";
  html += "<p><a style='color:#8fc3ff' href='/rescan'>다시 스캔</a></p>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleConnect() {
  String selectedSSID = server.arg("ssid");
  String enteredPass = server.arg("password");

  server.send(200, "text/html", htmlHeader("Connecting") + "<h3>" + selectedSSID + " 연결 시도 중...</h3><p>잠시 후 ESP32 IP로 접속하세요.</p>" + htmlFooter());
  delay(500);

  if (connectToWiFi(selectedSSID, enteredPass, WIFI_CONNECT_TIMEOUT_MS)) {
    saveWiFi(selectedSSID, enteredPass);
  } else {
    startSetupPortal();
  }
}

void handleForget() {
  clearSavedWiFi();
  startSetupPortal();
  String html = htmlHeader("Wi-Fi cleared");
  html += "<h3>저장된 Wi-Fi를 삭제했습니다.</h3>";
  html += "<p><a style='color:#8fc3ff' href='/setup'>다시 설정하기</a></p>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleRescan() {
  scanNetworks();
  server.sendHeader("Location", "/setup");
  server.send(303);
}

void handlePower() {
  String power = requestValue("power");
  power.toLowerCase();
  ledPower = power != "off" && power != "false" && power != "0";
  if (ledPower && ledBrightness == 0) {
    ledBrightness = DEFAULT_BRIGHTNESS;
    syncBrightnessIndex();
  }
  applyLedOutput();
  sendOkJson();
}

void handleBrightness() {
  ledBrightness = constrain(requestValue("brightness").toInt(), 0, 100);
  ledPower = ledBrightness > 0;
  syncBrightnessIndex();
  applyLedOutput();
  sendOkJson();
}

void handleMode() {
  String mode = requestValue("mode");
  mode.toUpperCase();
  if (mode == "AUTO" || mode == "MANUAL" || mode == "REST" || mode == "STUDY") {
    activeMode = mode;
    syncModeIndex();
    applyLedOutput();
  }
  sendOkJson();
}

void handleColor() {
  int red = manualRed;
  int green = manualGreen;
  int blue = manualBlue;

  if (!parseRgbRequest(red, green, blue)) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"expected rgb array or r,g,b values\"}");
    return;
  }

  manualRed = constrain(red, 0, 255);
  manualGreen = constrain(green, 0, 255);
  manualBlue = constrain(blue, 0, 255);
  activeMode = "MANUAL";
  syncModeIndex();
  ledPower = ledBrightness > 0;
  applyLedOutput();

  String json = "{\"ok\":true,\"mode\":\"MANUAL\",\"rgb\":[";
  json += String(manualRed) + "," + String(manualGreen) + "," + String(manualBlue) + "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200, "application/json", json);
}

void handleState() {
  String temperatureJson = jsonNumber(temperature);
  String humidityJson = jsonNumber(humidity);
  int red;
  int green;
  int blue;
  currentLedRgb(red, green, blue);
  String json = "{";
  json += "\"ok\":true,";
  json += "\"data\":{";
  json += "\"device\":{\"name\":\"ESP32 Smart LED\",\"connection\":\"wifi\",\"lastSync\":\"live\",\"serial\":\"" + currentIpText() + "\"},";
  json += "\"indoor\":{";
  json += "\"temperature\":" + temperatureJson + ",";
  json += "\"humidity\":" + humidityJson + ",";
  json += "\"airQuality\":\"" + dustStatusText() + "\",";
  json += "\"dust\":\"" + dustStatusText() + "\",";
  json += "\"dustDensity\":" + String(dustUg) + ",";
  json += "\"motion\":false,";
  json += "\"illuminance\":0";
  json += "},";
  json += "\"weather\":{\"status\":\"WiFi\",\"sky\":\"-\",\"rainType\":\"-\",\"rainfall\":0,\"wind\":0,\"outsideTemperature\":0},";
  json += "\"led\":{\"power\":" + String(ledPower ? "true" : "false") + ",\"mode\":\"" + activeModeLower() + "\",\"rgb\":[" + String(red) + "," + String(green) + "," + String(blue) + "],\"colorName\":\"" + activeMode + "\",\"brightness\":" + String(ledBrightness) + "},";
  json += "\"footLight\":{\"power\":" + String(ledPower ? "true" : "false") + ",\"auto\":false,\"trigger\":\"D19 mirrors main LED\",\"timeoutSeconds\":0},";
  json += "\"lcd\":{\"line1\":\"" + String(ledPower ? "LED ON " : "LED OFF ") + String(ledBrightness) + "%\",\"line2\":\"T:" + dhtValueText(temperature) + " H:" + dhtValueText(humidity) + "\",\"message\":\"ESP32 live\"},";
  json += "\"ai\":{\"provider\":\"esp32\",\"message\":\"ESP32 live\",\"reason\":\"sensor state from ESP32\"}";
  json += "},";
  json += "\"sensor\":{";
  json += "\"dustRaw\":" + String(dustRaw) + ",";
  json += "\"dustVoltage\":" + String(dustSensorVoltage, 2) + ",";
  json += "\"dustDensity\":" + String(dustUg) + ",";
  json += "\"updatedAt\":" + String(millis());
  json += "}";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200, "application/json", json);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void sendOkJson() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200, "application/json", "{\"ok\":true}");
}

String requestValue(String key) {
  if (server.hasArg(key)) {
    return server.arg(key);
  }

  String body = server.arg("plain");
  int keyIndex = body.indexOf("\"" + key + "\"");
  if (keyIndex < 0) {
    keyIndex = body.indexOf(key);
  }
  if (keyIndex < 0) {
    return "";
  }

  int colon = body.indexOf(':', keyIndex);
  if (colon < 0) {
    return "";
  }

  int start = colon + 1;
  while (start < body.length() && (body[start] == ' ' || body[start] == '"' || body[start] == '\'')) {
    start++;
  }

  int end = start;
  while (end < body.length() && body[end] != ',' && body[end] != '}' && body[end] != '"' && body[end] != '\'') {
    end++;
  }

  String value = body.substring(start, end);
  value.trim();
  return value;
}

bool parseRgbRequest(int &red, int &green, int &blue) {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    red = server.arg("r").toInt();
    green = server.arg("g").toInt();
    blue = server.arg("b").toInt();
    return true;
  }

  String body = server.arg("plain");
  int arrayStart = body.indexOf('[');
  int arrayEnd = body.indexOf(']', arrayStart + 1);
  if (arrayStart < 0 || arrayEnd < 0) {
    return false;
  }

  String values = body.substring(arrayStart + 1, arrayEnd);
  int firstComma = values.indexOf(',');
  int secondComma = values.indexOf(',', firstComma + 1);
  if (firstComma < 0 || secondComma < 0) {
    return false;
  }

  red = values.substring(0, firstComma).toInt();
  green = values.substring(firstComma + 1, secondComma).toInt();
  blue = values.substring(secondComma + 1).toInt();
  return true;
}

String jsonNumber(float value) {
  if (isnan(value)) {
    return "null";
  }
  return String(value, 1);
}

String sensorText() {
  if (isnan(temperature) || isnan(humidity)) {
    return "no data";
  }
  return String(temperature, 1) + "C / " + String(humidity, 1) + "%";
}

String currentIpText() {
  return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

String activeModeLower() {
  String value = activeMode;
  value.toLowerCase();
  return value;
}

String dhtValueText(float value) {
  if (isnan(value)) {
    return "null";
  }
  return String(value, 1);
}

String dustStatusText() {
  if (dustUg > 150) {
    return "Bad";
  }
  if (dustUg > 80) {
    return "Normal";
  }
  return "Good";
}

void scanNetworks() {
  Serial.println("주변 와이파이 스캔 중...");
  scanResultsHTML = "";
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(100);

  int n = WiFi.scanNetworks();
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      scanResultsHTML += "<option value='" + ssid + "'>" + ssid + " (" + WiFi.RSSI(i) + "dBm)</option>";
    }
  }
}

void setupAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID);
  Serial.print("핫스팟 개설 완료: ");
  Serial.println(apSSID);
  Serial.print("설정 페이지 IP: ");
  Serial.println(WiFi.softAPIP());
}

void startSetupPortal() {
  WiFi.mode(WIFI_AP_STA);
  scanNetworks();
  setupAP();
}

void setupHardware() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.setPowerSave(0);
  oled.clearDisplay();
  oled.drawString(0, 0, "ESP32 Smart LED");
  oled.drawString(0, 2, "Booting...");

  dht.begin();
  pinMode(BUTTON_BRIGHTNESS_PIN, INPUT_PULLUP);
  pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);
  pinMode(DUST_LED_PIN, OUTPUT);
  setDustLed(false);
  analogSetPinAttenuation(DUST_ADC_PIN, ADC_11db);

  ring.begin();
  ledBar.begin();
  ring.clear();
  ledBar.clear();
  ring.show();
  ledBar.show();
  applyLedOutput();
}

void setupRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/rescan", HTTP_GET, handleRescan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/forget", HTTP_GET, handleForget);
  server.on("/power", HTTP_POST, handlePower);
  server.on("/power", HTTP_OPTIONS, handleOptions);
  server.on("/brightness", HTTP_POST, handleBrightness);
  server.on("/brightness", HTTP_OPTIONS, handleOptions);
  server.on("/mode", HTTP_POST, handleMode);
  server.on("/mode", HTTP_OPTIONS, handleOptions);
  server.on("/color", HTTP_POST, handleColor);
  server.on("/color", HTTP_OPTIONS, handleOptions);
  server.on("/state", HTTP_GET, handleState);
  server.on("/state", HTTP_OPTIONS, handleOptions);
  server.begin();
  Serial.println("웹 서버가 시작되었습니다.");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  setupHardware();
  WiFi.mode(WIFI_OFF);
  delay(300);

  String savedSSID;
  String savedPassword;
  if (loadSavedWiFi(savedSSID, savedPassword)) {
    if (!connectToWiFi(savedSSID, savedPassword, WIFI_CONNECT_TIMEOUT_MS)) {
      startSetupPortal();
    }
  } else {
    startSetupPortal();
  }
  setupRoutes();
}

void loop() {
  server.handleClient();
  readButtons();
  readSensors();
  updateOled();
}

void readButtons() {
  if (wasButtonPressed(BUTTON_BRIGHTNESS_PIN, lastBrightnessButtonReading, stableBrightnessButtonState, lastBrightnessButtonChange)) {
    brightnessIndex = (brightnessIndex + 1) % BRIGHTNESS_STEP_COUNT;
    ledBrightness = brightnessSteps[brightnessIndex];
    ledPower = ledBrightness > 0;
    applyLedOutput();
  }

  if (wasButtonPressed(BUTTON_MODE_PIN, lastModeButtonReading, stableModeButtonState, lastModeButtonChange)) {
    modeIndex = (modeIndex + 1) % 4;
    activeMode = modes[modeIndex];
    applyLedOutput();
  }
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

void syncBrightnessIndex() {
  int nearestIndex = 0;
  int nearestDistance = 101;
  for (int index = 0; index < BRIGHTNESS_STEP_COUNT; index++) {
    int distance = abs(ledBrightness - brightnessSteps[index]);
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearestIndex = index;
    }
  }
  brightnessIndex = nearestIndex;
}

void syncModeIndex() {
  for (int index = 0; index < 4; index++) {
    if (activeMode == modes[index]) {
      modeIndex = index;
      return;
    }
  }
}

void readSensors() {
  if (millis() - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = millis();
    float newHumidity = dht.readHumidity();
    float newTemperature = dht.readTemperature();
    if (!isnan(newHumidity) && !isnan(newTemperature)) {
      humidity = newHumidity;
      temperature = newTemperature;
    }
  }

  if (millis() - lastDustRead >= DUST_INTERVAL_MS) {
    lastDustRead = millis();
    readDustSensor();
  }
}

void readDustSensor() {
  setDustLed(true);
  delayMicroseconds(280);
  dustRaw = analogRead(DUST_ADC_PIN);
  uint32_t pinMilliVolts = analogReadMilliVolts(DUST_ADC_PIN);
  delayMicroseconds(40);
  setDustLed(false);
  delayMicroseconds(9680);

  dustPinVoltage = pinMilliVolts / 1000.0;
  dustSensorVoltage = dustPinVoltage * 1.5; // 10k/20k divider: ESP32 ADC sees 2/3 of Vo.
  float density = (dustSensorVoltage - 0.9) * 200.0;
  if (density < 0) {
    density = 0;
  }
  if (density > 500) {
    density = 500;
  }
  dustUg = (int)density;
}

void setDustLed(bool on) {
  digitalWrite(DUST_LED_PIN, on == DUST_LED_ACTIVE_HIGH ? HIGH : LOW);
}

void updateOled() {
  if (millis() - lastOledUpdate < OLED_INTERVAL_MS) {
    return;
  }

  lastOledUpdate = millis();
  oledPage = (oledPage + 1) % 3;
  oled.clearDisplay();

  if (oledPage == 0) {
    drawOledPage("ESP32 LED", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Setup:192.168.4.1", "LED " + String(ledPower ? "ON" : "OFF") + " " + String(ledBrightness) + "%", "Mode " + activeMode);
  } else if (oledPage == 1) {
    drawOledPage("DHT11", sensorText(), "Dust " + String(dustUg) + "ug", "Vo " + String(dustSensorVoltage, 2) + "V");
  } else {
    drawOledPage("Wi-Fi", WiFi.status() == WL_CONNECTED ? "Connected" : "Setup AP", "AP ESP32-Setup", WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "192.168.4.1");
  }
}

void drawOledPage(String title, String line1, String line2, String line3) {
  oled.setPowerSave(0);
  oled.drawString(0, 0, fitOled(title).c_str());
  oled.drawString(0, 1, "----------------");
  oled.drawString(0, 3, fitOled(line1).c_str());
  oled.drawString(0, 4, fitOled(line2).c_str());
  oled.drawString(0, 5, fitOled(line3).c_str());
}

String fitOled(String text) {
  text.replace("\r", " ");
  text.replace("\n", " ");
  if (text.length() > 16) {
    return text.substring(0, 16);
  }
  return text;
}

void applyLedOutput() {
  uint32_t color = ledPower ? currentLedColor() : ring.Color(0, 0, 0);
  uint8_t pixelBrightness = ledPower ? map(ledBrightness, 0, 100, 0, 180) : 0;

  ring.setBrightness(pixelBrightness);
  ledBar.setBrightness(pixelBrightness);

  for (int index = 0; index < RING_COUNT; index++) {
    ring.setPixelColor(index, color);
  }
  for (int index = 0; index < LED_BAR_COUNT; index++) {
    ledBar.setPixelColor(index, color);
  }

  ring.show();
  ledBar.show();
}

uint32_t currentLedColor() {
  int red;
  int green;
  int blue;
  currentLedRgb(red, green, blue);
  return ring.Color(red, green, blue);
}

void currentLedRgb(int &red, int &green, int &blue) {
  if (activeMode == "MANUAL") {
    red = manualRed;
    green = manualGreen;
    blue = manualBlue;
    return;
  }
  if (activeMode == "REST") {
    red = 255;
    green = 120;
    blue = 70;
    return;
  }
  if (activeMode == "STUDY") {
    red = 180;
    green = 220;
    blue = 255;
    return;
  }
  red = 200;
  green = 255;
  blue = 220;
}
