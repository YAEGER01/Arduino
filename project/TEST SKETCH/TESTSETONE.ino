/*
 * Project S.I.G.A. - Dual Zone Controller (Adjustable Version)
 */

#include <SoftwareSerial.h>

// ==========================================
//        ADJUSTMENT PROFILE (TUNING)
// ==========================================
const int BLOCK_LIMIT = 60;         // [Pathway] Distance to trigger "Blocked" (cm)
const int SCAN_RANGE = 180;         // [Sensitivity] Max distance to care about people (cm)
const int STATIC_RANGE = 20;        // [Sensitivity] Move more than this to reset timer (cm)
const int SOUND_THRESHOLD = 500;    // [Sensitivity] Mic trigger level (0-1023)
const long STATIONARY_TIME = 10000; // [Time] How long until alarm triggers (ms)
const int PULSE_SPEED = 300;        // [Visual] Blink speed for Yellow/Red (ms)
const int ALARM_TONE = 1000;        // [Buzzer] Pitch of the final alarm (Hz)
const int PIR_LOCKOUT = 2000;       // [Time] Ignore PIR for x ms after trigger to stop loops
// ==========================================

// --- PIN DEFINITIONS ---
const int PIR1 = 2;
const int TRIG1 = 3;
const int ECHO1 = 4;
const int SOUND1 = A0;
const int WHITE1 = A2;
const int RED1 = A3;

const int PIR2 = 5;
const int TRIG2 = 6;
const int ECHO2 = 7;
const int SOUND2 = A1;
const int WHITE2 = A4;
const int RED2 = A5;

const int BUZZER = 9;
SoftwareSerial espSerial(10, 11); // RX, TX

// --- STATE VARIABLES ---
unsigned long lastMove1 = 0, lastMove2 = 0;
int anchor1 = 0, anchor2 = 0;
unsigned long pulseTimer = 0;
bool pulseState = false;

void setup()
{
  pinMode(PIR1, INPUT);
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(PIR2, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);
  pinMode(WHITE1, OUTPUT);
  pinMode(RED1, OUTPUT);
  pinMode(WHITE2, OUTPUT);
  pinMode(RED2, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  Serial.begin(9600);
  espSerial.begin(9600);
  Serial.println("S.I.G.A. DUAL-ZONE ONLINE");
}

void loop()
{
  // 1. DATA ACQUISITION
  int dist1 = getDistance(TRIG1, ECHO1);
  int dist2 = getDistance(TRIG2, ECHO2);
  int sVal1 = analogRead(SOUND1);
  int sVal2 = analogRead(SOUND2);

  bool S1 = (sVal1 > SOUND_THRESHOLD);
  bool S2 = (sVal2 > SOUND_THRESHOLD);
  bool P1 = digitalRead(PIR1);
  bool P2 = digitalRead(PIR2);
  bool W1 = (dist1 > BLOCK_LIMIT);
  bool W2 = (dist2 > BLOCK_LIMIT);

  // LED Timing
  if (millis() - pulseTimer > PULSE_SPEED)
  {
    pulseTimer = millis();
    pulseState = !pulseState;
  }

  // 2. DASHBOARD LOGGING (Clean View)
  printDashboard(dist1, dist2, sVal1, sVal2, P1, P2, W1, W2);

  // 3. PROCESSING
  processZone(1, S1, P1, W1, dist1, &anchor1, &lastMove1, WHITE1, RED1);
  processZone(2, S2, P2, W2, dist2, &anchor2, &lastMove2, WHITE2, RED2);

  // 4. NETWORKING
  espSerial.print(dist1);
  espSerial.print(",");
  espSerial.println(dist2);

  delay(50);
}

// --- CORE SYSTEM FUNCTIONS ---

void processZone(int id, bool S, bool P, bool W, int dist, int *anchor, unsigned long *timer, int wLed, int rLed)
{
  if (dist < SCAN_RANGE)
  {
    digitalWrite(wLed, HIGH);

    if (abs(dist - *anchor) > STATIC_RANGE)
    {
      *anchor = dist;
      *timer = millis();
    }

    if (S && P && W)
    { // 1-1-1
      visualWarning(wLed, rLed);
    }
    else if (!W && P)
    { // Blocked + Human
      if (S)
        visualAlarm(wLed, rLed);
      else
        visualWarning(wLed, rLed);

      if (millis() - *timer > STATIONARY_TIME)
      {
        triggerFinalAlarm();
        *timer = millis();
      }
    }
  }
  else
  {
    digitalWrite(wLed, LOW);
    digitalWrite(rLed, LOW);
    *timer = millis();
  }
}

int getDistance(int t, int e)
{
  digitalWrite(t, LOW);
  delayMicroseconds(2);
  digitalWrite(t, HIGH);
  delayMicroseconds(10);
  digitalWrite(t, LOW);
  long dur = pulseIn(e, HIGH, 20000);
  int d = dur * 0.034 / 2;
  // If reading is 0 or glitchy, return 200 (Clear)
  return (d <= 2 || d > 400) ? 200 : d;
}

void visualWarning(int w, int r)
{
  digitalWrite(w, pulseState);
  digitalWrite(r, pulseState);
}

void visualAlarm(int w, int r)
{
  digitalWrite(w, LOW);
  digitalWrite(r, pulseState);
}

void triggerFinalAlarm()
{
  Serial.println("\n>>> [CRITICAL] STATIONARY ENTITY DETECTED <<<");
  for (int i = 0; i < 10; i++)
  {
    tone(BUZZER, ALARM_TONE);
    delay(100);
    noTone(BUZZER);
    delay(100);
  }
}

void printDashboard(int d1, int d2, int s1, int s2, bool p1, bool p2, bool w1, bool w2)
{
  static unsigned long lastLog = 0;
  if (millis() - lastLog < 500)
    return; // Print every 0.5s to keep Serial readable
  lastLog = millis();

  Serial.print("Z1: ");
  Serial.print(d1);
  Serial.print("cm | ");
  Serial.print(p1 ? "P1 " : ".. ");
  Serial.print(s1 > SOUND_THRESHOLD ? "S1 " : ".. ");
  Serial.print(w1 ? "CLR" : "BLK");

  Serial.print("  ||  ");

  Serial.print("Z2: ");
  Serial.print(d2);
  Serial.print("cm | ");
  Serial.print(p2 ? "P2 " : ".. ");
  Serial.print(s2 > SOUND_THRESHOLD ? "S2 " : ".. ");
  Serial.print(w2 ? "CLR" : "BLK");
  Serial.println();
}