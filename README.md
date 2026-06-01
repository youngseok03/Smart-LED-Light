# Smart LED Light

ESP32 Dev Module에 WS2812 원형 LED, DHT11, GP2Y1014AU 먼지센서, DIS070012 1.3인치 I2C OLED를 연결해서 웹에서 제어하는 스마트 조명 데모입니다.

현재 주 사용 방식은 ESP32를 컴퓨터에 USB로 연결하고 `uv` Python bridge를 통해 React 웹에서 제어하는 구조입니다. ESP32 단독 Wi-Fi 웹 서버 스케치와 Arduino Uno용 스케치도 함께 남겨두었습니다.

## 1. 준비물

- ESP32 Dev Module
- WS2812 3핀 LED 링 24개
- DHT11 3핀 모듈
- GP2Y1014AU 먼지센서 모듈
- DIS070012 1.3인치 4핀 I2C OLED
- 택트 버튼 2개
- 3.3V / 5.5V 출력이 있는 브레드보드 전원 모듈
- 330 ohm 저항 1개, LED 데이터선 안정화용
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

## 3. ESP32 USB Serial 회로

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

Brightness button one side -> ESP32 GPIO26
Brightness button other    -> Breadboard GND rail

Mode button one side       -> ESP32 GPIO25
Mode button other          -> Breadboard GND rail
```

버튼은 코드에서 `INPUT_PULLUP`을 사용하므로 별도 저항이 필요 없습니다.

GP2Y1014AU 먼지센서는 동봉된 `150Ω` 저항과 `220uF` 콘덴서만 쓰는 배선 기준입니다.

```text
GP2Y Vcc       -> Breadboard 5.5V rail
GP2Y S-GND     -> Breadboard GND rail
GP2Y LED       -> ESP32 GPIO27
GP2Y V-LED     -> Breadboard 5.5V rail through 150 ohm resistor
GP2Y LED-GND   -> Breadboard GND rail
220uF capacitor + -> GP2Y V-LED
220uF capacitor - -> GP2Y LED-GND

GP2Y Vo/AO     -> ESP32 GPIO35
```

ESP32 ADC 입력은 3.3V를 넘기면 안 됩니다. `Vo/AO`를 바로 연결하는 방식에서 값이 포화되거나 불안정하면 `10kΩ/20kΩ` 전압분배를 다시 넣는 것이 안전합니다.

## 4. ESP32 USB Serial 업로드

Arduino IDE에서 엽니다.

```text
arduino/esp32_usb_serial/smart_led_usb_serial/smart_led_usb_serial.ino
```

설정:

```text
Board: ESP32 Dev Module
Port: 현재 ESP32 포트
Serial Monitor Baud: 9600
```

업로드 후 Serial Monitor를 `9600` baud로 열면 아래 메시지와 센서 디버그 값이 출력됩니다.

```text
READY,ESP32_USB_SERIAL
DEBUG,TEMP=...,HUM=...,LED_POWER=...,BRIGHT=...,MODE=...,DUST_RAW=...
```

Python bridge를 실행할 때는 Arduino IDE Serial Monitor를 닫아야 합니다. 두 프로그램이 같은 USB Serial 포트를 동시에 사용할 수 없습니다.

## 5. Wi-Fi 설정

Wi-Fi 단독 동작을 테스트하려면 아래 스케치를 사용합니다.

```text
arduino/esp32/sketch_may27a/sketch_may27a.ino
```

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

프로젝트의 React 웹 UI를 씁니다.

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

USB Serial bridge 방식에서는 Python bridge 주소를 URL 파라미터로 지정합니다.

```text
http://127.0.0.1:5174/?esp=http://127.0.0.1:8765
```

Wi-Fi ESP32 방식에서는 ESP32 IP를 URL 파라미터로 지정합니다.

```text
http://127.0.0.1:5174/?esp=http://ESP32_IP
```

예:

```text
http://127.0.0.1:5174/?esp=http://172.20.10.4
```

## 8. Python bridge

Python bridge는 보드를 USB Serial로 제어할 때 쓰는 구조입니다. Wi-Fi 없이 ESP32를 컴퓨터에 유선 연결해서 쓸 때도 이 bridge를 사용합니다. 실행은 반드시 `uv`를 사용합니다.

ESP32 USB Serial 전용 스케치:

```text
arduino/esp32_usb_serial/smart_led_usb_serial/smart_led_usb_serial.ino
```

현재 연결 기준 핀:

```text
WS2812 DIN             -> ESP32 GPIO18
DHT11 DATA/S           -> ESP32 GPIO4
OLED SDA               -> ESP32 GPIO21
OLED SCL               -> ESP32 GPIO22
Brightness button      -> ESP32 GPIO26 and GND
Mode button            -> ESP32 GPIO25 and GND
GP2Y Vo/AO             -> ESP32 GPIO35
GP2Y LED control       -> ESP32 GPIO27
OLED VCC               -> Breadboard 3.3V rail
DHT11 VCC/+            -> Breadboard 3.3V rail
WS2812 5V              -> Breadboard 5.5V rail
All GND                -> Breadboard GND rail + ESP32 GND
```

USB Serial 스케치에서 버튼은 `INPUT_PULLUP`으로 동작합니다. 버튼 한쪽 다리는 해당 GPIO에, 반대쪽 다리는 공통 GND에 연결하면 됩니다. `GPIO26` 버튼은 밝기를 `0, 20, 40, 60, 80, 100%` 순서로 바꾸고, 웹의 전원 버튼은 별도로 진짜 ON/OFF 명령을 보냅니다.

`MANUAL` 모드는 웹 색상 패드/컬러 휠에서 고른 RGB 색상을 ESP32로 전송해서 사용합니다. `AUTO`, `REST`, `STUDY`는 코드에 정해진 색을 사용합니다.

GP2Y1014AU의 `GPIO35` 연결은 측정값 입력 전용입니다. `GPIO35`는 ESP32에서 입력 전용 핀이므로 GP2Y의 LED 제어핀으로는 쓸 수 없습니다. GP2Y LED 제어핀은 `GPIO27`에 연결합니다.

현재 USB Serial 스케치는 동봉된 `150Ω` 저항과 `220uF` 콘덴서만 쓰는 배선 기준입니다. GP2Y `Vo/AO`는 `GPIO35`에 바로 연결하고, 코드에서도 분압 보정 없이 `GPIO35` 전압을 그대로 먼지센서 출력 전압으로 계산합니다.

```text
GP2Y VCC      -> 5V rail
GP2Y S-GND    -> GND rail
GP2Y V-LED    -> 150Ω resistor -> 5V rail
GP2Y LED-GND  -> GND rail
GP2Y LED      -> ESP32 GPIO27
GP2Y Vo/AO    -> ESP32 GPIO35
220uF +       -> GP2Y V-LED
220uF -       -> GP2Y LED-GND / GND rail
```

ESP32 ADC 입력은 3.3V를 넘기면 안 됩니다. `Vo/AO`를 바로 연결하는 방식에서 값이 포화되거나 불안정하면 `10kΩ/20kΩ` 전압분배를 다시 넣는 것이 안전합니다.

USB Serial 스케치는 Wi-Fi를 쓰지 않습니다. ESP32를 컴퓨터에 USB로 연결한 상태에서 Python bridge가 Serial 명령을 보내고, 웹은 Python bridge 주소로 요청을 보냅니다.

포트 확인:

```bash
uv run python/send_weather_to_arduino.py --list-ports
```

USB bridge 실행 예시:

```bash
uv run python/send_weather_to_arduino.py --port /dev/cu.usbserial-0001
```

웹 서버:

```bash
cd web
npm install
npm run dev -- --host 127.0.0.1 --port 5174
```

브라우저는 Python bridge를 보도록 엽니다.

```text
http://127.0.0.1:5174/?esp=http://127.0.0.1:8765
```

하드웨어 없이 메시지만 확인:

```bash
uv run python/send_weather_to_arduino.py --dry-run --once
```

ESP32 직접 Wi-Fi 방식에서는 이 bridge를 켜지 않아도 됩니다.

## 9. 동작 모드

```text
AUTO   -> 고정 자동 색상, 기본 RGB 200/255/220
MANUAL -> 웹에서 고른 RGB 색상
REST   -> 따뜻한 주황 계열 breathe 효과
STUDY  -> 연한 하늘색 breathe 효과
```

웹의 전원 버튼은 LED를 진짜 ON/OFF 합니다. ESP32의 GPIO26 버튼은 전원 버튼이 아니라 밝기 단계 `0, 20, 40, 60, 80, 100%`를 순환합니다. GPIO25 버튼은 `AUTO -> MANUAL -> REST -> STUDY` 순서로 모드를 바꿉니다.

## 10. 테스트 스케치

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

ESP32 USB Serial 통합 스케치:

```text
arduino/esp32_usb_serial/smart_led_usb_serial/smart_led_usb_serial.ino
```
