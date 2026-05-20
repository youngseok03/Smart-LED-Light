# Smart LED Web Demo

스마트 침실 조명 프로젝트의 웹 리모콘 데모입니다.

현재 버전은 `public/data.json`의 예시 센서값을 불러와 화면에 표시하고, Python 브리지가 실행 중이면 Arduino Uno R3로 LCD/전원 명령을 전송합니다.

## 실행 방법

```bash
cd web
npm install
npm run dev
```

브라우저에서 아래 주소로 접속합니다. 포트가 이미 사용 중이면 Vite가 `5174`처럼 다음 포트로 실행합니다.

```text
http://localhost:5173
```

## Arduino Uno R3 브리지 실행

Arduino Uno R3를 USB로 연결하고 Arduino IDE Serial Monitor를 닫은 뒤, 프로젝트 루트에서 아래 명령을 실행합니다.

```bash
uv run python/send_weather_to_arduino.py --list-ports
uv run python/send_weather_to_arduino.py --port auto
```

하드웨어 없이 시리얼 메시지만 확인하려면 아래처럼 실행합니다.

```bash
uv run python/send_weather_to_arduino.py --dry-run --once
```

## 포함된 기능

- 날씨 API 예시값 표시
- 실내 온도, 습도, 조도, 동작 감지, 공기질 표시
- LED 전원, 색상, 밝기, 모드 변경
- 발밑등 테스트 버튼
- LCD 출력 문구 미리보기
- AI 더미 생활 피드백 메시지 표시
- 웹 전원 버튼으로 Arduino Uno/LCD ON/OFF 명령 전송
