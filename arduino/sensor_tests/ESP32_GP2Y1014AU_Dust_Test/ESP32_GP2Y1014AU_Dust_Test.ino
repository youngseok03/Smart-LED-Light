#define DUST_ADC_PIN 35
#define DUST_LED_PIN 27
#define SERIAL_BAUD 115200
#define SAMPLE_INTERVAL_MS 1000

unsigned long lastSample = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);
  analogSetPinAttenuation(DUST_ADC_PIN, ADC_11db);
  //random(1, 30)
}

void loop() {
  if (millis() - lastSample < SAMPLE_INTERVAL_MS) {
    return;
  }

  lastSample = millis();

  digitalWrite(DUST_LED_PIN, LOW);
  delayMicroseconds(280);
  int raw = analogRead(DUST_ADC_PIN);
  delayMicroseconds(40);
  digitalWrite(DUST_LED_PIN, HIGH);
  delayMicroseconds(9680);

  Serial.println(raw);
}