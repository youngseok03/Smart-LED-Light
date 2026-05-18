// ── LED 색상 팔레트 정의 ──
const COLOR_MAP = {
  red:    { hex: '#ef4444', glow: 'rgba(239,68,68,0.6)' },
  orange: { hex: '#f97316', glow: 'rgba(249,115,22,0.6)' },
  yellow: { hex: '#facc15', glow: 'rgba(250,204,21,0.6)' },
  green:  { hex: '#4ade80', glow: 'rgba(74,222,128,0.6)' },
  blue:   { hex: '#38bdf8', glow: 'rgba(56,189,248,0.6)' },
  purple: { hex: '#a78bfa', glow: 'rgba(167,139,250,0.6)' },
  white:  { hex: '#f1f5f9', glow: 'rgba(241,245,249,0.6)' },
};

// 현재 상태를 저장하는 객체
let state = {};

// ── data.json 불러오기 ──
async function loadData() {
  // fetch로 같은 폴더의 data.json을 읽어옴
  const res = await fetch('./data.json');
  state = await res.json();
  render(); // 화면에 표시
}

// ── 화면 전체 업데이트 함수 ──
function render() {
  // 환경 정보 표시
  document.getElementById('temperature').textContent = state.temperature;
  document.getElementById('humidity').textContent    = state.humidity;
  document.getElementById('weather').textContent     = state.weather;
  document.getElementById('dust').textContent        = state.dust;
  document.getElementById('message').textContent     = '💬 ' + state.message;

  // 전원 상태 표시
  const badge = document.getElementById('power-badge');
  badge.textContent = state.power ? 'ON' : 'OFF';
  badge.className   = 'power-badge ' + (state.power ? 'on' : 'off');

  // LED 미리보기 원 업데이트
  updateLedPreview();

  // 밝기 슬라이더 업데이트
  document.getElementById('brightness-slider').value = state.brightness;
  document.getElementById('brightness-val').textContent = state.brightness + '%';

  // 색상 버튼 활성화 표시
  document.querySelectorAll('.btn-color').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.color === state.ledColor);
  });

  // 모드 버튼 활성화 표시
  document.querySelectorAll('.btn-mode').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.mode === state.mode);
  });
}

// ── LED 미리보기 원 색상/밝기/전원 반영 ──
function updateLedPreview() {
  const preview = document.getElementById('led-preview');
  const color   = COLOR_MAP[state.ledColor] || COLOR_MAP['white'];

  if (state.power) {
    // 밝기를 0~1 사이 값으로 변환해서 투명도에 적용
    const alpha = state.brightness / 100;
    preview.style.background  = color.hex;
    preview.style.opacity     = 0.4 + alpha * 0.6; // 최소 40% 이상 보이게
    preview.style.boxShadow   = `0 0 ${state.brightness / 2}px ${color.glow}, 0 0 ${state.brightness}px ${color.glow}`;
  } else {
    // 전원 꺼짐: 회색으로
    preview.style.background = '#1e2d45';
    preview.style.opacity    = 1;
    preview.style.boxShadow  = 'none';
  }
}

// ── 전원 버튼 클릭 ──
function togglePower() {
  state.power = !state.power; // true ↔ false 전환
  render();
}

// ── 색상 버튼 클릭 ──
function setColor(color) {
  state.ledColor = color;
  render();
}

// ── 밝기 슬라이더 변경 ──
function setBrightness(val) {
  state.brightness = parseInt(val);
  document.getElementById('brightness-val').textContent = val + '%';
  updateLedPreview(); // 미리보기만 빠르게 업데이트
}

// ── 모드 버튼 클릭 ──
function setMode(mode) {
  state.mode = mode;
  render();
}

// ── 페이지 로드 시 실행 ──
loadData();
