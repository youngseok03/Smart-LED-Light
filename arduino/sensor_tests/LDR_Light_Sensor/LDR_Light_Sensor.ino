/*
  LDR light sensor standalone test for Arduino Uno R3.

  Wiring for an LDR module with analog output:
    LDR VCC / +  -> Arduino 5V
    LDR GND / -  -> Arduino GND
    LDR AO / A0  -> Arduino A0

  Serial Monitor:
    9600 baud

  Note:
    Some modules output a higher value in bright light.
    Others output a higher value in darkness.
    The important first test is that the raw value changes when light changes.
*/

const int LDR_PIN = A0;
const int SAMPLE_DELAY_MS = 500;

// Tune this after checking your room's raw readings.
const int DARK_THRESHOLD = 400;

void setup() {
  Serial.begin(9600);
  pinMode(LDR_PIN, INPUT);

  Serial.println("LDR light sensor test");
  Serial.println("Cover the sensor and shine light on it.");
  Serial.println("raw, percent, state");
}

void loop() {
  int raw = analogRead(LDR_PIN);
  int percent = map(raw, 0, 1023, 0, 100);
  const char *state = raw < DARK_THRESHOLD ? "DARK" : "BRIGHT";

  Serial.print("raw=");
  Serial.print(raw);
  Serial.print(" percent=");
  Serial.print(percent);
  Serial.print("% state=");
  Serial.println(state);

  delay(SAMPLE_DELAY_MS);
}
