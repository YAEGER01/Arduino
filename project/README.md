# Hallway Sensor Monitoring System

This project detects presence, movement, and loitering in a hallway and shows the live data on a web dashboard. The current v6.7 firmware uses an Arduino Uno, a PIR sensor, an ultrasonic distance sensor, a sound sensor, LEDs, and a buzzer to classify what is happening near the hallway entrance.

The system is built as a small data pipeline:

1. The Arduino reads sensors and decides which state the hallway is in.
2. The Arduino sends a compact sensor packet to a NodeMCU/ESP8266 over serial.
3. The ESP8266 publishes that packet to MQTT.
4. A Python MQTT consumer stores the data in MySQL.
5. A Flask app reads the database and shows the latest records on the dashboard.

## How It Works

The firmware in [src/v6.7.ino](src/v6.7.ino) runs a simple state machine:

- `CLEAR` means no one is nearby.
- `WALK_IN` means movement toward the hallway is detected.
- `WALK_OUT` means movement away from the hallway is detected.
- `LINGER` means a person is standing still long enough to be watched.
- `LOITER` means the person stayed too long and the alarm is triggered.
- `SLEEP` means the system has been idle long enough to reduce activity.

The sketch uses filtered ultrasonic readings, PIR debouncing, and sound detection to decide when to switch states. When loitering is detected, the buzzer and LEDs switch into alarm behavior. The Arduino also sends periodic sensor updates for logging and web display.

### Presence & Blockage Detection

The system determines whether the hallway passage is **clear or obstructed** by fusing three sensor inputs on the Arduino:

1. **Ultrasonic distance sensor (HC-SR04):** Measures the distance to the nearest object in front of the sensor. The firmware averages 5 consecutive readings to filter out noise. If the distance drops below **30 cm**, the system registers a **presence** — something (or someone) is standing in the passage.

2. **PIR motion sensor:** Detects changes in infrared radiation caused by a moving body. The signal is **debounced** — once triggered, the system treats motion as active for **2 seconds** after the last trigger, preventing rapid on/off flickering from a single movement.

3. **Sound sensor:** An analog microphone module. If the raw reading exceeds **600** (out of 1023), the system considers a significant sound event has occurred, adding a third confirmation layer.

These three inputs feed into a **state machine** that classifies the hallway into one of six states:

| State              | Meaning                                                   | How it's reached                                                                                    |
| ------------------ | --------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| **CLEAR**          | Passage is open, no one nearby                            | Default state; no sensors triggered                                                                 |
| **WALK_IN**        | Someone is entering the hallway                           | Distance drops below 30 cm _and/or_ PIR triggers within 120 cm range                                |
| **WALK_OUT**       | Someone is leaving the hallway                            | Distance increases by more than 60 cm while presence was active                                     |
| **LINGER**         | Person has stopped moving near the entrance               | Distance remains stable (within 60 cm change) for more than 1 second during WALK_IN                 |
| **LOITER (alarm)** | Person has been stationary too long — **passage blocked** | Person stays still within a 15 cm radius for more than **5 seconds**; buzzer and alarm LED activate |
| **SLEEP**          | No activity for 30 seconds; system in low-power idle      | Automatically exits on any sensor activity                                                          |

In short: the system doesn't just detect _that_ someone is present — it tracks _how long_ they stay still. A brief passage (walk in, walk out) is normal. A person who stops and remains for more than 5 seconds triggers the **blockage alarm**, indicating the hallway passage is obstructed.

The backend in [serve/v6.7.py](serve/v6.7.py) listens to MQTT messages, parses the payload, and saves each reading into MySQL. The Flask app in [serve/app.py](serve/app.py) exposes the dashboard and an API endpoint at `/api/data`.

## Features

- Real-time hallway presence detection.
- Movement classification for entering and exiting traffic.
- Loiter detection with a timed alarm.
- Noise-filtered ultrasonic distance sampling.
- PIR signal debouncing to reduce false triggers.
- Buzzer and LED feedback for each major state.
- MQTT-based logging pipeline.
- MySQL storage for historical sensor data.
- Web dashboard with live data refresh.
- Raw database timestamps shown exactly as stored, without time conversion.

## Project Layout

```text
project/
├── src/v6.7.ino        Arduino firmware for sensing and alarm logic
├── esp/ESPv6.7.ino     ESP8266 MQTT bridge
├── serve/
│   ├── app.py          Flask dashboard and API
│   ├── v6.7.py         MQTT consumer that writes to MySQL
│   └── db_config.py    Database configuration and auto-setup
├── templates/
│   └── index.html      Dashboard UI
├── s1.ps1              Starts the Python services
├── requirements.txt    Python dependencies
└── wire/wiring.txt     Wiring reference
```

## Hardware

Current active firmware is written for one sensor set. The exact pin mapping is documented in [src/v6.7.ino](src/v6.7.ino) and [wire/wiring.txt](wire/wiring.txt).

Typical parts used by the project:

- Arduino Uno
- ESP8266 / NodeMCU
- PIR motion sensor
- HC-SR04 ultrasonic sensor
- Sound sensor
- Buzzer
- White LED
- Red LED

## Data Format

The Arduino sends a pipe-delimited packet through serial.

```text
SET1|STATE|DISTANCE|PIR|SOUND_DETECTED|SOUND_LEVEL|ALARM_ACTIVE
```

Example:

```text
SET1|LINGER|42|1|0|350|0
```

Meaning:

- `SET1` is the sensor set name.
- `LINGER` is the current state.
- `42` is the distance in centimeters.
- `1` means PIR motion is active.
- `0` means sound threshold was not exceeded.
- `350` is the raw sound reading.
- `0` means the alarm is not active.

## Setup

### 1. Install dependencies

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

### 2. Configure the database

The database settings come from environment variables in `serve/db_config.py`. If no custom `.env` file is present, the project uses the default values defined there.

The first run creates the `hallway_db` database plus the required tables automatically.

### 3. Start the Python services

Run the PowerShell helper script from the project root:

```powershell
.\s1.ps1
```

This starts:

- the Flask app on `http://localhost:5000`
- the MQTT consumer that writes sensor data to MySQL

### 4. Upload the firmware

Upload [src/v6.7.ino](src/v6.7.ino) to the Arduino Uno and [esp/ESPv6.7.ino](esp/ESPv6.7.ino) to the ESP8266.

### 5. Open the dashboard

Visit:

```text
http://localhost:5000/dashboard
```

## Notes

- The Arduino sketch is documented with comments around the tuning variables and state machine.
- `SLEEP` is an idle mode, not a power-off state.
- Legacy files remain in the repo for reference, but the current active flow is the v6.7 path described above.

## Troubleshooting

- If the dashboard shows no data, confirm that Mosquitto is running and that the MQTT consumer is connected.
- If MySQL errors appear, verify the credentials in `serve/db_config.py` and ensure the database service is running.
- If the sensor readings look unstable, adjust the tuning variables in [src/v6.7.ino](src/v6.7.ino).
