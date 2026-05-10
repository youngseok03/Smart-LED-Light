import os
import requests
from datetime import datetime, timedelta
from dotenv import load_dotenv

load_dotenv()

SERVICE_KEY = os.environ.get("WEATHER_API_KEY")
URL = "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtFcst"


def get_base_datetime():
    """
    초단기예보 base_date, base_time 계산.
    발표 시각은 매시 :30분, 자료 제공은 발표 후 약 10분 뒤(:40분).
    """
    now = datetime.now()
    if now.minute >= 40:
        base = now.replace(minute=30, second=0, microsecond=0)
    else:
        base = (now - timedelta(hours=1)).replace(minute=30, second=0, microsecond=0)
    return base.strftime("%Y%m%d"), base.strftime("%H%M")


def get_weather(nx=60, ny=127):
    """
    기상청 초단기예보 API 호출.
    nx, ny: 격자 좌표 (기본값: 서울 중구 부근)
    반환: 예보 아이템 리스트
    """
    if not SERVICE_KEY:
        raise ValueError("WEATHER_API_KEY가 .env 파일에 설정되지 않았습니다.")

    base_date, base_time = get_base_datetime()

    params = {
        "serviceKey": SERVICE_KEY,
        "pageNo": "1",
        "numOfRows": "100",
        "dataType": "JSON",
        "base_date": base_date,
        "base_time": base_time,
        "nx": str(nx),
        "ny": str(ny),
    }

    response = requests.get(URL, params=params, timeout=10)
    response.raise_for_status()

    data = response.json()
    header = data["response"]["header"]

    if header["resultCode"] != "00":
        raise RuntimeError(f"API 오류: {header['resultMsg']}")

    items = data["response"]["body"]["items"]["item"]
    return items


def parse_rn1(raw_value):
    """
    RN1(1시간 강수량) 값을 안전하게 처리.
    API는 강수가 없을 때 "강수없음" 문자열을 반환하고,
    강수가 있을 때는 "0.5", "1.0" 같은 숫자 문자열을 반환함.
    1mm 미만은 "1.0mm 미만"처럼 문자열로 올 수도 있음.
    → 숫자로 변환 가능하면 float로, 아니면 0.0으로 처리함.
    """
    if raw_value is None:
        return 0.0
    # 숫자 변환 시도: 성공하면 강수량(mm), 실패하면 0.0으로 처리
    try:
        return float(raw_value)
    except ValueError:
        # "강수없음", "1.0mm 미만" 등 문자열인 경우 0.0으로 처리
        return 0.0


def parse_weather(items):
    """
    예보 아이템에서 주요 날씨 정보만 추출.
    category 코드: T1H=기온, SKY=하늘상태, PTY=강수형태, REH=습도, WSD=풍속, RN1=1시간강수량
    """
    weather = {}
    # RN1(1시간 강수량)을 추가함 — LED 색상 판단에 필요
    target_categories = {"T1H", "SKY", "PTY", "REH", "WSD", "RN1"}

    for item in items:
        category = item.get("category")
        if category in target_categories:
            fcst_time = item.get("fcstTime")
            if fcst_time not in weather:
                weather[fcst_time] = {}
            raw_value = item.get("fcstValue")
            # RN1은 "강수없음" 같은 문자열이 올 수 있으므로 별도 처리
            if category == "RN1":
                weather[fcst_time][category] = parse_rn1(raw_value)
            else:
                weather[fcst_time][category] = raw_value

    return weather


SKY_CODE = {"1": "맑음", "3": "구름많음", "4": "흐림"}
PTY_CODE = {"0": "없음", "1": "비", "2": "비/눈", "3": "눈", "5": "빗방울", "6": "빗방울눈날림", "7": "눈날림"}


def print_weather_summary(weather):
    print(f"{'시각':<8} {'기온':>6} {'하늘':<8} {'강수':<8} {'습도':>6} {'풍속':>6} {'강수량':>8}")
    print("-" * 62)
    for fcst_time, values in sorted(weather.items()):
        temp = values.get("T1H", "-")
        sky = SKY_CODE.get(values.get("SKY", ""), "-")
        pty = PTY_CODE.get(values.get("PTY", "0"), "-")
        humidity = values.get("REH", "-")
        wind = values.get("WSD", "-")
        # RN1은 parse_rn1()에서 이미 float로 변환됨 — 없으면 0.0으로 표시
        rn1 = values.get("RN1", 0.0)
        print(f"{fcst_time:<8} {temp:>5}°C {sky:<8} {pty:<8} {humidity:>5}% {wind:>5}m/s {rn1:>6.1f}mm")


if __name__ == "__main__":
    try:
        items = get_weather(nx=60, ny=127)
        weather = parse_weather(items)
        print_weather_summary(weather)
    except ValueError as e:
        print(f"설정 오류: {e}")
    except requests.RequestException as e:
        print(f"네트워크 오류: {e}")
    except RuntimeError as e:
        print(f"API 오류: {e}")
