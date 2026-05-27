#include <Adafruit_NeoPixel.h>

#define LED_PIN 5
#define LED_COUNT 30
#define LDR_PIN A0
#define BUTTON_PIN 8
#define LDR_DARK_THRESHOLD 400
#define BUTTON_DEBOUNCE_MS 45
#define BRIGHTNESS_STEP_COUNT 6

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

const uint8_t brightnessSteps[BRIGHTNESS_STEP_COUNT] = {0, 20, 40, 60, 80, 100};
uint8_t brightnessIndex = 3;
bool stripOff = true;
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastButtonChange = 0;
unsigned long lastDebug = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LDR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  strip.begin();
  strip.clear();
  strip.show();

  Serial.println("WS2812 30 LED test ready");
}

void loop() {
  if (wasButtonPressed()) {
    brightnessIndex = (brightnessIndex + 1) % BRIGHTNESS_STEP_COUNT;
    Serial.print("BRIGHTNESS=");
    Serial.println(brightnessSteps[brightnessIndex]);
  }

  int rawLight = analogRead(LDR_PIN);
  bool dark = rawLight < LDR_DARK_THRESHOLD;

  if (!dark || brightnessSteps[brightnessIndex] == 0) {
    turnStripOff();
    printDebug(rawLight, dark);
    return;
  }

  renderBrightness();
  printDebug(rawLight, dark);
}

bool wasButtonPressed() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonChange = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastButtonChange) > BUTTON_DEBOUNCE_MS && reading != stableButtonState) {
    stableButtonState = reading;
    return stableButtonState == LOW;
  }

  return false;
}

void turnStripOff() {
  if (!stripOff) {
    strip.clear();
    strip.show();
    stripOff = true;
  }
}

void renderBrightness() {
  stripOff = false;
  strip.setBrightness(map(brightnessSteps[brightnessIndex], 0, 100, 0, 180));
  fillAll(strip.Color(255, 220, 160));
  strip.show();
}

void fillAll(uint32_t color) {
  for (int pixel = 0; pixel < LED_COUNT; pixel++) {
    strip.setPixelColor(pixel, color);
  }
}

void printDebug(int rawLight, bool dark) {
  if (millis() - lastDebug < 1000) {
    return;
  }

  lastDebug = millis();
  Serial.print("LDR=");
  Serial.print(rawLight);
  Serial.print(",DARK=");
  Serial.print(dark ? 1 : 0);
  Serial.print(",BRIGHTNESS=");
  Serial.print(brightnessSteps[brightnessIndex]);
  Serial.print(",LED=");
  Serial.println((dark && brightnessSteps[brightnessIndex] > 0) ? 1 : 0);
}
