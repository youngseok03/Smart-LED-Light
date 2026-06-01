#define DUST_ADC_PIN A0
#define DUST_LED_PIN 7
#define SERIAL_BAUD 9600
#define SAMPLE_INTERVAL_MS 1000
#define CLEAN_AIR_VOLTAGE 0.6
#define UG_PER_M3_PER_VOLT 200.0

struct DustReading {
  int raw;
  float voltage;
  int density;
};

unsigned long lastSample = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);

  Serial.println("GP2Y1014AU dust sensor test for Arduino Uno");
  Serial.println("Vo/AO -> A0, LED control -> D7");
  Serial.println("Wiring: V-LED -> 150 ohm -> 5V, 220uF + -> V-LED, 220uF - -> GND");
  Serial.println("A0_raw,A0_voltage,voltage_over_clean,dust_ugm3");
}

void loop() {
  if (millis() - lastSample < SAMPLE_INTERVAL_MS) {
    return;
  }

  lastSample = millis();
  DustReading reading = readDustSensor();

  Serial.print("A0_raw=");
  Serial.print(reading.raw);
  Serial.print(",A0_voltage=");
  Serial.print(reading.voltage, 3);
  Serial.print(",voltage_over_clean=");
  Serial.print(reading.voltage - CLEAN_AIR_VOLTAGE, 3);
  Serial.print(",dust_ugm3=");
  Serial.println(reading.density);
}

DustReading readDustSensor() {
  digitalWrite(DUST_LED_PIN, LOW);
  delayMicroseconds(280);
  int raw = analogRead(DUST_ADC_PIN);
  delayMicroseconds(40);
  digitalWrite(DUST_LED_PIN, HIGH);
  delayMicroseconds(9680);

  float voltage = raw * (5.0 / 1023.0);
  float density = (voltage - CLEAN_AIR_VOLTAGE) * UG_PER_M3_PER_VOLT;
  if (density < 0) {
    density = 0;
  }
  if (density > 500) {
    density = 500;
  }

  DustReading reading;
  reading.raw = raw;
  reading.voltage = voltage;
  reading.density = (int)density;
  return reading;
}
