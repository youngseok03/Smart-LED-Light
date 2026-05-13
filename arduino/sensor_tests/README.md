# ESP32 Sensor Test Sketches

These sketches are standalone sensor tests extracted from the main `smart_led_light` firmware. The main integrated file is not modified by these test sketches.

Open each folder in Arduino IDE and upload the matching `.ino` file:

| Folder | Sensor | ESP32 pin |
|---|---|---:|
| `DHT11` | DHT11 temperature/humidity sensor | GPIO4 |
| `PIR_Motion_Sensor` | PIR motion sensor | GPIO27 |
| `LDR_Light_Sensor` | LDR light sensor voltage divider | GPIO34 |
| `GP2Y1014AU_Dust_Sensor` | GP2Y1014AU dust sensor | GPIO35, GPIO23 |

Use Serial Monitor at `115200` baud for all sketches.
