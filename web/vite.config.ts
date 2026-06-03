import { readFileSync } from "node:fs";
import type { IncomingMessage } from "node:http";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig, type Plugin } from "vite";
import react from "@vitejs/plugin-react";

const KMA_URL = "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtFcst";
const CACHE_MS = 10 * 60 * 1000;

type ServerEnv = Record<string, string | undefined>;

type WeatherData = {
  status: string;
  sky: string;
  rainType: string;
  rainfall: number;
  wind: number;
  outsideTemperature: number;
};

type KmaItem = {
  category?: string;
  fcstTime?: string;
  fcstValue?: string;
};

const configDir = dirname(fileURLToPath(import.meta.url));
const projectRoot = resolve(configDir, "..");

function parseEnvFile(path: string) {
  const env: ServerEnv = {};

  try {
    const text = readFileSync(path, "utf8");
    for (const line of text.split(/\r?\n/)) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith("#")) continue;

      const normalized = trimmed.startsWith("export ") ? trimmed.slice(7).trim() : trimmed;
      const equalsIndex = normalized.indexOf("=");
      if (equalsIndex < 0) continue;

      const key = normalized.slice(0, equalsIndex).trim();
      let value = normalized.slice(equalsIndex + 1).trim();
      if (
        (value.startsWith('"') && value.endsWith('"')) ||
        (value.startsWith("'") && value.endsWith("'"))
      ) {
        value = value.slice(1, -1);
      }
      env[key] = value;
    }
  } catch {
    // Missing .env files are fine; the API route reports a clear error.
  }

  return env;
}

function loadServerEnv() {
  return {
    ...parseEnvFile(resolve(projectRoot, ".env")),
    ...parseEnvFile(resolve(configDir, ".env")),
    ...process.env,
  };
}

function getWeatherApiKey(env: ServerEnv) {
  return (
    env.WEATHER_API_KEY ||
    env.KMA_API_KEY ||
    env.KMA_SERVICE_KEY ||
    env.DATA_GO_KR_SERVICE_KEY
  );
}

function getBaseDateTime() {
  const base = new Date();
  if (base.getMinutes() >= 40) {
    base.setMinutes(30, 0, 0);
  } else {
    base.setHours(base.getHours() - 1, 30, 0, 0);
  }

  const yyyy = String(base.getFullYear());
  const mm = String(base.getMonth() + 1).padStart(2, "0");
  const dd = String(base.getDate()).padStart(2, "0");
  const hh = String(base.getHours()).padStart(2, "0");
  return {
    baseDate: `${yyyy}${mm}${dd}`,
    baseTime: `${hh}30`,
  };
}

function numberValue(value: unknown, fallback = 0) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function rainValue(value: unknown) {
  const text = String(value ?? "").trim();
  if (!text || text.includes("강수없음")) return 0;
  if (text.includes("미만")) return 0.5;

  const match = text.match(/\d+(\.\d+)?/);
  return match ? Number(match[0]) : 0;
}

function parseWeather(items: KmaItem[]): WeatherData {
  const skyMap: Record<string, string> = {
    "1": "맑음",
    "3": "구름많음",
    "4": "흐림",
  };
  const rainMap: Record<string, string> = {
    "0": "없음",
    "1": "비",
    "2": "비/눈",
    "3": "눈",
    "5": "빗방울",
    "6": "빗방울/눈날림",
    "7": "눈날림",
  };

  const byTime = new Map<string, Record<string, string>>();
  for (const item of items) {
    if (!item.fcstTime || !item.category) continue;
    const values = byTime.get(item.fcstTime) ?? {};
    values[item.category] = item.fcstValue ?? "";
    byTime.set(item.fcstTime, values);
  }

  const currentTime = `${String(new Date().getHours()).padStart(2, "0")}${String(new Date().getMinutes()).padStart(2, "0")}`;
  const sortedTimes = [...byTime.keys()].sort();
  const selectedTime = sortedTimes.find((time) => time >= currentTime) ?? sortedTimes[0];
  const selected = selectedTime ? byTime.get(selectedTime) ?? {} : {};

  const sky = skyMap[selected.SKY ?? ""] ?? "-";
  const rainType = rainMap[selected.PTY ?? "0"] ?? "-";
  const rainfall = rainValue(selected.RN1);
  const status = rainType !== "없음" && rainType !== "-" ? rainType : sky;

  return {
    status,
    sky,
    rainType,
    rainfall,
    wind: numberValue(selected.WSD),
    outsideTemperature: numberValue(selected.T1H),
  };
}

async function fetchWeather(): Promise<WeatherData> {
  const env = loadServerEnv();
  const serviceKey = getWeatherApiKey(env);
  if (!serviceKey) {
    throw new Error("WEATHER_API_KEY is missing in .env");
  }

  const nx = env.WEATHER_NX ?? "60";
  const ny = env.WEATHER_NY ?? "127";
  const { baseDate, baseTime } = getBaseDateTime();
  const query = new URLSearchParams({
    pageNo: "1",
    numOfRows: "100",
    dataType: "JSON",
    base_date: baseDate,
    base_time: baseTime,
    nx,
    ny,
  });
  const encodedKey = serviceKey.includes("%") ? serviceKey : encodeURIComponent(serviceKey);
  const response = await fetch(`${KMA_URL}?serviceKey=${encodedKey}&${query.toString()}`);
  if (!response.ok) {
    throw new Error(`KMA HTTP ${response.status}`);
  }

  const payload = await response.json();
  const header = payload?.response?.header;
  if (header?.resultCode !== "00") {
    throw new Error(header?.resultMsg || "KMA API error");
  }

  const items = payload?.response?.body?.items?.item;
  if (!Array.isArray(items)) {
    throw new Error("KMA API returned no forecast items");
  }

  return parseWeather(items as KmaItem[]);
}

function readRequestBody(request: IncomingMessage) {
  return new Promise<string>((resolveBody, rejectBody) => {
    let body = "";
    request.on("data", (chunk) => {
      body += chunk;
    });
    request.on("end", () => {
      resolveBody(body);
    });
    request.on("error", rejectBody);
  });
}

function normalizeWeather(value: unknown): WeatherData {
  const data = value as Partial<WeatherData> | null;
  if (!data || typeof data !== "object") {
    throw new Error("weather payload is required");
  }

  return {
    status: String(data.status ?? "-"),
    sky: String(data.sky ?? data.status ?? "-"),
    rainType: String(data.rainType ?? "-"),
    rainfall: numberValue(data.rainfall),
    wind: numberValue(data.wind),
    outsideTemperature: numberValue(data.outsideTemperature),
  };
}

function setJsonHeaders(response: { setHeader(name: string, value: string): void }) {
  response.setHeader("Content-Type", "application/json; charset=utf-8");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  response.setHeader("Access-Control-Allow-Headers", "Content-Type");
}

function weatherApiPlugin(): Plugin {
  let cache: { time: number; weather: WeatherData } | null = null;
  let testWeather: { time: number; weather: WeatherData } | null = null;

  return {
    name: "smart-led-weather-api",
    configureServer(server) {
      server.middlewares.use("/api/weather-test", async (request, response) => {
        setJsonHeaders(response);

        if (request.method === "OPTIONS") {
          response.statusCode = 204;
          response.end();
          return;
        }

        if (request.method === "DELETE") {
          testWeather = null;
          response.end(JSON.stringify({ ok: true, cleared: true }));
          return;
        }

        if (request.method !== "POST") {
          response.statusCode = 405;
          response.end(JSON.stringify({ ok: false, error: "POST or DELETE required" }));
          return;
        }

        try {
          const body = await readRequestBody(request);
          testWeather = {
            time: Date.now(),
            weather: normalizeWeather(JSON.parse(body || "{}")),
          };
          response.end(JSON.stringify({ ok: true, weather: testWeather.weather, source: "test" }));
        } catch (error) {
          const message = error instanceof Error ? error.message : "Invalid weather test payload";
          response.statusCode = 400;
          response.end(JSON.stringify({ ok: false, error: message }));
        }
      });

      server.middlewares.use("/api/weather", async (_request, response) => {
        setJsonHeaders(response);

        try {
          if (testWeather) {
            response.end(JSON.stringify({ ok: true, weather: testWeather.weather, cachedAt: testWeather.time, source: "test" }));
            return;
          }

          const now = Date.now();
          if (!cache || now - cache.time > CACHE_MS) {
            cache = {
              time: now,
              weather: await fetchWeather(),
            };
          }
          response.end(JSON.stringify({ ok: true, weather: cache.weather, cachedAt: cache.time }));
        } catch (error) {
          const message = error instanceof Error ? error.message : "Weather API failed";
          response.statusCode = 500;
          response.end(JSON.stringify({ ok: false, error: message }));
        }
      });
    },
  };
}

export default defineConfig({
  plugins: [react(), weatherApiPlugin()],
});
