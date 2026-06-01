# /// script
# dependencies = [
#   "pyserial>=3.5",
# ]
# ///

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading
import time

import serial
from serial.tools import list_ports


BAUD_RATE = 9600
BRIDGE_HOST = "127.0.0.1"
BRIDGE_PORT = 8765
PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATA_FILE = PROJECT_ROOT / "web" / "public" / "data.json"
OLED_WIDTH_CHARS = 21


def sanitize_display_text(value, width=OLED_WIDTH_CHARS):
    text = str(value).replace("|", " ").replace("\n", " ").strip()
    return text[:width]


def parse_bool(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        lowered = value.strip().lower()
        if lowered in {"1", "true", "on", "yes"}:
            return True
        if lowered in {"0", "false", "off", "no"}:
            return False
    raise ValueError("expected boolean power value")


def parse_percent(value):
    number = int(value)
    return max(0, min(100, number))


def parse_mode(value):
    mode = str(value).strip().upper()
    allowed_modes = {"AUTO", "MANUAL", "REST", "STUDY"}
    if mode not in allowed_modes:
        raise ValueError("expected mode to be AUTO, MANUAL, REST, or STUDY")
    return mode


def parse_rgb(value):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError("expected rgb to be a three-number list")
    return [max(0, min(255, int(channel))) for channel in value]


def load_dashboard_data(data_file):
    with Path(data_file).open("r", encoding="utf-8") as file:
        return json.load(file)


def fallback_ai_message(data):
    weather = data.get("weather", {})
    indoor = data.get("indoor", {})
    status = str(weather.get("status", "")).lower()
    dust = str(indoor.get("dust", "")).lower()
    outside_temperature = float(weather.get("outsideTemperature", 0) or 0)

    if "rain" in status or weather.get("rainfall", 0):
        return "Take umbrella"
    if dust in {"bad", "poor", "very bad"}:
        return "Wear a mask"
    if outside_temperature >= 30:
        return "Drink water"
    if outside_temperature <= 0:
        return "Wear warm clothes"
    return "Have a good day"


def get_ai_message(data):
    ai_message = data.get("ai", {}).get("message")
    lcd_message = data.get("lcd", {}).get("message")
    return ai_message or lcd_message or fallback_ai_message(data)


def display_value(value, fallback="-"):
    return fallback if value is None else value


def build_lcd_lines(data):
    indoor = data.get("indoor", {})
    weather = data.get("weather", {})
    led = data.get("led", {})
    ai_message = get_ai_message(data)

    power_text = "ON" if led.get("power", True) else "OFF"
    brightness = led.get("brightness", 0)
    line1 = f"LED {power_text} {brightness}%"
    line2 = f"W:{weather.get('status', '-')} {weather.get('outsideTemperature', '-')}C"
    line3 = f"In:{display_value(indoor.get('temperature'))}C H{display_value(indoor.get('humidity'))}%"
    line4 = ai_message

    return [sanitize_display_text(line) for line in (line1, line2, line3, line4)]


def build_display_message(data):
    return "DATA," + "|".join(build_lcd_lines(data)) + "\n"


def parse_debug_value(value, boolean=False):
    if value == "null":
        return None

    if boolean and value in {"0", "1"}:
        return value == "1"

    try:
        if "." in value:
            return float(value)
        return int(value)
    except ValueError:
        return value


def parse_sensor_debug(line):
    if not line.startswith("DEBUG,"):
        return None

    raw_values = {}
    for part in line.split(",")[1:]:
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        raw_values[key] = parse_debug_value(value, key in {"DARK", "PIR", "ONBOARD_LED", "LED_POWER"})

    sensor = {"updatedAt": time.time()}
    field_map = {
        "LDR": "ldrRaw",
        "DARK": "dark",
        "PIR": "pirMotion",
        "ONBOARD_LED": "onboardLed",
        "TEMP": "temperature",
        "HUM": "humidity",
        "LED_POWER": "ledPower",
        "BRIGHT": "brightness",
        "MODE": "mode",
        "RGB": "rgb",
        "DUST_RAW": "dustRaw",
        "DUST_V": "dustVoltage",
        "DUST_UG": "dustDensity",
    }

    for raw_key, sensor_key in field_map.items():
        if raw_key in raw_values:
            sensor[sensor_key] = raw_values[raw_key]

    return sensor


def classify_dust(dust_density):
    if dust_density is None:
        return "Unknown"
    if dust_density > 150:
        return "Bad"
    if dust_density > 80:
        return "Normal"
    return "Good"


def apply_live_sensor_data(data, sensor, port):
    if not sensor:
        return data

    device = data.setdefault("device", {})
    indoor = data.setdefault("indoor", {})
    dust_density = sensor.get("dustDensity")

    device["connection"] = "live"
    device["serial"] = port
    if sensor.get("ldrRaw") is not None:
        indoor["illuminance"] = sensor["ldrRaw"]
    if sensor.get("pirMotion") is not None:
        indoor["motion"] = sensor["pirMotion"]
    if "temperature" in sensor:
        indoor["temperature"] = sensor["temperature"]
    if "humidity" in sensor:
        indoor["humidity"] = sensor["humidity"]
    if dust_density is not None:
        indoor["dustDensity"] = dust_density
        indoor["dust"] = classify_dust(dust_density)
        indoor["airQuality"] = indoor["dust"]

    led = data.setdefault("led", {})
    if sensor.get("ledPower") is not None:
        led["power"] = sensor["ledPower"]
    if sensor.get("brightness") is not None:
        led["brightness"] = sensor["brightness"]
    if sensor.get("mode") is not None:
        mode = str(sensor["mode"]).lower()
        led["mode"] = mode
        led["colorName"] = str(sensor["mode"])
        mode_rgb = {
            "auto": [200, 255, 220],
            "manual": led.get("rgb", [255, 245, 220]),
            "rest": [255, 120, 70],
            "study": [180, 220, 255],
        }
        led["rgb"] = mode_rgb.get(mode, led.get("rgb", [200, 255, 220]))
    if sensor.get("rgb") is not None:
        try:
            manual_rgb = [int(channel) for channel in str(sensor["rgb"]).split("-")]
            if led.get("mode") == "manual":
                led["rgb"] = manual_rgb
        except ValueError:
            pass

    return data


def print_ports():
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return

    print("Available serial ports:")
    for port in ports:
        print(f"- {port.device}: {port.description}")


def auto_detect_port():
    candidates = []
    for port in list_ports.comports():
        haystack = f"{port.device} {port.description} {port.manufacturer or ''}".lower()
        if any(token in haystack for token in ("usbserial", "usbmodem", "slab", "wchusbserial")):
            candidates.append(port.device)

    if candidates:
        return candidates[0]
    return None


class ArduinoBridge:
    def __init__(self, port, baud_rate=BAUD_RATE, dry_run=False):
        self.port = port
        self.baud_rate = baud_rate
        self.dry_run = dry_run
        self.lock = threading.Lock()
        self.state_lock = threading.Lock()
        self.serial = None
        self.reader_stop = threading.Event()
        self.reader_thread = None
        self.latest_sensor = None

        if self.dry_run:
            print("Dry run: serial output will be printed only.")
            return

        self.serial = serial.Serial(port, baud_rate, timeout=2)
        time.sleep(2)
        self.start_reader()

    def start_reader(self):
        if self.dry_run or not self.serial:
            return

        def read_loop():
            while not self.reader_stop.is_set():
                try:
                    line = self.serial.readline().decode("utf-8", errors="replace").strip()
                except Exception as error:
                    if not self.reader_stop.is_set():
                        print(f"serial read error: {error}")
                    return

                if line:
                    print(f"arduino: {line}")
                    sensor = parse_sensor_debug(line)
                    if sensor:
                        with self.state_lock:
                            self.latest_sensor = sensor

        self.reader_thread = threading.Thread(target=read_loop, daemon=True)
        self.reader_thread.start()

    def send_line(self, message):
        with self.lock:
            if self.dry_run:
                print(f"dry-run serial: {message.strip()}")
                return
            self.serial.write(message.encode("utf-8"))
            self.serial.flush()

    def close(self):
        self.reader_stop.set()
        if self.serial:
            self.serial.close()
        if self.reader_thread:
            self.reader_thread.join(timeout=1)

    def sensor_state(self):
        with self.state_lock:
            if not self.latest_sensor:
                return None
            return dict(self.latest_sensor)


def send_once(port, data_file, dry_run, baud_rate):
    data = load_dashboard_data(data_file)
    message = build_display_message(data)
    bridge = ArduinoBridge(port, baud_rate=baud_rate, dry_run=dry_run)
    try:
        bridge.send_line(message)
    finally:
        bridge.close()

    print(f"sent to {port}: {message.strip()}")


def start_data_loop(bridge, data_file, interval):
    def run():
        while True:
            try:
                data = load_dashboard_data(data_file)
                message = build_display_message(data)
                bridge.send_line(message)
                print(f"sent: {message.strip()}")
            except Exception as error:
                print(f"bridge data error: {error}")
            time.sleep(interval)

    thread = threading.Thread(target=run, daemon=True)
    thread.start()


def make_handler(bridge, data_file):
    class BridgeHandler(BaseHTTPRequestHandler):
        def _send_json(self, status_code, payload):
            body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
            self.send_response(status_code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            self.wfile.write(body)

        def do_OPTIONS(self):
            self._send_json(200, {"ok": True})

        def do_GET(self):
            if self.path == "/health":
                self._send_json(
                    200,
                    {
                        "ok": True,
                        "port": bridge.port,
                        "dryRun": bridge.dry_run,
                        "dataFile": str(data_file),
                    },
                )
                return

            if self.path == "/state":
                try:
                    data = load_dashboard_data(data_file)
                except Exception as error:
                    self._send_json(500, {"ok": False, "error": str(error)})
                    return
                sensor = bridge.sensor_state()
                data = apply_live_sensor_data(data, sensor, bridge.port)
                self._send_json(200, {"ok": True, "data": data, "lcdLines": build_lcd_lines(data), "sensor": sensor})
                return

            self._send_json(404, {"ok": False, "error": "not found"})

        def do_POST(self):
            if self.path not in {"/lcd", "/power", "/brightness", "/mode", "/color"}:
                self._send_json(404, {"ok": False, "error": "not found"})
                return

            content_length = int(self.headers.get("Content-Length", "0"))
            raw_body = self.rfile.read(content_length) if content_length else b"{}"

            try:
                payload = json.loads(raw_body.decode("utf-8"))
                if self.path == "/brightness":
                    brightness = parse_percent(payload["brightness"])
                    command = f"CMD,BRIGHT,{brightness}\n"
                    response_payload = {"ok": True, "brightness": brightness, "sent": command.strip()}
                elif self.path == "/color":
                    red, green, blue = parse_rgb(payload["rgb"])
                    command = f"CMD,COLOR,{red},{green},{blue}\n"
                    response_payload = {"ok": True, "rgb": [red, green, blue], "mode": "MANUAL", "sent": command.strip()}
                elif self.path == "/mode":
                    mode = parse_mode(payload["mode"])
                    command = f"CMD,MODE,{mode}\n"
                    response_payload = {"ok": True, "mode": mode, "sent": command.strip()}
                else:
                    power = parse_bool(payload["power"])
                    command = "CMD,POWER,ON\n" if power else "CMD,POWER,OFF\n"
                    response_payload = {"ok": True, "power": power, "sent": command.strip()}
            except Exception as error:
                self._send_json(400, {"ok": False, "error": str(error)})
                return

            try:
                bridge.send_line(command)
            except Exception as error:
                self._send_json(500, {"ok": False, "error": str(error)})
                return

            self._send_json(200, response_payload)

        def log_message(self, format, *args):
            return

    return BridgeHandler


def run_bridge(port, data_file, interval, dry_run, baud_rate):
    bridge = ArduinoBridge(port, baud_rate=baud_rate, dry_run=dry_run)
    start_data_loop(bridge, data_file, interval)

    server = ThreadingHTTPServer((BRIDGE_HOST, BRIDGE_PORT), make_handler(bridge, data_file))
    print(f"Bridge server: http://{BRIDGE_HOST}:{BRIDGE_PORT}")
    print(f"Serial target: {port} @ {baud_rate}")
    print(f"Data file: {data_file}")
    print("Use the web power button to send ON/OFF commands.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Stopping bridge.")
    finally:
        server.server_close()
        bridge.close()


def resolve_port(port, dry_run):
    if port != "auto":
        return port
    detected = auto_detect_port()
    if detected:
        return detected
    if dry_run:
        return "dry-run"
    raise RuntimeError("No Arduino/ESP32 serial port found. Run with --list-ports and pass --port.")


def main():
    parser = argparse.ArgumentParser(
        description="Send dashboard JSON and web ON/OFF commands to Arduino/ESP32 over USB serial."
    )
    parser.add_argument("--port", default="auto", help="Arduino Uno serial port, or auto")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help="serial baud rate")
    parser.add_argument("--data-file", default=DEFAULT_DATA_FILE, type=Path, help="dashboard JSON file")
    parser.add_argument("--interval", type=int, default=5, help="send interval in seconds")
    parser.add_argument("--once", action="store_true", help="send once and exit")
    parser.add_argument("--dry-run", action="store_true", help="print serial messages without hardware")
    parser.add_argument("--list-ports", action="store_true", help="show serial ports and exit")
    args = parser.parse_args()

    if args.list_ports:
        print_ports()
        return

    data_file = args.data_file.resolve()
    port = resolve_port(args.port, args.dry_run)

    if args.once:
        send_once(port, data_file, args.dry_run, args.baud)
        return

    print("Close Arduino IDE Serial Monitor before running this script.")
    run_bridge(port, data_file, args.interval, args.dry_run, args.baud)


if __name__ == "__main__":
    main()
