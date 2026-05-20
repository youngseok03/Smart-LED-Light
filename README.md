# Smart LED Light

Arduino Uno R3, DIS070012 I2C OLED, DHT11, LDR 조도센서, 외부 LED, 웹 리모컨을 연결한 스마트 침실 조명 데모입니다.

## 1. 준비물

- Arduino Uno R3
- DIS070012 4핀 I2C OLED
- DHT11 3핀 모듈
- LDR 조도센서 모듈
- LED 1개와 220 ohm 저항 1개
- 택트 버튼 2개
- 점퍼선, 브레드보드
- Arduino IDE
- Node.js / npm
- uv

## 2. Arduino IDE 라이브러리 설치

Arduino IDE의 Library Manager에서 아래 라이브러리를 설치합니다.

```text
U8g2 by oliver
DHT sensor library
Adafruit Unified Sensor
```

Uno R3 메모리 절약을 위해 OLED는 `U8x8` 텍스트 모드로 구동합니다.

## 3. 회로 연결

```text
OLED VCC      -> Arduino 5V
OLED GND      -> Arduino GND
OLED SDA      -> Arduino A4
OLED SCL      -> Arduino A5

DHT11 VCC/+   -> Arduino 5V
DHT11 DATA/S  -> Arduino D7
DHT11 GND/-   -> Arduino GND

LDR VCC/+     -> Arduino 5V
LDR GND/-     -> Arduino GND
LDR AO        -> Arduino A0

LED +         -> 220 ohm resistor -> Arduino D5
LED -         -> Arduino GND

Brightness button one side -> Arduino D8
Brightness button other    -> Arduino GND

Mode button one side       -> Arduino D9
Mode button other          -> Arduino GND
```

버튼은 코드에서 `INPUT_PULLUP`을 사용하므로 별도 저항이 필요 없습니다. 버튼은 브레드보드 가운데 홈을 가로질러 꽂고, 한쪽을 핀에, 반대쪽을 GND에 연결합니다.

## 4. Arduino 업로드

Arduino IDE에서 아래 파일을 엽니다.

```text
arduino/smart_led_light/smart_led_light.ino
```

설정:

```text
Board: Arduino Uno
Port: 현재 Arduino Uno 포트
```

업로드가 끝나면 Serial Monitor와 Serial Plotter를 닫습니다. Python bridge가 같은 USB serial 포트를 사용하기 때문입니다.

## 5. 웹 앱 설치와 실행

처음 한 번만 설치합니다.

```bash
cd web
npm install
```

웹 서버를 실행합니다.

```bash
npm run dev
```

브라우저에서 Vite가 출력한 주소로 접속합니다. 보통 아래 둘 중 하나입니다.

```text
http://localhost:5173/
http://localhost:5174/
```

## 6. Python bridge 실행

다른 터미널에서 프로젝트 루트로 이동합니다.

```bash
cd /Users/a1234/workspace/school/Smart-LED-Light/Smart-LED-Light
```

포트를 확인합니다.

```bash
uv run python/send_weather_to_arduino.py --list-ports
```

Arduino Uno 포트로 bridge를 실행합니다.

```bash
uv run python/send_weather_to_arduino.py --port /dev/cu.usbmodem112301
```

포트가 다르면 위 명령의 포트만 바꿉니다. Serial Monitor가 열려 있으면 `Resource busy`가 나므로 닫아야 합니다.

## 7. 동작

- 웹 전원 버튼: D5 LED ON/OFF
- 웹 밝기 슬라이더: D5 LED PWM 밝기 조절
- 웹 모드 버튼: Arduino OLED에 `AUTO`, `MANUAL`, `REST`, `STUDY` 모드 전송
- D8 버튼: D5 LED 밝기 `0, 20, 40, 60, 80, 100%` 순환
- D9 버튼: 모드 `AUTO -> MANUAL -> REST -> STUDY` 순환
- OLED: 부팅 시 `한글` 비트맵을 한 번 표시하고, 이후 실내 센서 / 조도 / 날씨 AI 메시지를 순환 표시

## 8. 빠른 점검

하드웨어 없이 bridge 메시지만 확인하려면:

```bash
uv run python/send_weather_to_arduino.py --dry-run --once
```

bridge 상태 확인:

```bash
curl http://127.0.0.1:8765/health
```

밝기 명령 확인:

```bash
curl -X POST http://127.0.0.1:8765/brightness \
  -H 'Content-Type: application/json' \
  -d '{"brightness":40}'
```

모드 명령 확인:

```bash
curl -X POST http://127.0.0.1:8765/mode \
  -H 'Content-Type: application/json' \
  -d '{"mode":"REST"}'
```
