#include <Adafruit_NeoPixel.h>

// ── 핀 설정 ──────────────────────────────────────
#define PIR_PIN       2
#define LIGHT_PIN     A0
#define LED_PIN       4
#define LED_COUNT     30

// ── 조도 임계값 ───────────────────────────────────
// 이 값보다 낮으면 어둡다고 판단
#define DARK_THRESHOLD 400

// ── 재점등 금지 시간 ──────────────────────────────
// LED가 꺼진 후 1분 동안 다시 켜지지 않음
const unsigned long RESTART_BLOCK_TIME = 60000UL;

// ── NeoPixel 초기화 ───────────────────────────────
Adafruit_NeoPixel strip(
  LED_COUNT,
  LED_PIN,
  NEO_GRB + NEO_KHZ800
);

// ── 상태 변수 ─────────────────────────────────────
bool ledActive = false;
bool restartBlocked = false;

unsigned long ledOffTime = 0;

void setup() {
  Serial.begin(9600);

  pinMode(PIR_PIN, INPUT);

  strip.begin();
  strip.clear();
  strip.show();

  // LED 전체 밝기
  strip.setBrightness(150);

  Serial.println("PIR 센서 초기화 중... 30초 대기");
  delay(30000);
  Serial.println("준비 완료!");
}

void loop() {
  unsigned long currentTime = millis();

  int lightValue = analogRead(LIGHT_PIN);
  int pirState = digitalRead(PIR_PIN);

  bool isDark = (lightValue < DARK_THRESHOLD);
  bool isMotion = (pirState == HIGH);

  // ── 재점등 금지 시간 확인 ───────────────────────
  if (restartBlocked &&
      currentTime - ledOffTime >= RESTART_BLOCK_TIME) {

    restartBlocked = false;
    Serial.println("재점등 금지 시간 종료");
  }

  // ── 센서 상태 출력 ──────────────────────────────
  Serial.print("조도: ");
  Serial.print(lightValue);

  Serial.print(" | PIR: ");
  Serial.print(isMotion ? "감지됨" : "없음");

  Serial.print(" | 재점등 제한: ");
  Serial.print(restartBlocked ? "적용 중" : "해제");

  Serial.print(" | 상태: ");

  // ── LED가 현재 켜져 있는 경우 ───────────────────
  if (ledActive) {

    // 밝아지거나 움직임이 없으면 즉시 소등
    if (!isDark || !isMotion) {
      ledOff();

      ledActive = false;
      restartBlocked = true;
      ledOffTime = currentTime;

      Serial.println("조건 불만족 → LED OFF / 1분 재점등 금지");
    }
    else {
      Serial.println("어두움 + 움직임 유지 → LED ON 유지");
    }
  }

  // ── LED가 현재 꺼져 있는 경우 ───────────────────
  else {

    // 재점등 제한 중이면 LED를 켜지 않음
    if (restartBlocked) {
      unsigned long remainingTime =
        RESTART_BLOCK_TIME - (currentTime - ledOffTime);

      Serial.print("재점등 금지 중 / 남은 시간: ");
      Serial.print(remainingTime / 1000);
      Serial.println("초");
    }

    // 재점등 제한이 아니고 어두우며 움직임이 감지된 경우
    else if (isDark && isMotion) {
      ledOn();
      ledActive = true;

      Serial.println("어두움 + 움직임 감지 → LED ON");
    }

    else {
      Serial.println("조건 불만족 → LED OFF 유지");
    }
  }

  delay(200);
}

// ── LED 켜기 ──────────────────────────────────────
void ledOn() {
  for (int i = 0; i < strip.numPixels(); i++) {
    // 주황색
    strip.setPixelColor(i, strip.Color(255, 80, 0));
  }

  strip.show();
}

// ── LED 끄기 ──────────────────────────────────────
void ledOff() {
  strip.clear();
  strip.show();
}
