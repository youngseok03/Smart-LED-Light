# Smart LED Light

## Project Goal

Control an LED light based on real-time weather data retrieved via a weather API.
The Arduino reads weather information sent from a Python script over USB Serial and adjusts the LED output accordingly.

## Current Board

Arduino UNO

## Current Stage

Initial setup and Arduino connection test.

## Folder Structure

```
Smart-LED-Light/
├── arduino/
│   └── smart_led_light/
│       └── smart_led_light.ino   # Arduino sketch: serial print + built-in LED blink
├── python/
│   └── send_weather_to_arduino.py  # Placeholder for weather API → Arduino serial script
├── docs/                           # Documentation (empty for now)
└── README.md
```

## Next Steps

- Upload the basic LED test code using Arduino IDE
- Test Serial Monitor output
- Test USB Serial communication between Python and Arduino
- Add weather API integration
- Add LED output logic
