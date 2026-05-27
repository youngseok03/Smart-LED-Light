# Smart LED Light

ESP32 Dev Module에 WS2812 원형 LED, DHT11, GP2Y1014AU 먼지센서, DIS070012 1.3인치 I2C OLED를 직접 연결해서 Wi-Fi 웹 제어로 동작하는 스마트 조명 데모입니다.

Arduino Uno용 스케치와 `uv` Python bridge는 백업/이전 구조로 남겨두었습니다.

## 1. 준비물

- ESP32 Dev Module
- WS2812 3핀 LED 링/스트립 30개
- DHT11 3핀 모듈
- GP2Y1014AU 먼지센서 모듈
- DIS070012 1.3인치 4핀 I2C OLED
- 택트 버튼 2개
- 3.3V / 5.5V 출력이 있는 브레드보드 전원 모듈
- 330 ohm 저항 1개
- 10k ohm 저항 1개, 20k ohm 저항 1개
- GP2Y1014AU용 150 ohm 저항, 220uF 콘덴서
- Arduino IDE
- Node.js / npm
- uv

## 2. Arduino IDE 라이브러리

Arduino IDE Library Manager에서 설치합니다.

```text
U8g2 by oliver
DHT sensor library
Adafruit Unified Sensor
Adafruit NeoPixel
```

ESP32 보드는 Arduino IDE에서 아래처럼 선택합니다.

```text
Board: ESP32 Dev Module
```

## 3. ESP32 회로

브레드보드 전원 모듈을 꽂은 상태 기준입니다. ESP32는 업로드와 Serial Monitor 확인을 위해 USB로 전원을 공급하고, 브레드보드 전원 모듈은 센서/LED 전원으로 사용합니다.

중요:

- ESP32 `GND`와 브레드보드 전원 모듈 `GND` 레일은 반드시 연결합니다.
- 브레드보드의 `3.3V` 레일과 `5.5V` 레일은 서로 연결하지 않습니다.
- `5.5V` 레일을 ESP32의 `3V3` 핀에 연결하면 안 됩니다.
- WS2812가 불안정하거나 뜨거워지면 5.5V가 너무 높을 수 있으니 전원 모듈을 5V 쪽으로 낮춰서 사용합니다.

전원 레일:

```text
Breadboard 3.3V rail -> OLED VCC, DHT11 VCC/+
Breadboard 5.5V rail -> WS2812 5V, GP2Y Vcc, GP2Y V-LED through 150 ohm
Breadboard GND rail  -> ESP32 GND, OLED GND, DHT11 GND, WS2812 GND, GP2Y GND
ESP32 power          -> USB cable
```

```text
OLED VCC       -> Breadboard 3.3V rail
OLED GND       -> Breadboard GND rail
OLED SDA       -> ESP32 GPIO21
OLED SCL       -> ESP32 GPIO22

DHT11 VCC/+    -> Breadboard 3.3V rail
DHT11 DATA/S   -> ESP32 GPIO4
DHT11 GND/-    -> Breadboard GND rail

WS2812 5V      -> Breadboard 5.5V rail
WS2812 GND     -> Breadboard GND rail
WS2812 DIN     -> 330 ohm resistor -> ESP32 GPIO18

Brightness button one side -> ESP32 GPIO32
Brightness button other    -> Breadboard GND rail

Mode button one side       -> ESP32 GPIO33
Mode button other          -> Breadboard GND rail
```

버튼은 코드에서 `INPUT_PULLUP`을 사용하므로 별도 저항이 필요 없습니다.

GP2Y1014AU 먼지센서는 ESP32 ADC 보호를 위해 전압분배를 거칩니다.

```text
GP2Y Vcc       -> Breadboard 5.5V rail
GP2Y S-GND     -> Breadboard GND rail
GP2Y LED       -> ESP32 GPIO26
GP2Y V-LED     -> Breadboard 5.5V rail through 150 ohm resistor
GP2Y LED-GND   -> Breadboard GND rail
220uF capacitor + -> GP2Y V-LED
220uF capacitor - -> GP2Y LED-GND

GP2Y Vo        -> 10k ohm resistor -> ESP32 GPIO34
ESP32 GPIO34   -> 20k ohm resistor -> Breadboard GND rail
```

먼지센서 `Vo`는 ESP32 ADC가 3.3V를 넘지 않도록 `10k/20k` 전압분배를 거칩니다. 브레드보드 전원 모듈을 5.5V로 쓰더라도 이 분압 비율은 그대로 사용합니다.

## 4. ESP32 업로드

Arduino IDE에서 엽니다.

```text
arduino/esp32/sketch_may27a/sketch_may27a.ino
```

설정:

```text
Board: ESP32 Dev Module
Port: 현재 ESP32 포트
Baud: 115200
```

업로드 후 Serial Monitor를 `115200` baud로 열면 ESP32가 만든 설정용 AP와 IP를 볼 수 있습니다.

## 5. Wi-Fi 설정

처음 부팅하면 ESP32가 설정용 Wi-Fi를 만듭니다.

```text
SSID: ESP32-Setup
주소: http://192.168.4.1/
```

휴대폰이나 노트북으로 `ESP32-Setup`에 접속한 뒤 `192.168.4.1`을 열고, 사용할 Wi-Fi를 선택해서 비밀번호를 입력합니다.

핫스팟을 쓸 때 ESP32-WROOM-32는 2.4GHz만 지원하므로 iPhone은 `호환성 최대화`를 켜는 것이 좋습니다.

연결 성공 시 Serial Monitor에 ESP32 IP가 출력됩니다.

```text
와이파이 연결 성공!
할당받은 IP 주소: 172.20.10.4
```

성공한 Wi-Fi 정보는 ESP32 내부 저장소에 저장됩니다. 그래서 다음부터는 USB를 빼고 일반 전원으로 켜도 같은 Wi-Fi에 자동 연결됩니다. 비밀번호를 잘못 저장했거나 다른 Wi-Fi로 바꾸려면 ESP32 내장 웹의 `Forget Wi-Fi` 링크를 눌러 다시 설정합니다.

## 6. ESP32 내장 웹 제어

ESP32 IP로 바로 접속하면 내장 제어 페이지가 열립니다.

```text
http://172.20.10.4/
```

가능한 동작:

- WS2812 ON/OFF
- 밝기 조절
- 모드 변경: `AUTO`, `MANUAL`, `REST`, `STUDY`
- DHT11 온도/습도 표시
- GP2Y1014AU 먼지센서 값 표시
- OLED에 IP, LED 상태, DHT11, 먼지센서 상태 순환 표시
- GPIO32 버튼: 밝기 `0, 20, 40, 60, 80, 100%` 순환
- GPIO33 버튼: 모드 `AUTO -> MANUAL -> REST -> STUDY` 순환

DHT11 값을 읽지 못하면 JSON에서는 아래처럼 내려갑니다.

```json
{
  "temperature": null,
  "humidity": null
}
```

상태 API:

```text
http://172.20.10.4/state
```

## 7. React 웹 리모컨

프로젝트의 React 웹 UI를 쓰려면 웹 서버만 켭니다. ESP32 방식에서는 Python bridge가 필요 없습니다.

처음 한 번:

```bash
cd web
npm install
```

실행:

```bash
npm run dev -- --host 127.0.0.1 --port 5174
```

브라우저:

```text
http://127.0.0.1:5174/
```

웹은 기본 ESP32 주소를 `http://172.20.10.4`로 사용합니다. ESP32 IP가 다르면 URL 파라미터로 지정합니다.

```text
http://127.0.0.1:5174/?esp=http://ESP32_IP
```

예:

```text
http://127.0.0.1:5174/?esp=http://172.20.10.4
```

## 8. Python bridge

Python bridge는 Arduino Uno를 USB Serial로 제어할 때 쓰는 이전 구조입니다. 실행은 반드시 `uv`를 사용합니다.

포트 확인:

```bash
uv run python/send_weather_to_arduino.py --list-ports
```

Arduino Uno bridge 실행:

```bash
uv run python/send_weather_to_arduino.py --port /dev/cu.usbmodem112301
```

하드웨어 없이 메시지만 확인:

```bash
uv run python/send_weather_to_arduino.py --dry-run --once
```

ESP32 직접 Wi-Fi 방식에서는 이 bridge를 켜지 않아도 됩니다.

## 9. 테스트 스케치

WS2812 30 LED만 확인:

```text
arduino/sensor_tests/WS2812_30_LED_Test/WS2812_30_LED_Test.ino
```

OLED만 확인:

```text
arduino/sensor_tests/OLED_Display_Test/OLED_Display_Test.ino
```

Arduino Uno 통합 스케치:

```text
arduino/smart_led_light/smart_led_light.ino
```
