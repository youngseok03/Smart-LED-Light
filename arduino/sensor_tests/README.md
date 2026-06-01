# Arduino Uno Sensor Test Sketches

센서를 하나씩 확인하기 위한 Arduino Uno R3 단독 테스트 코드 모음입니다.

ESP32 통합 회로와 브레드보드 전원 모듈 배선은 프로젝트 루트의 `README.md`를 기준으로 봅니다. 현재 ESP32 통합 회로는 브레드보드 `3.3V` 레일을 OLED/DHT11에, `5.5V` 레일을 WS2812/GP2Y1014AU에 쓰고, 모든 GND를 공통으로 묶습니다.

## ESP32 GP2Y1014AU Dust Sensor

폴더:

```text
arduino/sensor_tests/ESP32_GP2Y1014AU_Dust_Test/ESP32_GP2Y1014AU_Dust_Test.ino
```

현재 통합 코드와 같은 핀 기준입니다.

```text
GP2Y Vo/AO    -> ESP32 GPIO35
GP2Y LED      -> ESP32 GPIO27
GP2Y VCC      -> 5V rail
GP2Y S-GND    -> GND rail
GP2Y V-LED    -> 150 ohm resistor -> 5V rail
GP2Y LED-GND  -> GND rail
220uF +       -> GP2Y V-LED
220uF -       -> GND rail
```

Serial Monitor baud rate는 `115200`입니다. `raw`, `adc_v`, `voltage_over_clean`, `dust_ugm3`가 1초마다 출력됩니다.

## Arduino Uno GP2Y1014AU Dust Sensor

폴더:

```text
arduino/sensor_tests/Uno_GP2Y1014AU_Dust_Test/Uno_GP2Y1014AU_Dust_Test.ino
```

```text
GP2Y Vo/AO    -> Arduino A0
GP2Y LED      -> Arduino D7
GP2Y VCC      -> Arduino 5V
GP2Y S-GND    -> Arduino GND
GP2Y V-LED    -> 150 ohm resistor -> Arduino 5V
GP2Y LED-GND  -> Arduino GND
220uF +       -> GP2Y V-LED
220uF -       -> Arduino GND
```

Serial Monitor baud rate는 `9600`입니다. GP2Y1014AU 기준 no-dust 전압은 우선 `0.6V`로 계산합니다. 깨끗한 공기에서 `A0_voltage` 평균이 다르면 코드의 `CLEAN_AIR_VOLTAGE` 값을 그 평균에 맞춰 보정합니다.

## LDR Light Sensor

폴더:

```text
arduino/sensor_tests/LDR_Light_Sensor/LDR_Light_Sensor.ino
```

### 3핀/4핀 조도 센서 모듈 배선

아날로그 출력이 있는 LDR 모듈 기준입니다.

```text
LDR VCC / +  -> Arduino 5V
LDR GND / -  -> Arduino GND
LDR AO / A0  -> Arduino A0
```

모듈에 `DO` 핀이 있으면 디지털 임계값 출력입니다. 이번 테스트는 밝기 변화를 숫자로 보기 위해 `AO`를 사용합니다.

### 확인 방법

1. Arduino IDE에서 `LDR_Light_Sensor.ino`를 엽니다.
2. Board를 `Arduino Uno`, Port를 현재 Uno 포트로 선택합니다.
3. 업로드 후 Serial Monitor를 엽니다.
4. baud rate를 `9600`으로 맞춥니다.
5. 센서를 손으로 가리거나 빛을 비춰 `raw` 값이 변하는지 확인합니다.

값의 방향은 모듈 회로에 따라 다를 수 있습니다. 일반적으로 raw 값이 안정적으로 변하면 센서와 배선은 정상입니다.
