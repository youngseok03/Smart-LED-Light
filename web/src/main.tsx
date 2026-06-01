import React, { useEffect, useMemo, useState } from "react";
import { createRoot } from "react-dom/client";
import {
  Activity,
  CloudRain,
  Droplets,
  Gauge,
  Lightbulb,
  Moon,
  Palette,
  Power,
  Radio,
  Sun,
  Thermometer,
  Wifi,
  Wind,
  type LucideIcon,
} from "lucide-react";
import "./styles.css";

type LedMode = "auto" | "manual" | "rest" | "study";

type LedState = {
  power: boolean;
  mode: LedMode;
  rgb: [number, number, number];
  colorName: string;
  brightness: number;
};

type DashboardData = {
  device: {
    name: string;
    connection: string;
    lastSync: string;
    serial: string;
  };
  indoor: {
    temperature: number | null;
    humidity: number | null;
    airQuality: string;
    dust: string;
    dustDensity?: number;
    motion: boolean;
    illuminance: number;
  };
  weather: {
    status: string;
    sky: string;
    rainType: string;
    rainfall: number;
    wind: number;
    outsideTemperature: number;
  };
  led: LedState;
  footLight: {
    power: boolean;
    auto: boolean;
    trigger: string;
    timeoutSeconds: number;
  };
  lcd: {
    line1: string;
    line2: string;
    message: string;
  };
  ai?: {
    provider: string;
    message: string;
    reason?: string;
  };
};

type LiveSensorState = {
  ldrRaw?: number;
  dark?: boolean;
  pirMotion?: boolean;
  onboardLed?: boolean;
  dustRaw?: number;
  dustVoltage?: number;
  dustDensity?: number;
  updatedAt?: number;
};

type BridgeStateResponse = {
  ok: boolean;
  data?: DashboardData;
  sensor?: LiveSensorState | null;
};

type ColorPreset = {
  name: string;
  rgb: [number, number, number];
};

const colorPresets: ColorPreset[] = [
  { name: "연한 초록", rgb: [200, 255, 220] },
  { name: "따뜻한 흰색", rgb: [255, 240, 210] },
  { name: "청록색", rgb: [120, 240, 230] },
  { name: "연한 하늘색", rgb: [210, 240, 255] },
  { name: "연한 노란색", rgb: [255, 245, 180] },
];

const modes: LedMode[] = ["auto", "manual", "rest", "study"];

function resolveEsp32BaseUrl() {
  const params = new URLSearchParams(window.location.search);
  const queryUrl = params.get("esp");
  if (queryUrl) {
    window.localStorage.setItem("smartLedEsp32Url", queryUrl);
    return queryUrl.replace(/\/$/, "");
  }

  return (window.localStorage.getItem("smartLedEsp32Url") ?? "http://172.20.10.4").replace(/\/$/, "");
}

const esp32BaseUrl = resolveEsp32BaseUrl();
const bridgeUrl = `${esp32BaseUrl}/power`;
const brightnessUrl = `${esp32BaseUrl}/brightness`;
const modeUrl = `${esp32BaseUrl}/mode`;
const colorUrl = `${esp32BaseUrl}/color`;
const stateUrl = `${esp32BaseUrl}/state`;

function rgbValue(rgb: [number, number, number]) {
  return `rgb(${rgb.join(", ")})`;
}

function rgbaValue(rgb: [number, number, number], alpha: number) {
  return `rgba(${rgb.join(", ")}, ${alpha})`;
}

function hslToRgb(hue: number, saturation: number, lightness: number): [number, number, number] {
  const s = saturation / 100;
  const l = lightness / 100;
  const chroma = (1 - Math.abs(2 * l - 1)) * s;
  const h = hue / 60;
  const x = chroma * (1 - Math.abs((h % 2) - 1));
  const m = l - chroma / 2;

  let r = 0;
  let g = 0;
  let b = 0;

  if (h >= 0 && h < 1) [r, g, b] = [chroma, x, 0];
  else if (h >= 1 && h < 2) [r, g, b] = [x, chroma, 0];
  else if (h >= 2 && h < 3) [r, g, b] = [0, chroma, x];
  else if (h >= 3 && h < 4) [r, g, b] = [0, x, chroma];
  else if (h >= 4 && h < 5) [r, g, b] = [x, 0, chroma];
  else [r, g, b] = [chroma, 0, x];

  return [
    Math.round((r + m) * 255),
    Math.round((g + m) * 255),
    Math.round((b + m) * 255),
  ];
}

function rgbToHsl([red, green, blue]: [number, number, number]) {
  const r = red / 255;
  const g = green / 255;
  const b = blue / 255;
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const delta = max - min;
  let hue = 0;
  let saturation = 0;
  const lightness = (max + min) / 2;

  if (delta !== 0) {
    saturation = delta / (1 - Math.abs(2 * lightness - 1));

    if (max === r) hue = 60 * (((g - b) / delta) % 6);
    else if (max === g) hue = 60 * ((b - r) / delta + 2);
    else hue = 60 * ((r - g) / delta + 4);
  }

  return {
    hue: (hue + 360) % 360,
    saturation: saturation * 100,
    lightness: lightness * 100,
  };
}

function wheelPointFromRgb(rgb: [number, number, number]) {
  const { hue, saturation } = rgbToHsl(rgb);
  const radius = Math.min(46, Math.max(14, saturation * 0.46));
  const radians = (hue * Math.PI) / 180;

  return {
    x: 50 + Math.cos(radians) * radius,
    y: 50 + Math.sin(radians) * radius,
  };
}

export default function App() {
  const [dashboard, setDashboard] = useState<DashboardData | null>(null);
  const [loadError, setLoadError] = useState("");
  const [led, setLed] = useState<LedState>({
    power: true,
    mode: "auto",
    rgb: [200, 255, 220],
    colorName: "연한 초록",
    brightness: 70,
  });
  const [footLightPower, setFootLightPower] = useState(false);
  const [wheelPoint, setWheelPoint] = useState({ x: 78, y: 50 });
  const [bridgeStatus, setBridgeStatus] = useState(`ESP32 ${esp32BaseUrl}`);
  const [liveSensor, setLiveSensor] = useState<LiveSensorState | null>(null);

  useEffect(() => {
    async function loadDashboard() {
      try {
        const response = await fetch("/data.json", { cache: "no-store" });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = (await response.json()) as DashboardData;
        setDashboard(data);
        setLed(data.led);
        setWheelPoint(wheelPointFromRgb(data.led.rgb));
        setFootLightPower(data.footLight.power);
      } catch (error) {
        setLoadError("data.json을 불러오지 못했습니다.");
        console.error(error);
      }
    }

    loadDashboard();
  }, []);

  useEffect(() => {
    let cancelled = false;

    async function loadBridgeState() {
      try {
        const response = await fetch(stateUrl, { cache: "no-store" });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const payload = (await response.json()) as BridgeStateResponse;
        if (cancelled || !payload.ok || !payload.data) return;

        setDashboard(payload.data);
        setLiveSensor(payload.sensor ?? null);
        setBridgeStatus(payload.sensor ? "ESP32 live sensors" : "ESP32 online, waiting sensors");
      } catch {
        if (!cancelled) {
          setLiveSensor(null);
        }
      }
    }

    void loadBridgeState();
    const intervalId = window.setInterval(loadBridgeState, 1000);
    return () => {
      cancelled = true;
      window.clearInterval(intervalId);
    };
  }, []);

  const lampStyle = useMemo<React.CSSProperties>(
    () => ({
      background: rgbValue(led.rgb),
      boxShadow: led.power
        ? `0 0 ${28 + led.brightness * 0.55}px ${rgbaValue(led.rgb, 0.5)}`
        : "none",
      opacity: led.power ? 1 : 0.22,
    }),
    [led],
  );

  if (!dashboard && !loadError) {
    return <div className="loading">Smart LED Remote 준비 중...</div>;
  }

  const indoor = dashboard?.indoor;
  const weather = dashboard?.weather;
  const footLight = dashboard?.footLight;
  const lcd = dashboard?.lcd;
  const ai = dashboard?.ai;
  const device = dashboard?.device;
  const dustValue =
    indoor?.dustDensity === undefined ? "센서 대기" : `${indoor.dustDensity}ug / ${indoor.dust}`;
  const illuminanceValue =
    liveSensor?.ldrRaw === undefined ? `${indoor?.illuminance ?? "-"} lux` : `${liveSensor.ldrRaw} raw`;

  function selectWheelColor(event: React.PointerEvent<HTMLDivElement>, shouldSend = false) {
    const rect = event.currentTarget.getBoundingClientRect();
    const radius = rect.width / 2;
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    const dx = x - radius;
    const dy = y - radius;
    const rawDistance = Math.hypot(dx, dy);
    const distance = Math.min(radius, rawDistance);
    const pointScale = rawDistance > radius ? radius / rawDistance : 1;
    const hue = ((Math.atan2(dy, dx) * 180) / Math.PI + 360) % 360;
    const saturation = Math.max(18, (distance / radius) * 96);
    const lightness = 74 - (distance / radius) * 18;
    const selectedRgb = hslToRgb(hue, saturation, lightness);

    setWheelPoint({
      x: 50 + ((dx * pointScale) / radius) * 50,
      y: 50 + ((dy * pointScale) / radius) * 50,
    });
    setLed((current) => ({
      ...current,
      rgb: selectedRgb,
      colorName: "사용자 색상",
      mode: "manual",
    }));
    if (shouldSend) {
      void sendManualColor(selectedRgb, "사용자 색상");
    }
  }

  async function sendManualColor(rgb: [number, number, number], colorName: string) {
    setBridgeStatus("Sending...");

    try {
      const response = await fetch(colorUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ rgb }),
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      setBridgeStatus(`Manual color ${rgb.join(",")} sent`);
      setLed((current) => ({
        ...current,
        rgb,
        colorName,
        mode: "manual",
        power: current.brightness > 0,
      }));
    } catch (error) {
      console.error("ESP32 color request failed", error);
      setBridgeStatus("ESP32 offline");
    }
  }

  async function togglePower() {
    const nextPower = !led.power;
    setLed((current) => ({ ...current, power: nextPower }));
    setBridgeStatus("Sending...");

    try {
      const response = await fetch(bridgeUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ power: nextPower }),
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      setBridgeStatus(nextPower ? "ESP32 ON sent" : "ESP32 OFF sent");
    } catch (error) {
      console.error("ESP32 power request failed", error);
      setBridgeStatus("ESP32 offline");
    }
  }

  async function sendBrightness(brightness: number) {
    setBridgeStatus("Sending...");

    try {
      const response = await fetch(brightnessUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ brightness }),
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      setBridgeStatus(`LED brightness ${brightness}% sent`);
    } catch (error) {
      console.error("ESP32 brightness request failed", error);
      setBridgeStatus("ESP32 offline");
    }
  }

  function updateBrightness(brightness: number) {
    const nextBrightness = Math.max(0, Math.min(100, brightness));
    setLed((current) => ({ ...current, brightness: nextBrightness, power: nextBrightness > 0 }));
    void sendBrightness(nextBrightness);
  }

  async function updateMode(mode: LedMode) {
    setLed((current) => ({ ...current, mode }));
    setBridgeStatus("Sending...");

    try {
      const response = await fetch(modeUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mode }),
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      setBridgeStatus(`LED mode ${mode.toUpperCase()} sent`);
    } catch (error) {
      console.error("ESP32 mode request failed", error);
      setBridgeStatus("ESP32 offline");
    }
  }

  return (
    <main className="remoteShell">
      <div className="ambientPane" aria-hidden="true" />
      <section className="statusZone" aria-label="스마트 조명 상태">
        <div className="brandRow">
          <div>
            <p className="eyebrow">Adventure Design · Team 2</p>
            <h1>Smart Bedroom LED</h1>
          </div>
          <div className="connection">
            <Wifi size={16} />
            <span>{loadError || (device?.connection === "demo" ? "Demo data" : device?.connection)}</span>
          </div>
        </div>

        <div className="displayPanel">
          <div className="heroBadge">
            <Lightbulb size={18} />
            <span>{led.mode.toUpperCase()} MODE</span>
          </div>
          <div className="lcdScreen">
            <p className="screenLabel">LCD Preview</p>
            <p>{lcd?.line1 ?? "-"}</p>
            <p>{lcd?.line2 ?? "-"}</p>
          </div>

          <div className="lampPreview" aria-label="LED 색상 미리보기">
            <div className="lampHalo" style={lampStyle} />
            <div className="lampBase">
              <span />
            </div>
          </div>

          <div className="quickStats">
            <Stat icon={Thermometer} label="실내온도" value={`${indoor?.temperature ?? "--"}°C`} />
            <Stat icon={Droplets} label="실내습도" value={`${indoor?.humidity ?? "--"}%`} />
            <Stat icon={CloudRain} label="날씨" value={weather?.status ?? "--"} />
            <Stat
              icon={Activity}
              label="실내미세먼지"
              value={dustValue}
            />
          </div>
        </div>

        <div className="messageStrip">
          <span>{ai?.provider === "dummy" ? "AI 더미 피드백" : "생활 피드백"}</span>
          <strong>{loadError || ai?.message || lcd?.message || "데이터를 불러오는 중입니다."}</strong>
        </div>
      </section>

      <section className="controlZone" aria-label="리모콘 제어">
        <div className="remoteHead">
          <div>
            <p className="eyebrow">Remote Control</p>
            <h2>조명 제어</h2>
          </div>
          <button
            className={`powerButton ${led.power ? "" : "off"}`}
            type="button"
            onClick={togglePower}
            aria-label="전원 토글"
          >
            <Power size={25} />
            <span>{led.power ? "ON" : "OFF"}</span>
          </button>
        </div>
        <p className="bridgeStatus">{bridgeStatus}</p>

        <div className="modeRow" role="group" aria-label="조명 모드">
          {modes.map((mode) => (
            <button
              className={`modeButton ${led.mode === mode ? "active" : ""}`}
              key={mode}
              type="button"
              onClick={() => void updateMode(mode)}
            >
              {mode.toUpperCase()}
            </button>
          ))}
        </div>

        <div className="controlGrid">
          <div className="remoteSection">
            <SectionTitle icon={Palette} label="LED Color" value={led.colorName} />
            <div className="colorControl">
              <div className="colorPad" role="group" aria-label="색상 프리셋 선택">
                {colorPresets.map((preset) => (
                  <button
                    className={`colorButton ${led.colorName === preset.name ? "selected" : ""}`}
                    key={preset.name}
                    style={{ "--swatch": rgbValue(preset.rgb) } as React.CSSProperties}
                    type="button"
                    aria-label={preset.name}
                    onClick={() => {
                      setWheelPoint(wheelPointFromRgb(preset.rgb));
                      void sendManualColor(preset.rgb, preset.name);
                    }}
                  />
                ))}
              </div>
              <div
                className="colorWheel"
                role="slider"
                tabIndex={0}
                aria-label="사용자 색상 선택"
                aria-valuetext={led.colorName}
                style={
                  {
                    "--wheel-x": `${wheelPoint.x}%`,
                    "--wheel-y": `${wheelPoint.y}%`,
                    "--selected-color": rgbValue(led.rgb),
                  } as React.CSSProperties
                }
                onPointerDown={(event) => {
                  event.currentTarget.setPointerCapture(event.pointerId);
                  selectWheelColor(event);
                }}
                onPointerMove={(event) => {
                  if (event.buttons === 1) selectWheelColor(event);
                }}
                onPointerUp={(event) => selectWheelColor(event, true)}
              >
                <span />
              </div>
            </div>
          </div>

          <div className="remoteSection">
            <SectionTitle icon={Sun} label="Brightness" value={`${led.brightness}%`} />
            <input
              type="range"
              min="0"
              max="100"
              value={led.brightness}
              onChange={(event) => updateBrightness(Number(event.target.value))}
            />
            <div className="stepRow">
              <button
                type="button"
                onClick={() => updateBrightness(led.brightness - 10)}
              >
                -
              </button>
              <button
                type="button"
                onClick={() => updateBrightness(led.brightness + 10)}
              >
                +
              </button>
            </div>
          </div>

          <div className="remoteSection">
            <SectionTitle icon={Moon} label="Foot Light" value={footLightPower ? "ON" : footLight?.auto ? "AUTO" : "OFF"} />
            <button className="wideButton" type="button" onClick={() => setFootLightPower((value) => !value)}>
              <Moon size={18} />
              발밑등 테스트
            </button>
            <p className="hintText">{footLight?.trigger ?? "어두움 + 움직임 감지 시 점등"}</p>
          </div>
        </div>
      </section>

      <section className="dataZone" aria-label="센서와 API 데이터">
        <DataColumn
          title="Weather API"
          rows={[
            ["하늘", weather?.sky ?? "-", Sun],
            ["강수", weather?.rainType ?? "-", CloudRain],
            ["강수량", `${weather?.rainfall ?? "-"}mm`, Gauge],
            ["풍속", `${weather?.wind ?? "-"}m/s`, Wind],
          ]}
        />
        <DataColumn
          title="Indoor Sensors"
          rows={[
            ["조도", illuminanceValue, Lightbulb],
            ["동작 감지", indoor?.motion ? "감지됨" : "대기 중", Radio],
            ["공기질", indoor?.airQuality ?? "-", Activity],
            ["Serial", device?.serial ?? "-", Wifi],
          ]}
        />
      </section>
    </main>
  );
}

function Stat({ icon: Icon, label, value }: { icon: LucideIcon; label: string; value: string }) {
  return (
    <div className="stat">
      <Icon size={19} />
      <span>{label}</span>
      <strong>{value}</strong>
    </div>
  );
}

function SectionTitle({ icon: Icon, label, value }: { icon: LucideIcon; label: string; value: string }) {
  return (
    <div className="sectionTitle">
      <span>
        <Icon size={16} />
        {label}
      </span>
      <strong>{value}</strong>
    </div>
  );
}

type DataRow = [label: string, value: string, icon: LucideIcon];

function DataColumn({ title, rows }: { title: string; rows: DataRow[] }) {
  return (
    <div className="dataColumn">
      <p className="eyebrow">{title}</p>
      <dl>
        {rows.map(([label, value, Icon]) => (
          <div key={label}>
            <dt>
              <Icon size={16} />
              {label}
            </dt>
            <dd>{value}</dd>
          </div>
        ))}
      </dl>
    </div>
  );
}

createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
