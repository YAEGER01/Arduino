/*
 * SENSOR FUSION ARCHITECTURE - Ultrasonic primary, PIR+Sound secondary
 * Zones: Set 1 (Left Hallway), Set 2 (Right Hallway)
 * Shared: Buzzer pin 9
 *
 * Logic: Ultrasonic triggers → 500ms window → require PIR OR sound to confirm
 * - No confirmation within window = reset
 * - Confirmed presence = white LED ON
 * - 3 seconds confirmed = siren (alternating white/red)
 * - All sensors clear >1s = reset to idle
 */

#include <SoftwareSerial.h>

// Configuration: pin assignments only (immutable)
struct ZoneConfig {
  int pirPin, trigPin, echoPin, soundPin, whitePin, redPin;
};

// Runtime state (mutable)
struct ZoneState {
  unsigned long candidateTime = 0;
  unsigned long confirmStart = 0;
  bool ultrasonicDetected = false;
  bool pirDetected = false;
  bool soundDetected = false;
  bool confirmed = false;
  bool sirenActive = false;
  unsigned long lastSirenToggle = 0;
  bool sirenToggleState = false;
  // PIR debounce state
  bool pirLastReading = false;
  bool pirDebounced = false;
  unsigned long pirDebounceTime = 0;
};

const ZoneConfig ZONE_CONFIGS[2] = {
  {2, 3, 4, A0, A2, A3},  // Set 1: PIR=2, US=3/4, Sound=A0, White=A2, Red=A3
  {5, 6, 7, A1, A4, A5}   // Set 2: PIR=5, US=6/7, Sound=A1, White=A4, Red=A5
};

ZoneState zoneStates[2];

const int BUZZER_PIN = 9;
const int SOUND_THRESH = 400;
const int US_THRESH = 80;
const unsigned long CONFIRM_WINDOW = 500;
const unsigned long ALARM_DELAY = 3000;
const unsigned long SIREN_PERIOD = 500;
const unsigned long US_TIMEOUT = 25000;
const unsigned long PIR_DEBOUNCE_MS = 50;

SoftwareSerial espSerial(8, 12); // RX=pin8 (ESP→Arduino), TX=pin12 (Arduino→ESP)
unsigned long buzzEnd = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  for (int i = 0; i < 2; i++) {
    const ZoneConfig &zc = ZONE_CONFIGS[i];
    pinMode(zc.pirPin, INPUT);
    pinMode(zc.trigPin, OUTPUT);
    pinMode(zc.echoPin, INPUT);
    pinMode(zc.soundPin, INPUT);
    pinMode(zc.whitePin, OUTPUT);
    pinMode(zc.redPin, OUTPUT);
    digitalWrite(zc.whitePin, LOW);
    digitalWrite(zc.redPin, LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("SENSOR FUSION SYSTEM INITIALIZED");
  Serial.println("ZONE1: PIR=2, US=3/4, SOUND=A0, WHT=A2, RED=A3");
  Serial.println("ZONE2: PIR=5, US=6/7, SOUND=A1, WHT=A4, RED=A5");
  Serial.println("============================================");
}

void loop() {
  unsigned long now = millis();

  // Poll all zones
  for (int i = 0; i < 2; i++) {
    processZone(i, now);
  }

  // Non-blocking buzzer
  if (digitalRead(BUZZER_PIN) && now - buzzEnd >= 200) {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Send JSON to ESP every 5s
  static unsigned long lastSend = 0;
  if (now - lastSend >= 5000) {
    lastSend = now;
    sendStatusToESP(now);
  }

  delay(50);
}

void readUltrasonic(int trig, int echo, long *dist) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long dur = pulseIn(echo, HIGH, US_TIMEOUT);
  *dist = (dur > 0) ? dur * 0.034 / 2 : 999;
}

void processZone(int idx, unsigned long now) {
  ZoneState &zs = zoneStates[idx];
  const ZoneConfig &zc = ZONE_CONFIGS[idx];

  long d;
  readUltrasonic(zc.trigPin, zc.echoPin, &d);
  int soundVal = analogRead(zc.soundPin);

  // PIR debounce (standard pattern)
  bool rawPir = digitalRead(zc.pirPin);
  if (rawPir != zs.pirLastReading) {
    zs.pirDebounceTime = now;
    zs.pirLastReading = rawPir;
  }
  if (now - zs.pirDebounceTime >= PIR_DEBOUNCE_MS) {
    zs.pirDebounced = rawPir;
  }
  zs.pirDetected = zs.pirDebounced;

  zs.ultrasonicDetected = (d > 0 && d < US_THRESH);
  zs.soundDetected = (soundVal > SOUND_THRESH);

  bool anyDetected = zs.ultrasonicDetected || zs.pirDetected || zs.soundDetected;

  // Reset if nothing detected for 1s while confirmed
  if (zs.confirmed && !anyDetected && now - zs.confirmStart > 1000) {
    zs.confirmed = false;
    zs.sirenActive = false;
    zs.candidateTime = 0;
    digitalWrite(zc.whitePin, LOW);
    digitalWrite(zc.redPin, LOW);
    Serial.print("ZONE "); Serial.print(idx); Serial.println(": CLEARED");
    return;
  }

  // Candidate: ultrasonic triggers, start confirmation window
  if (zs.ultrasonicDetected && zs.candidateTime == 0) {
    zs.candidateTime = now;
  }

  // Confirmation window logic
  if (zs.candidateTime && !zs.confirmed) {
    if (now - zs.candidateTime < CONFIRM_WINDOW) {
      if (zs.pirDetected || zs.soundDetected) {
        zs.confirmed = true;
        zs.confirmStart = now;
        zs.candidateTime = 0;
        digitalWrite(zc.whitePin, HIGH);
        digitalWrite(zc.redPin, LOW);
        triggerBuzzer();
        Serial.print("ZONE "); Serial.print(idx); Serial.println(": CONFIRMED");
      }
    } else {
      // Window expired without confirmation
      zs.candidateTime = 0;
    }
  }

  // Siren activation after 3s confirmed
  if (zs.confirmed && !zs.sirenActive && now - zs.confirmStart >= ALARM_DELAY) {
    zs.sirenActive = true;
    zs.lastSirenToggle = now;
    Serial.print("ZONE "); Serial.print(idx); Serial.println(": SIREN ACTIVATED");
  }

  // Siren blinking
  if (zs.sirenActive) {
    if (now - zs.lastSirenToggle >= SIREN_PERIOD) {
      zs.lastSirenToggle = now;
      zs.sirenToggleState = !zs.sirenToggleState;
      digitalWrite(zc.whitePin, zs.sirenToggleState);
      digitalWrite(zc.redPin, !zs.sirenToggleState);
    }
  }
}

void triggerBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzEnd = millis();
}

void sendStatusToESP(unsigned long now) {
  for (int i = 0; i < 2; i++) {
    const ZoneState &zs = zoneStates[i];
    String json = "{";
    json += "\"zone\":" + String(i) + ",";
    json += "\"ts\":" + String(now) + ",";
    json += "\"us\":" + String(zs.ultrasonicDetected ? 1 : 0) + ",";
    json += "\"pir\":" + String(zs.pirDetected ? 1 : 0) + ",";
    json += "\"snd\":" + String(zs.soundDetected ? 1 : 0) + ",";
    json += "\"conf\":" + String(zs.confirmed ? 1 : 0) + ",";
    json += "\"siren\":" + String(zs.sirenActive ? 1 : 0);
    json += "}";

    espSerial.println("BEGIN");
    espSerial.println(json);
    espSerial.println("END");

    Serial.print("Zone "); Serial.print(i); Serial.print(" JSON: ");
    Serial.println(json);
  }
}
