# ESP32 Smart LED Light Wiring Guide

이 문서는 `어드벤처디자인 과제개발계획서_MAIN.docx`의 스마트 무드등 요구사항을 ESP32 DevKit V1 기준으로 정리한 회로 연결표입니다.

## 1. 주요 기능

- 침대 위 무드등: 날씨/실내 환경/웹 명령에 따라 NeoPixel RGB LED 색상과 밝기 제어
- 침대 아래 잔등: LDR 조도 센서와 PIR 모션 센서가 동시에 조건을 만족할 때만 LED 스트립 자동 점등
- 환경 표시: DHT11 온습도, GP2Y1014AU 먼지 센서, 외부 날씨 메시지를 1.3인치 I2C OLED에 순환 출력
- 사용자 입력: 버튼 1은 수동 색상 변경/자동 모드 전환, 버튼 2는 전원/밝기 조절
- 통신: ESP32 내장 Wi-Fi 웹 제어 + USB Serial 명령 수신

## 2. ESP32 핀맵

| 기능 | 부품/신호 | ESP32 핀 | 전압/주의 |
|---|---|---:|---|
| I2C SDA | YwRobot 1.3 OLED SDA | GPIO21 | DIS070012, SH1106 128x64 I2C |
| I2C SCL | YwRobot 1.3 OLED SCL | GPIO22 | 기본 주소 0x3C, 안 뜨면 0x3D 확인 |
| 온습도 | DHT11 DATA | GPIO4 | 3.3V 구동, DATA에 10k pull-up 권장 |
| 무드등 | NeoPixel DIN | GPIO18 | 330 ohm 직렬 저항 권장 |
| 발밑등 | MOSFET Gate | GPIO19 | 100 ohm 직렬 + 10k pulldown |
| 모션 | PIR OUT | GPIO27 | 대부분 3.3V HIGH 출력, 모듈별 확인 |
| 조도 | LDR divider OUT | GPIO34 | ADC1 입력 전용, 0-3.3V만 입력 |
| 먼지 아날로그 | GP2Y1014AU Vo divider OUT | GPIO35 | ADC1 입력 전용, 5V 초과 금지 |
| 먼지 LED 구동 | NPN/MOSFET Gate/Base | GPIO23 | 코드 기본값은 active-high 구동 |
| 버튼 1 | MODE/COLOR | GPIO25 | 버튼 반대쪽은 GND, 내부 pull-up 사용 |
| 버튼 2 | POWER/BRIGHTNESS | GPIO26 | 버튼 반대쪽은 GND, 내부 pull-up 사용 |

## 3. 전원 연결

| 전원 | 연결 대상 |
|---|---|
| ESP32 3V3 | DHT11 VCC, LDR 분압 상단, 버튼 회로 기준 전압 |
| ESP32 GND | 모든 센서/LED 외부전원 GND와 공통 접지 |
| 외부 5V | NeoPixel VCC, GP2Y1014AU Vcc/V-LED, 5V LED 스트립 사용 시 스트립 + |
| 외부 12V | 12V LED 스트립 사용 시 스트립 + 전용. ESP32 VIN에 넣지 말 것 |

전류가 큰 LED 스트립과 NeoPixel은 ESP32 3V3 핀에서 전원을 공급하지 마세요. 외부 전원을 쓰고 GND만 ESP32와 반드시 공통으로 묶습니다.

## 4. 부품별 배선

### DHT11

- VCC -> ESP32 3V3
- GND -> ESP32 GND
- DATA -> GPIO4
- DATA와 3V3 사이에 10k pull-up 저항

### 1.3인치 I2C OLED (YwRobot DIS070012)

- SDA -> GPIO21
- SCL -> GPIO22
- VCC -> ESP32 3V3 권장
- GND -> ESP32 GND

코드는 `U8g2` 라이브러리의 `U8G2_SH1106_128X64_NONAME_F_HW_I2C` 생성자를 사용합니다. 화면이 켜지지 않으면 `OLED_I2C_ADDR` 값을 `0x3D`로 바꿔 테스트하세요.

### NeoPixel RGB LED

- 5V -> 외부 5V +
- GND -> 외부 5V - 및 ESP32 GND 공통
- DIN -> GPIO18, 가능하면 330 ohm 직렬 저항 삽입
- 5V와 GND 사이에 1000 uF 이상 커패시터 권장

5V NeoPixel에서 신호 인식이 불안정하면 74AHCT125 같은 3.3V-to-5V 레벨 시프터를 DIN 앞에 넣습니다.

### 침대 아래 LED 스트립

- 스트립 + -> 외부 전원 +
- 스트립 - -> N채널 로직레벨 MOSFET Drain
- MOSFET Source -> 외부 전원 GND 및 ESP32 GND
- MOSFET Gate -> GPIO19, 100 ohm 직렬 저항
- Gate와 GND 사이 -> 10k pulldown

IRLZ44N, AO3400, IRLZ34N처럼 3.3V 게이트로 충분히 켜지는 로직레벨 MOSFET을 사용하세요.

### PIR 모션 센서

- VCC -> 모듈 권장 전압에 맞춰 5V 또는 3.3V
- GND -> ESP32 GND
- OUT -> GPIO27

HC-SR501 계열은 보통 OUT이 3.3V 수준이라 ESP32에 직접 연결할 수 있지만, 사용하는 모듈이 5V OUT이면 분압 또는 레벨 시프터가 필요합니다.

### LDR 조도 센서

- 3V3 -> LDR 한쪽
- LDR 다른쪽 -> GPIO34 및 10k 저항 한쪽
- 10k 저항 다른쪽 -> GND

이 배선에서는 밝을수록 ADC 값이 커지고 어두울수록 작아집니다. 코드의 `DARK_THRESHOLD`는 실제 방에서 시리얼 출력의 `LDR` 값을 보고 조정하세요.

### GP2Y1014AU 먼지 센서

권장 구동은 Sharp GP2Y1010/1014 계열의 펄스 LED 조건에 맞춘 방식입니다.

- Pin 1 V-LED -> 150 ohm 저항을 거쳐 외부 5V
- Pin 2 LED-GND -> GND
- Pin 3 LED -> GPIO23으로 직접 연결하지 말고, 아래 구동 회로 사용
- Pin 4 S-GND -> ESP32 GND
- Pin 5 Vo -> 전압 분압 후 GPIO35
- Pin 6 Vcc -> 외부 5V
- V-LED와 GND 사이 -> 220 uF 커패시터

실제 커넥터 표기가 모듈마다 다를 수 있으니 제품 데이터시트의 Pin 1-6 배열을 먼저 확인하세요.

GPIO23 구동 회로:

- GPIO23 -> 1k 저항 -> NPN Base 또는 N-MOSFET Gate
- NPN Emitter 또는 MOSFET Source -> GND
- 센서 Pin 3 LED -> NPN Collector 또는 MOSFET Drain
- 코드의 `DUST_LED_ACTIVE_HIGH = true` 유지

Vo 전압 보호:

- GP2Y Vo -> 10k 저항 -> GPIO35
- GPIO35 -> 20k 저항 -> GND
- 코드의 `DUST_DIVIDER_SCALE = 1.5`

이 분압은 5V 가능성을 3.3V 이하로 낮추기 위한 보호용입니다.

## 5. 버튼 동작

| 버튼 | 짧게 누름 | 길게 누름 |
|---|---|---|
| GPIO25 MODE/COLOR | 수동 모드로 전환하고 색상 팔레트 순환 | 자동/수동 모드 전환 |
| GPIO26 POWER/BRIGHTNESS | 무드등 ON/OFF | 밝기 25/50/75/100% 순환 |

## 6. PC/Python에서 보낼 수 있는 Serial 명령

시리얼 속도는 `115200`입니다. 각 명령은 줄바꿈 `\n`으로 끝냅니다.

```text
RGB,120,240,230
MODE,AUTO
MODE,MANUAL
POWER,1
POWER,0
BRIGHT,80
WEATHER,Clear,24,45,0,Have a good day
MSG,Take an umbrella
STATUS
```

ESP32는 5초마다 다음 형식으로 센서 상태를 출력합니다.

```text
SENSOR,tempC,humidity,dustUgM3,ldrRaw,dark,motion,mode,power,brightness,r,g,b
```

## 7. 웹 제어

`smart_led_light.ino`에서 `WIFI_SSID`와 `WIFI_PASSWORD`를 비워두면 ESP32가 `SmartLight-ESP32` AP를 만듭니다.

- AP 비밀번호: `12345678`
- 기본 접속 주소: `http://192.168.4.1`

공유기에 붙이고 싶으면 코드 상단의 `WIFI_SSID`, `WIFI_PASSWORD`를 입력한 뒤 업로드하세요.

지원 API:

```text
/api/status
/api/power?value=1
/api/power?value=0
/api/mode?value=AUTO
/api/mode?value=MANUAL
/api/brightness?value=80
/api/color?r=120&g=240&b=230
/api/weather?summary=Rain&temp=24&humidity=55&rain=1.2&msg=Take%20umbrella
```

## 8. Arduino IDE 설정

- Board: ESP32 Dev Module
- Upload Speed: 921600 또는 업로드 실패 시 115200
- Serial Monitor: 115200 baud
- 설치 라이브러리:
  - DHT sensor library
  - Adafruit Unified Sensor
  - Adafruit NeoPixel
  - U8g2

ESP32 Arduino Core 3.x에서는 LEDC API가 바뀌었기 때문에 코드에 2.x/3.x 호환 처리를 넣어두었습니다.
