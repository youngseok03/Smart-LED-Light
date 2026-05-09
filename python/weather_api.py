import requests

SERVICE_KEY = "98dc9316ebf75f3b58dbe7038d1e770f3d97f121a59a41075696f18792779a9f"

url = "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtFcst"

params = {
    'serviceKey': SERVICE_KEY,
    'pageNo': '1',
    'numOfRows': '100',
    'dataType': 'JSON',
    'base_date': '20260510',
    'base_time': '0630',
    'nx': '60',
    'ny': '127'
}

response = requests.get(url, params=params)

print(response.url)
print(response.status_code)
print(response.text)