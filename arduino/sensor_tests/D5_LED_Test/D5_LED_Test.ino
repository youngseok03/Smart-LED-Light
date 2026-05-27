#define LED_TEST_PIN 5

void setup() {
  pinMode(LED_TEST_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_TEST_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_TEST_PIN, LOW);
  delay(1000);
}
