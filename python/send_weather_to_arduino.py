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


def build_lcd_lines(data):
    indoor = data.get("indoor", {})
    weather = data.get("weather", {})
    led = data.get("led", {})
    ai_message = get_ai_message(data)

    power_text = "ON" if led.get("power", True) else "OFF"
    brightness = led.get("brightness", 0)
    line1 = f"LED {power_text} {brightness}%"
    line2 = f"W:{weather.get('status', '-')} {weather.get('outsideTemperature', '-')}C"
    line3 = f"In:{indoor.get('temperature', '-')}C H{indoor.get('humidity', '-')}%"
    line4 = ai_message

    return [sanitize_display_text(line) for line in (line1, line2, line3, line4)]


def build_display_message(data):
    return "DATA," + "|".join(build_lcd_lines(data)) + "\n"


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
        self.serial = None

        if self.dry_run:
            print("Dry run: serial output will be printed only.")
            return

        self.serial = serial.Serial(port, baud_rate, timeout=2)
        time.sleep(2)

    def send_line(self, message):
        with self.lock:
            if self.dry_run:
                print(f"dry-run serial: {message.strip()}")
                return
            self.serial.write(message.encode("utf-8"))
            self.serial.flush()

    def close(self):
        if self.serial:
            self.serial.close()


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
                self._send_json(200, {"ok": True, "data": data, "lcdLines": build_lcd_lines(data)})
                return

            self._send_json(404, {"ok": False, "error": "not found"})

        def do_POST(self):
            if self.path not in {"/lcd", "/power", "/brightness", "/mode"}:
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
    raise RuntimeError("No Arduino Uno serial port found. Run with --list-ports and pass --port.")


def main():
    parser = argparse.ArgumentParser(
        description="Send dummy dashboard JSON and web ON/OFF commands to Arduino Uno over USB serial."
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
