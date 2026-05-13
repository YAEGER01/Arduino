#include <SoftwareSerial.h>

/* --- USER TUNING ZONE --- */
const int THRESH_IN = 30;
const int THRESH_OUT = 40;
const int MOVE_MARGIN = 60;
const int PIR_LIMIT_CM = 120;
const long LOITER_MS = 5000; // Alarm triggers after this threshold (5 seconds)
const long STATE_HOLD = 600;
const long SLEEP_TIMEOUT = 30000;

/* --- SENSOR & LOGIC CONSTANTS --- */
const int LOITER_RADIUS = 15;
const int TIME_STEP_MS = 3000;
const int SENSE_SMOOTHING = 5;
const int STABILITY_MS = 150;
const int PIR_HOLD_MS = 2000;

/* --- PINOUT PER WIRING TABLE --- */
const int PIR_PIN = 2;
const int TRIG_PIN = 3;
const int ECHO_PIN = 4;
const int SOUND_PIN = A0;
const int WHITE_LED = A2;
const int RED_LED = A3;
const int BUZZ_PIN = 9;
const int SW_RX = 10;
const int SW_TX = 11;

SoftwareSerial nodeMCU(SW_RX, SW_TX);

/* --- SYSTEM VARIABLES --- */
enum SystemState
{
  CLEAR,
  WALK_IN,
  WALK_OUT,
  LINGER,
  LOITER,
  SLEEP
};
SystemState currentState = CLEAR;

unsigned long stateStartTime = 0;
unsigned long lastChangeTime = 0;
unsigned long lastPirTrigger = 0;
long lastValidDist = 150;
long lastChangeDist = 150;
long anchorDist = 0;
bool presence = false;
bool debouncedPIR = false;

/* --- LOGGING HELPER --- */
String getStateName(SystemState state)
{
  switch (state)
  {
  case CLEAR:
    return "CLEAR";
  case WALK_IN:
    return "WALK_IN";
  case WALK_OUT:
    return "WALK_OUT";
  case LINGER:
    return "LINGER";
  case LOITER:
    return "LOITER (ALARM)";
  case SLEEP:
    return "SLEEP";
  default:
    return "UNKNOWN";
  }
}

/* --- ALARM & AUDIO FEEDBACK --- */
void playAlarm()
{
  // Rapid fire alarm for Loitering
  tone(BUZZ_PIN, 2000, 50);
  delay(100);
  tone(BUZZ_PIN, 1500, 50);
}

void playStatusSound(SystemState state)
{
  switch (state)
  {
  case SLEEP:
    tone(BUZZ_PIN, 400, 200);
    digitalWrite(WHITE_LED, LOW);
    digitalWrite(RED_LED, LOW);
    break;
  case CLEAR:
    noTone(BUZZ_PIN);
    tone(BUZZ_PIN, 1000, 50);
    digitalWrite(WHITE_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    break;
  case WALK_IN:
    tone(BUZZ_PIN, 1000, 100);
    break;
  }
}

void setup()
{
  Serial.begin(9600);
  nodeMCU.begin(9600);

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(WHITE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(WHITE_LED, HIGH);
  tone(BUZZ_PIN, 1000, 50);
  delay(100);
  tone(BUZZ_PIN, 1000, 50);

  Serial.println(F("===================================="));
  Serial.println(F("   SYSTEM ACTIVE - ALARM ARMED (5s) "));
  Serial.println(F("===================================="));
}

long getFilteredDist()
{
  long sum = 0;
  int valid = 0;
  for (int i = 0; i < SENSE_SMOOTHING; i++)
  {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long dur = pulseIn(ECHO_PIN, HIGH, 20000);
    if (dur > 0)
    {
      sum += (dur * 0.034 / 2);
      valid++;
    }
  }
  return (valid > 0) ? (sum / valid) : lastValidDist;
}

void updateLogic()
{
  long d = getFilteredDist();
  int soundLevel = analogRead(SOUND_PIN);

  if (digitalRead(PIR_PIN) == HIGH)
    lastPirTrigger = millis();

  debouncedPIR = (millis() - lastPirTrigger < PIR_HOLD_MS);

  if (abs(d - lastChangeDist) > 2 || debouncedPIR || soundLevel > 600)
  {
    lastChangeDist = d;
    lastChangeTime = millis();

    if (currentState == SLEEP)
    {
      currentState = CLEAR;
      playStatusSound(CLEAR);
      stateStartTime = millis();
    }
  }

  presence = (d < THRESH_IN);
  SystemState nextState = currentState;
  unsigned long elapsed = millis() - stateStartTime;

  if (millis() - lastChangeTime > SLEEP_TIMEOUT && currentState == CLEAR)
  {
    nextState = SLEEP;
  }

  switch (currentState)
  {
  case CLEAR:
    digitalWrite(WHITE_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    if (presence || (debouncedPIR && d < PIR_LIMIT_CM))
      nextState = WALK_IN;
    break;

  case WALK_IN:
    digitalWrite(RED_LED, (millis() % 400 < 200));
    if (!presence && !debouncedPIR)
      nextState = CLEAR;
    else if ((lastValidDist - d) < -MOVE_MARGIN)
      nextState = WALK_OUT;
    else if (abs(lastValidDist - d) < MOVE_MARGIN && elapsed > 1000)
    {
      nextState = LINGER;
      anchorDist = d;
    }
    break;

  case WALK_OUT:
    digitalWrite(RED_LED, LOW);
    if (!presence && elapsed > 1000)
      nextState = CLEAR;
    else if ((lastValidDist - d) > MOVE_MARGIN)
      nextState = WALK_IN;
    break;

  case LINGER:
    digitalWrite(RED_LED, HIGH);
    if (elapsed > LOITER_MS)
    {
      nextState = LOITER;
      Serial.println(F("[ALERT] Person stayed for 5s! TRIGGERING ALARM."));
    }
    else if (abs(d - anchorDist) > LOITER_RADIUS)
      nextState = (d < anchorDist) ? WALK_IN : WALK_OUT;
    break;

  case LOITER:
    digitalWrite(RED_LED, (millis() % 200 < 100)); // Rapid flash Red LED
    playAlarm();                                   // Continuous audible alarm
    if (!presence)
    {
      noTone(BUZZ_PIN);
      nextState = WALK_OUT;
    }
    else if (abs(d - anchorDist) > LOITER_RADIUS)
    {
      noTone(BUZZ_PIN);
      nextState = (d < anchorDist) ? WALK_IN : WALK_OUT;
    }
    break;

  case SLEEP:
    digitalWrite(WHITE_LED, (millis() % 2000 < 100));
    digitalWrite(RED_LED, LOW);
    break;
  }

  if (nextState != currentState && elapsed > STATE_HOLD)
  {
    Serial.print(F("[EVENT] State Change: "));
    Serial.print(getStateName(currentState));
    Serial.print(F(" -> "));
    Serial.println(getStateName(nextState));

    currentState = nextState;
    stateStartTime = millis();
    playStatusSound(currentState);
  }
  lastValidDist = d;
}

void loop()
{
  updateLogic();

  static unsigned long lastSend = 0;
  if (millis() - lastSend > 3000)
  {
    lastSend = millis();
    int soundLevel = analogRead(SOUND_PIN);

    nodeMCU.print("SET1|");
    nodeMCU.print(getStateName(currentState));
    nodeMCU.print("|");
    nodeMCU.print(lastValidDist);
    nodeMCU.print("|");
    nodeMCU.print(debouncedPIR ? "1" : "0");
    nodeMCU.print("|");
    nodeMCU.print(soundLevel > 600 ? "1" : "0");
    nodeMCU.print("|");
    nodeMCU.print(soundLevel);
    nodeMCU.print("|");
    nodeMCU.println((currentState == LOITER) ? "1" : "0");

    Serial.print(F("[DATA] State: "));
    Serial.print(getStateName(currentState));
    Serial.print(F(" | Dist: "));
    Serial.print(lastValidDist);
    Serial.print(F("cm"));
    Serial.print(F(" | PIR: "));
    Serial.print(debouncedPIR ? "HIGH" : "LOW ");
    Serial.print(F(" | Sound: "));
    Serial.println(soundLevel);
  }

  delay(STABILITY_MS);
}