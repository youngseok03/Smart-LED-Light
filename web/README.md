# Smart LED Web Demo

스마트 침실 조명 프로젝트의 웹 리모콘 데모입니다.

현재 버전은 실제 ESP32/Arduino와 통신하지 않고, `public/data.json`의 예시 센서값을 불러와 화면에 표시합니다. 버튼 조작은 화면 상태를 바꾸는 데모 동작입니다.

## 실행 방법

```bash
cd web
npm install
npm run dev
```

브라우저에서 아래 주소로 접속합니다.

```text
http://localhost:5173
```

## 포함된 기능

- 날씨 API 예시값 표시
- 실내 온도, 습도, 조도, 동작 감지, 공기질 표시
- LED 전원, 색상, 밝기, 모드 변경
- 발밑등 테스트 버튼
- LCD 출력 문구 미리보기
- 생활 피드백 메시지 표시
