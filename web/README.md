# Smart LED Web Demo

이 폴더는 스마트 침실 조명의 웹 리모컨입니다. 처음 실행하는 경우 아래 순서대로 진행하세요.

## 1. 설치

```bash
cd web
npm install
```

## 2. 웹 서버 실행

```bash
npm run dev
```

브라우저에서 Vite가 출력한 주소로 접속합니다.

```text
http://localhost:5173/
http://localhost:5174/
```

## 3. Arduino / Python bridge

Arduino 업로드와 회로 연결은 프로젝트 루트의 `README.md`를 참고하세요.

웹 버튼이 실제 Arduino로 동작하려면 프로젝트 루트에서 Python bridge를 실행해야 합니다.

```bash
uv run python/send_weather_to_arduino.py --list-ports
uv run python/send_weather_to_arduino.py --port /dev/cu.usbmodem112301
```

Serial Monitor가 열려 있으면 bridge가 포트를 사용할 수 없으므로 닫아야 합니다.

## 4. 웹에서 가능한 제어

- 전원 버튼: Arduino D5 LED ON/OFF
- Brightness 슬라이더: Arduino D5 LED PWM 밝기 조절
- 모드 버튼: `AUTO`, `MANUAL`, `REST`, `STUDY`를 Arduino로 전송
- 데이터 화면: `public/data.json`의 더미 센서/날씨/AI 메시지 표시
