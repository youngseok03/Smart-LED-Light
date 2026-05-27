# Smart LED Web Demo

이 폴더는 ESP32 Smart LED의 웹 리모컨입니다. ESP32가 Wi-Fi에 연결되어 있고 `/state` API를 열어둔 상태에서 사용합니다.

## 1. 설치

```bash
cd web
npm install
```

## 2. 웹 서버 실행

```bash
npm run dev -- --host 127.0.0.1 --port 5174
```

브라우저에서 아래 주소로 접속합니다.

```text
http://127.0.0.1:5174/
```

ESP32 IP가 기본값 `172.20.10.4`와 다르면 `esp` 파라미터로 지정합니다.

```text
http://127.0.0.1:5174/?esp=http://ESP32_IP
```

## 3. ESP32에서 표시되는 값

- DHT11 온도/습도
- GP2Y1014AU 먼지센서 값
- LED 전원 상태
- LED 밝기
- LED 모드

DHT11을 읽지 못하면 ESP32 `/state` 응답에서 `temperature`, `humidity`가 `null`로 내려오고, 웹에서는 `--`로 표시됩니다.

## 4. Python bridge

Python bridge는 Arduino Uno 또는 ESP32 USB Serial 스케치를 유선으로 제어할 때 사용합니다. ESP32 Wi-Fi 방식에서는 켜지 않아도 됩니다. USB Serial 구조를 테스트할 때는 반드시 `uv`로 실행합니다.

```bash
uv run python/send_weather_to_arduino.py --list-ports
uv run python/send_weather_to_arduino.py --port /dev/cu.usbmodem112301
uv run python/send_weather_to_arduino.py --dry-run --once
```

ESP32 USB Serial 스케치를 올린 경우 웹은 bridge 주소를 보도록 엽니다.

```text
http://127.0.0.1:5174/?esp=http://127.0.0.1:8765
```

## 5. 웹에서 가능한 제어

- 전원 버튼: ESP32에 연결된 WS2812 LED ON/OFF
- Brightness 슬라이더: WS2812 밝기 조절
- 모드 버튼: `AUTO`, `MANUAL`, `REST`, `STUDY` 전송
- 데이터 화면: ESP32 `/state`의 DHT11/먼지센서 상태 표시
