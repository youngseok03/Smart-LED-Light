#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define DHT_PIN 7
#define DHT_TYPE DHT11
#define LED_PIN LED_BUILTIN
#define SERIAL_BAUD 9600
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
DHT dht(DHT_PIN, DHT_TYPE);

String apiWeather = "No API";
String apiRain = "-";
String apiWind = "-";
String aiMessage = "AI msg standby";
String lcdLines[LCD_ROWS] = {
  "Smart LED ready",
  "Uno R3 + I2C LCD",
  "Waiting bridge",
  "Use web power"
};

unsigned long lastDisplay = 0;
int displayPage = 0;
bool lcdPower = true;
bool ledPower = true;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin();
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  showBootScreen();

  delay(1500);
}

void loop() {
  readSerialData();

  if (!lcdPower) {
    return;
  }

  if (millis() - lastDisplay >= 2000) {
    lastDisplay = millis();
    displayPage = (displayPage + 1) % 3;
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
  lcdLines[0] = "Weather: " + apiWeather;
  lcdLines[1] = "Rain: " + apiRain + "mm";
  lcdLines[2] = "Wind: " + apiWind + "m/s";
  lcdLines[3] = aiMessage;
  displayApiData();
}

void handlePowerCommand(String command) {
  command.trim();

  if (command == "ON") {
    lcdPower = true;
    ledPower = true;
    digitalWrite(LED_PIN, HIGH);
    lcd.display();
    lcd.backlight();
    lcd.clear();
    printLine(0, "Power ON");
    printLine(1, "Web control OK");
    lastDisplay = 0;
  } else if (command == "OFF") {
    lcdPower = false;
    ledPower = false;
    digitalWrite(LED_PIN, LOW);
    lcd.clear();
    lcd.noBacklight();
    lcd.noDisplay();
  }
}

void handleDisplayData(String payload) {
  int start = 0;
  for (int row = 0; row < LCD_ROWS; row++) {
    int separator = payload.indexOf('|', start);
    if (separator == -1) {
      lcdLines[row] = payload.substring(start);
      start = payload.length();
    } else {
      lcdLines[row] = payload.substring(start, separator);
      start = separator + 1;
    }
    lcdLines[row].trim();
  }

  apiWeather = lcdLines[1];
  aiMessage = lcdLines[LCD_ROWS - 1];
  displayApiData();
}

void showBootScreen() {
  lcd.clear();
  for (int row = 0; row < LCD_ROWS; row++) {
    printLine(row, lcdLines[row]);
  }
}

void displayCurrentPage() {
  if (displayPage == 0) {
    displayDhtData();
  } else {
    displayApiData();
  }
}

void displayDhtData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  lcd.clear();

  if (isnan(humidity) || isnan(temperature)) {
    printLine(0, "DHT11 Error");
    printLine(1, "Check D7");
    if (LCD_ROWS > 2) {
      printLine(2, "Arduino Uno R3");
      printLine(3, ledPower ? "Power: ON" : "Power: OFF");
    }
    return;
  }

  printLine(0, "Indoor sensor");
  printLine(1, "Temp:" + String((int)temperature) + (char)223 + "C H:" + String((int)humidity) + "%");

  if (LCD_ROWS > 2) {
    printLine(2, ledPower ? "Mood LED: ON" : "Mood LED: OFF");
    printLine(3, "Web + Serial OK");
  }
}

void displayApiData() {
  lcd.clear();

  for (int row = 0; row < LCD_ROWS; row++) {
    printLine(row, lcdLines[row]);
  }
}

String fitToLcd(String value) {
  value.trim();
  if (value.length() > LCD_COLS) {
    return value.substring(0, LCD_COLS);
  }
  while (value.length() < LCD_COLS) {
    value += " ";
  }
  return value;
}

void printLine(int row, String value) {
  if (row < 0 || row >= LCD_ROWS) {
    return;
  }
  lcd.setCursor(0, row);
  lcd.print(fitToLcd(value));
}
