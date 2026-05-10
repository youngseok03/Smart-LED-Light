"""
led_color.py
기상청 초단기예보(getUltraSrtFcst)에서 가져온 날씨 값을 이용해
Smart LED 색상을 자동으로 결정하는 모듈.

- 6개 피처 사용: T1H, SKY, PTY, RN1, REH, WSD
- decide_led_color()가 메인 함수
- 단독 실행 시: 가장 가까운 예보 시각을 골라 LED 색상을 출력
"""

import json
from datetime import datetime
from pathlib import Path

# 기존 weather_api.py 재사용 (RN1 포함 6개 피처를 이미 가져옴)
from weather_api import get_weather, parse_weather, SKY_CODE, PTY_CODE


# =====================================================================
# 1) 안전한 숫자 변환 함수
#    API가 "강수없음", "1.0mm 미만" 같은 문자열을 줘도 죽지 않도록 처리
# =====================================================================
def safe_float(value, default=0.0):
    """value를 float로 변환. 실패하면 default 반환."""
    if value is None:
        return default
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def safe_int(value, default=0):
    """value를 int로 변환. 문자열 "1", 정수 1, 실수 1.0 모두 1로 처리."""
    if value is None:
        return default
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


# =====================================================================
# 2) 보조 함수: 계절 / 시간대 / 장마(지속적 비) 판단
# =====================================================================
def estimate_season(month=None):
    """월(1~12)을 받아 계절 문자열 반환. None이면 현재 월 사용."""
    if month is None:
        month = datetime.now().month
    if 3 <= month <= 5:
        return "spring"
    if 6 <= month <= 8:
        return "summer"
    if 9 <= month <= 11:
        return "autumn"
    return "winter"


def is_daytime(hour):
    """낮 시간대(9~17시)인지 판단."""
    return 9 <= hour <= 17


def is_evening(hour):
    """저녁 시간대(18~22시)인지 판단."""
    return 18 <= hour <= 22


def is_persistent_rain(rain_history, min_records=3, ratio=0.6):
    """
    rain_history(최근 기록 리스트)를 보고 '지속적인 비'인지 판단.
    각 항목 형식: {"pty": 1, "rn1": 1.0} (다른 키가 더 있어도 무시)

    한계:
    - 초단기예보(getUltraSrtFcst) 단일 호출에는 미래 약 6시간 정보만 들어 있어
      "오늘 종일 비 왔다"는 과거형 사실을 단일 호출만으로 알 수 없음.
    - 따라서 1시간마다 호출 결과를 누적 저장한 rain_history를 입력으로
      받아야 정확한 장마 판단이 가능.
    """
    if not rain_history or len(rain_history) < min_records:
        return False
    rain_count = 0
    for record in rain_history:
        pty = safe_int(record.get("pty"))
        rn1 = safe_float(record.get("rn1"))
        if pty in (1, 2, 5, 6) or rn1 > 0:
            rain_count += 1
    return (rain_count / len(rain_history)) >= ratio


# =====================================================================
# 3) rain_history 간단 저장/불러오기 (선택)
#    1회 실행마다 PTY/RN1을 누적 저장 → 장마 판단 데이터로 활용
# =====================================================================
HISTORY_FILE = Path(__file__).parent / "rain_history.json"


def load_rain_history():
    """rain_history.json에서 기록 불러오기. 파일 없으면 빈 리스트."""
    if not HISTORY_FILE.exists():
        return []
    try:
        with HISTORY_FILE.open("r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return []


def append_rain_record(values, fcst_date=None, fcst_time=None, max_records=12):
    """
    이번 호출의 PTY/RN1을 rain_history.json에 추가.

    중복 방지 규칙:
    - (fcst_date, fcst_time)이 같은 기록이 이미 있으면 새로 추가하지 않음
    - 즉, 같은 예보 데이터를 짧은 간격으로 여러 번 호출해도 한 번만 저장됨

    각 기록 구조:
    {
        "collected_at": "YYYY-MM-DDTHH:MM:SS",  # 저장 시각
        "fcst_date":    "YYYYMMDD",             # 예보 날짜
        "fcst_time":    "HHMM",                 # 예보 시각
        "pty": "0",
        "rn1": 0.0
    }

    파일이 무한히 커지지 않도록 중복 제거 후 최근 max_records개만 유지.
    """
    # 인자가 없으면 현재 시각 기준으로 채움 (단독 사용 시 안전장치)
    if fcst_date is None:
        fcst_date = datetime.now().strftime("%Y%m%d")
    if fcst_time is None:
        fcst_time = datetime.now().strftime("%H%M")

    history = load_rain_history()

    # 1) 같은 (fcst_date, fcst_time) 기록이 이미 있으면 → append 생략
    for rec in history:
        if (rec.get("fcst_date") == fcst_date
                and rec.get("fcst_time") == fcst_time):
            return history  # 중복: 파일 변경 없이 반환

    # 2) 새로운 예보 시각이면 추가
    history.append({
        "collected_at": datetime.now().isoformat(timespec="seconds"),
        "fcst_date": fcst_date,
        "fcst_time": fcst_time,
        "pty": values.get("PTY", "0"),
        "rn1": values.get("RN1", 0.0),
    })

    # 3) 중복 제거 후 최근 max_records개만 유지
    history = history[-max_records:]

    try:
        with HISTORY_FILE.open("w", encoding="utf-8") as f:
            json.dump(history, f, ensure_ascii=False, indent=2)
    except OSError:
        # 저장 실패해도 메인 흐름은 계속 진행
        pass
    return history


# =====================================================================
# 4) LED 색상 결정 메인 함수
# =====================================================================
def decide_led_color(values, mode=None, season=None, current_hour=None, rain_history=None):
    """
    날씨 값과 옵션을 받아 LED 색상을 결정.

    Parameters
    ----------
    values : dict
        weather[fcstTime] 형태의 dict.
        예: {"T1H": "21", "SKY": "4", "PTY": "1", "RN1": 1.0, "REH": "85", "WSD": "2.1"}
    mode : str | None
        "study"=공부/집중 모드, "rest"=휴식/저녁 모드, None=날씨만 기준.
    season : str | None
        "spring"/"summer"/"autumn"/"winter" 또는 None(현재 월 자동 추정).
    current_hour : int | None
        0~23. None이면 datetime.now().hour 사용.
    rain_history : list | None
        장마 판단을 위한 최근 기록 리스트. None이면 장마 분기 비활성화.

    Returns
    -------
    dict
        {"weather_status": str, "color_name": str, "rgb": (r,g,b), "reason": str}
    """
    # --- 입력값 안전 변환 ---
    pty = safe_int(values.get("PTY"), 0)
    sky = safe_int(values.get("SKY"), 0)
    rn1 = safe_float(values.get("RN1"), 0.0)
    t1h = safe_float(values.get("T1H"), 0.0)
    reh = safe_float(values.get("REH"), 0.0)
    wsd = safe_float(values.get("WSD"), 0.0)

    # --- 계절/시간대 자동 보정 ---
    if season is None:
        season = estimate_season()
    if current_hour is None:
        current_hour = datetime.now().hour

    # --- 모든 분기 공통 reason 문자열 ---
    reason_base = (
        f"PTY={PTY_CODE.get(str(pty), pty)}, "
        f"RN1={rn1:.1f}mm, "
        f"SKY={SKY_CODE.get(str(sky), sky)}, "
        f"T1H={t1h}, REH={int(reh)}, WSD={wsd}"
    )

    def result(status, color_name, rgb, extra=""):
        return {
            "weather_status": status,
            "color_name": color_name,
            "rgb": rgb,
            "reason": f"{extra} | {reason_base}" if extra else reason_base,
        }

    # ========== 우선순위에 따른 분기 ==========

    # 1. 휴식/저녁 모드 (사용자가 직접 선택)
    if mode == "rest":
        return result("휴식이 필요한 저녁 / 비 오고 지친 날",
                      "따뜻한 흰색", (255, 230, 200),
                      "mode=rest 선택")

    # 2. 공부/집중 모드 (사용자가 직접 선택)
    if mode == "study":
        return result("공부/집중이 필요한 맑은 낮 시간",
                      "차가운 흰색", (235, 245, 255),
                      "mode=study 선택")

    # 3. 눈 오는 날 / 겨울의 매우 어두운 날
    if pty in (2, 3, 6, 7):
        return result("눈 오는 날 / 겨울의 매우 어두운 날",
                      "따뜻한 흰색", (255, 240, 210),
                      "PTY가 눈/진눈깨비")

    # 4. 장마처럼 지속적인 비 (rain_history 기반)
    if rain_history is not None and is_persistent_rain(rain_history):
        return result("장마처럼 하루 종일 비 오는 날",
                      "따뜻한 흰색", (255, 240, 210),
                      "rain_history 다수가 비")

    # 5. 맑지만 매우 더운 여름날 (★조건 수정: '무더운 날'보다 우선)
    if sky == 1 and pty == 0 and t1h >= 30:
        return result("맑지만 매우 더운 여름날",
                      "청록색", (120, 240, 230),
                      "맑음 + 30℃ 이상")

    # 6. 무더운 날
    if t1h >= 30 or (t1h >= 28 and reh >= 70):
        return result("무더운 날", "청록색", (100, 235, 225),
                      "30℃ 이상 또는 28℃+습도70 이상")

    # 7. 비 오는 날
    if pty in (1, 5) or rn1 > 0:
        return result("비 오는 날", "연한 초록", (200, 255, 220),
                      "비/빗방울 또는 RN1>0")

    # 8. 비 오기 직전 먹구름 낀 날
    if sky == 4 and pty == 0 and rn1 == 0 and reh >= 80:
        return result("비 오기 직전 먹구름 낀 날",
                      "따뜻한 흰색", (255, 245, 225),
                      "흐림 + 습도 80% 이상")

    # 9. 봄·여름에 바람까지 강한 날
    if wsd >= 6 and t1h >= 18:
        return result("봄·여름에 바람까지 강한 날",
                      "연한 초록", (190, 255, 210),
                      "WSD≥6 + 18℃ 이상")

    # 10. 바람이 강한 날
    if wsd >= 6:
        return result("바람이 강한 날", "연한 하늘색", (200, 235, 255),
                      "WSD≥6")

    # 11. 맑은 아침 (햇살/기상/활력) — 일반 맑은 날보다 먼저 판단
    if sky == 1 and pty == 0 and rn1 == 0 and 6 <= current_hour <= 10:
        return result("맑은 아침",
                      "연한 노란색", (255, 245, 180),
                      "맑고 강수 없음 + 아침 시간대 조건 만족")

    # 12. 겨울인데 맑은 날
    if sky == 1 and pty == 0 and rn1 == 0 and t1h <= 5:
        return result("겨울인데 맑은 날", "따뜻한 흰색", (255, 235, 205),
                      "맑음 + 5℃ 이하")

    # 13. 맑고 선선한 봄날
    if sky == 1 and pty == 0 and rn1 == 0 and 15 <= t1h <= 22:
        return result("맑고 선선한 봄날", "연한 초록", (200, 255, 215),
                      "맑음 + 15~22℃")

    # 14. 가을에 흐리고 선선한 날
    if season == "autumn" and sky == 4 and pty == 0 and rn1 == 0 and 15 <= t1h <= 22:
        return result("가을에 흐리고 선선한 날",
                      "따뜻한 흰색", (255, 240, 220),
                      "가을 + 흐림 + 15~22℃")

    # 15. 흐린 날
    if sky == 4 and pty == 0 and rn1 == 0:
        return result("흐린 날", "연한 하늘색", (210, 240, 255),
                      "흐림 + 강수 없음")

    # 16. 맑은 날
    if sky == 1 and pty == 0 and rn1 == 0:
        return result("맑은 날", "연한 초록", (210, 255, 220),
                      "맑음 + 강수 없음")

    # 17. 기본값
    return result("기본값", "중간 흰색", (240, 240, 240),
                  "어떤 조건에도 해당하지 않음")


# =====================================================================
# 5) 가장 가까운 예보 시각 찾기
# =====================================================================
def find_nearest_forecast_time(weather, current=None):
    """
    weather dict에서 현재 시각과 가장 가까운 fcstTime("HHMM") 반환.
    """
    if not weather:
        return None
    if current is None:
        current = datetime.now()

    def diff_minutes(fcst_time):
        h, m = int(fcst_time[:2]), int(fcst_time[2:])
        return abs((h * 60 + m) - (current.hour * 60 + current.minute))

    return min(weather.keys(), key=diff_minutes)


# =====================================================================
# 6) 메인 실행: 가장 가까운 예보 기준으로 LED 색상 출력
# =====================================================================
if __name__ == "__main__":
    import requests  # 예외 타입 사용

    try:
        items = get_weather(nx=60, ny=127)
        weather = parse_weather(items)

        nearest = find_nearest_forecast_time(weather)
        if nearest is None:
            print("예보 데이터를 가져오지 못했습니다.")
        else:
            values = weather[nearest]

            # rain_history 자동 누적 (장마 판단용)
            # 같은 (fcst_date, fcst_time)은 중복 저장되지 않음
            fcst_date = datetime.now().strftime("%Y%m%d")
            history = append_rain_record(values,
                                         fcst_date=fcst_date,
                                         fcst_time=nearest)

            led = decide_led_color(values, rain_history=history)

            print(f"현재 예보 시각: {nearest}")
            print(f"날씨 상태: {led['weather_status']}")
            print(f"추천 LED 색상: {led['color_name']}")
            r, g, b = led["rgb"]
            print(f"RGB: {r}, {g}, {b}")
            print(f"판단 근거: {led['reason']}")
    except ValueError as e:
        print(f"설정 오류: {e}")
    except requests.RequestException as e:
        print(f"네트워크 오류: {e}")
    except RuntimeError as e:
        print(f"API 오류: {e}")
