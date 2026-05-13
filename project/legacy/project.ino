

/* --- USER TUNING ZONE --- */
const int THRESH_IN     = 50;  
const int THRESH_OUT    = 65;  
const int MOVE_MARGIN   = 5;   
const int PIR_LIMIT_CM  = 120; 
const long LOITER_MS    = 5000;
const long STATE_HOLD   = 600; 

// --- NEW SLEEP TUNER ---
const long SLEEP_TIMEOUT = 30000; // 30 seconds of total inactivity = Sleep

// --- ANCHOR & PROGRESSIVE TUNERS ---
const int LOITER_RADIUS   = 15;   
const int TIME_STEP_MS    = 3000; 
long anchorDist = 0;             

// --- SENSITIVITY ADJUSTMENTS ---
const int SENSE_SMOOTHING = 5;   
const int STABILITY_MS    = 150; 
const int PIR_HOLD_MS     = 2000; 
/* -------------------------- */

#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11); // RX=10, TX=11

/* --- PINOUT --- */
const int PIR_PIN1 = 2, TRIG_PIN1 = 3, ECHO_PIN1 = 4, BUZZ_PIN1 = 9, SOUND_PIN1 = A0;
const int PIR_PIN2 = 5, TRIG_PIN2 = 6, ECHO_PIN2 = 7, BUZZ_PIN2 = 12, SOUND_PIN2 = A1;

    

/* --- SYSTEM VARIABLES --- */
enum SystemState { CLEAR, WALK_IN, WALK_OUT, LINGER, LOITER, SLEEP };

// Set 1
SystemState currentState1 = CLEAR;
unsigned long stateStartTime1 = 0;
unsigned long lastChangeTime1 = 0;
unsigned long lastPirTrigger1 = 0;
long lastValidDist1 = 150;
long lastChangeDist1 = 150;
bool presence1 = false;
bool debouncedPIR1 = false;
int trendBuffer1 = 0;
int loiterRepetitionCount1 = 0;
int soundLevel1 = 0;

// Set 2
SystemState currentState2 = CLEAR;
unsigned long stateStartTime2 = 0;
unsigned long lastChangeTime2 = 0;
unsigned long lastPirTrigger2 = 0;
long lastValidDist2 = 150;
long lastChangeDist2 = 150;
bool presence2 = false;
bool debouncedPIR2 = false;
int trendBuffer2 = 0;
int loiterRepetitionCount2 = 0;
int soundLevel2 = 0;

// Shared
long anchorDist1 = 0;
long anchorDist2 = 0; 

/* --- AUDIO ENGINE --- */
void playStatusSound(SystemState state, int buzzPin) {
  switch (state) {
    case SLEEP: // Subtle power-down blip
      tone(buzzPin, 800, 50); delay(100); tone(buzzPin, 400, 100);
      break;
    case CLEAR:
      for(int i = 0; i < 4; i++) { tone(buzzPin, 1600 + (i * 100), 30); delay(50); }
      break;
    case WALK_IN:
      for (int i = 900; i < 1300; i += 80) { tone(buzzPin, i, 15); delay(15); }
      break;
    case WALK_OUT:
      for (int i = 1300; i > 900; i -= 80) { tone(buzzPin, i, 15); delay(15); }
      break;
    case LOITER:
      tone(buzzPin, 880, 50); delay(100); tone(buzzPin, 880, 50);
      break;
    case LINGER:
      tone(buzzPin, 1100, 20); delay(40); tone(buzzPin, 1100, 20);
      break;
  }
}

void setup() {
  mySerial.begin(9600);
  // Set 1
  pinMode(PIR_PIN1, INPUT); pinMode(TRIG_PIN1, OUTPUT); pinMode(ECHO_PIN1, INPUT);
  pinMode(BUZZ_PIN1, OUTPUT);
  // Set 2
  pinMode(PIR_PIN2, INPUT); pinMode(TRIG_PIN2, OUTPUT); pinMode(ECHO_PIN2, INPUT);
  pinMode(BUZZ_PIN2, OUTPUT);
  // Sound sensors are analog, no pinMode needed
  
  tone(BUZZ_PIN1, 1000, 100); delay(150); tone(BUZZ_PIN1, 1200, 100);
  tone(BUZZ_PIN2, 1000, 100); delay(150); tone(BUZZ_PIN2, 1200, 100);
  delay(1000);
}

long getFilteredDist(int trigPin, int echoPin, long lastValidDist) {
  long sum = 0; int valid = 0;
  for(int i = 0; i < SENSE_SMOOTHING; i++) {
    digitalWrite(trigPin, LOW); delayMicroseconds(2);
    digitalWrite(trigPin, HIGH); delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long dur = pulseIn(echoPin, HIGH, 20000);
    if (dur > 0) { sum += (dur * 0.034 / 2); valid++; }
    delayMicroseconds(500);
  }
  return (valid > 0) ? (sum / valid) : lastValidDist;
}

void updateLogic1() {
  long d = getFilteredDist(TRIG_PIN1, ECHO_PIN1, lastValidDist1);
  soundLevel1 = analogRead(SOUND_PIN1);

  if (digitalRead(PIR_PIN1) == HIGH) lastPirTrigger1 = millis();
  debouncedPIR1 = (millis() - lastPirTrigger1 < PIR_HOLD_MS);

  // ACTIVITY CHECK: If distance changes or PIR triggers, reset the activity timer
  if (abs(d - lastChangeDist1) > 1 || debouncedPIR1) {
    lastChangeDist1 = d;
    lastChangeTime1 = millis();

    // Wake up if sleeping
    if (currentState1 == SLEEP) {
      currentState1 = CLEAR;
      playStatusSound(CLEAR, BUZZ_PIN1);
      stateStartTime1 = millis();
    }
  }

  if (d < THRESH_IN) presence1 = true;
  else if (d > THRESH_OUT) presence1 = false;

  SystemState nextState = currentState1;
  unsigned long elapsed = millis() - stateStartTime1;

  // Auto-Sleep trigger
  if (millis() - lastChangeTime1 > SLEEP_TIMEOUT && currentState1 != SLEEP && currentState1 == CLEAR) {
    nextState = SLEEP;
  }

  switch (currentState1) {
    case SLEEP:
      // Handled by activity check wake-up
      break;

    case CLEAR:
      if (presence1 || (debouncedPIR1 && d < PIR_LIMIT_CM)) nextState = WALK_IN;
      break;

    case WALK_IN:
      if (!presence1 && !debouncedPIR1) nextState = CLEAR;
      else if ((lastValidDist1 - d) < -MOVE_MARGIN) {
        trendBuffer1--; if (trendBuffer1 <= -2) nextState = WALK_OUT;
      }
      else if (abs(lastValidDist1 - d) < MOVE_MARGIN && elapsed > 1000) {
        nextState = LINGER; long anchorDist = d; trendBuffer1 = 0;
      }
      break;

    case WALK_OUT:
      if ((lastValidDist1 - d) > MOVE_MARGIN) {
        trendBuffer1++; if (trendBuffer1 >= 2) nextState = WALK_IN;
      } else if (!presence1 && elapsed > 1000) nextState = CLEAR;
      break;

    case LINGER:
      if (abs(d - anchorDist) > LOITER_RADIUS) {
        if (d < anchorDist) nextState = WALK_IN; else nextState = WALK_OUT;
      }
      else if (elapsed > LOITER_MS) nextState = LOITER;
      else if (!presence1) nextState = WALK_OUT;
      break;

    case LOITER:
      if (!presence1) nextState = WALK_OUT;
      else if (abs(d - anchorDist) > LOITER_RADIUS) {
        if (d < anchorDist) nextState = WALK_IN; else nextState = WALK_OUT;
      }
      else {
        long nextTrigger = LOITER_MS + (loiterRepetitionCount1 * (loiterRepetitionCount1 + 1) / 2 * TIME_STEP_MS);
        if (elapsed > nextTrigger) {
          loiterRepetitionCount1++;
          playStatusSound(LOITER, BUZZ_PIN1);
        }
      }
      break;
  }

  // Watchdog reset (In case sensors get stuck but PIR is dead)
  if (millis() - lastChangeTime1 > 15000 && !debouncedPIR1 && currentState1 != SLEEP && presence1) {
    nextState = CLEAR;
  }

  if (nextState != currentState1 && elapsed > STATE_HOLD) {
    currentState1 = nextState;
    stateStartTime1 = millis();
    trendBuffer1 = 0;
    loiterRepetitionCount1 = 0;


    playStatusSound(currentState1, BUZZ_PIN1);
  }
  lastValidDist1 = d;
}

void updateLogic2() {
  long d = getFilteredDist(TRIG_PIN2, ECHO_PIN2, lastValidDist2);
  soundLevel2 = analogRead(SOUND_PIN2);

  if (digitalRead(PIR_PIN2) == HIGH) lastPirTrigger2 = millis();
  debouncedPIR2 = (millis() - lastPirTrigger2 < PIR_HOLD_MS);

  // ACTIVITY CHECK: If distance changes or PIR triggers, reset the activity timer
  if (abs(d - lastChangeDist2) > 1 || debouncedPIR2) {
    lastChangeDist2 = d;
    lastChangeTime2 = millis();

    // Wake up if sleeping
    if (currentState2 == SLEEP) {
      currentState2 = CLEAR;
      // No LCD backlight for set2
      playStatusSound(CLEAR, BUZZ_PIN2);
      stateStartTime2 = millis();
    }
  }

  if (d < THRESH_IN) presence2 = true;
  else if (d > THRESH_OUT) presence2 = false;

  SystemState nextState = currentState2;
  unsigned long elapsed = millis() - stateStartTime2;

  // Auto-Sleep trigger
  if (millis() - lastChangeTime2 > SLEEP_TIMEOUT && currentState2 != SLEEP && currentState2 == CLEAR) {
    nextState = SLEEP;
  }

  switch (currentState2) {
    case SLEEP:
      // Handled by activity check wake-up
      break;

    case CLEAR:
      if (presence2 || (debouncedPIR2 && d < PIR_LIMIT_CM)) nextState = WALK_IN;
      break;

    case WALK_IN:
      if (!presence2 && !debouncedPIR2) nextState = CLEAR;
      else if ((lastValidDist2 - d) < -MOVE_MARGIN) {
        trendBuffer2--; if (trendBuffer2 <= -2) nextState = WALK_OUT;
      }
      else if (abs(lastValidDist2 - d) < MOVE_MARGIN && elapsed > 1000) {
        nextState = LINGER; long anchorDist = d; trendBuffer2 = 0;
      }
      break;

    case WALK_OUT:
      if ((lastValidDist2 - d) > MOVE_MARGIN) {
        trendBuffer2++; if (trendBuffer2 >= 2) nextState = WALK_IN;
      } else if (!presence2 && elapsed > 1000) nextState = CLEAR;
      break;

    case LINGER:
      if (abs(d - anchorDist) > LOITER_RADIUS) {
        if (d < anchorDist) nextState = WALK_IN; else nextState = WALK_OUT;
      }
      else if (elapsed > LOITER_MS) nextState = LOITER;
      else if (!presence2) nextState = WALK_OUT;
      break;

    case LOITER:
      if (!presence2) nextState = WALK_OUT;
      else if (abs(d - anchorDist) > LOITER_RADIUS) {
        if (d < anchorDist) nextState = WALK_IN; else nextState = WALK_OUT;
      }
      else {
        long nextTrigger = LOITER_MS + (loiterRepetitionCount2 * (loiterRepetitionCount2 + 1) / 2 * TIME_STEP_MS);
        if (elapsed > nextTrigger) {
          loiterRepetitionCount2++;
          playStatusSound(LOITER, BUZZ_PIN2);
        }
      }
      break;
  }

  // Watchdog reset (In case sensors get stuck but PIR is dead)
  if (millis() - lastChangeTime2 > 15000 && !debouncedPIR2 && currentState2 != SLEEP && presence2) {
    nextState = CLEAR;
  }

  if (nextState != currentState2 && elapsed > STATE_HOLD) {
    currentState2 = nextState;
    stateStartTime2 = millis();
    trendBuffer2 = 0;
    loiterRepetitionCount2 = 0;

    playStatusSound(currentState2, BUZZ_PIN2);
  }
  lastValidDist2 = d;
}



void loop() {
  updateLogic1();
  updateLogic2();

  // Send data to ESP via SoftwareSerial every 5 seconds
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();
    // Send set 1
    mySerial.print("{");
    mySerial.print("\"set\":\"1\",");
    mySerial.print("\"timestamp\":"); mySerial.print(millis()); mySerial.print(",");
    mySerial.print("\"state\":\""); mySerial.print(stateToString(currentState1)); mySerial.print("\",");
    mySerial.print("\"distance\":"); mySerial.print(lastValidDist1); mySerial.print(",");
    mySerial.print("\"pir\":"); mySerial.print(debouncedPIR1 ? "true" : "false"); mySerial.print(",");
    mySerial.print("\"presence\":"); mySerial.print(presence1 ? "true" : "false"); mySerial.print(",");
    mySerial.print("\"last_change_time\":"); mySerial.print(lastChangeTime1); mySerial.print(",");
    mySerial.print("\"last_change_dist\":"); mySerial.print(lastChangeDist1); mySerial.print(",");
    mySerial.print("\"trend_buffer\":"); mySerial.print(trendBuffer1); mySerial.print(",");
    mySerial.print("\"loiter_count\":"); mySerial.print(loiterRepetitionCount1); mySerial.print(",");
    mySerial.print("\"last_pir_trigger\":"); mySerial.print(lastPirTrigger1); mySerial.print(",");
    mySerial.print("\"sound_level\":"); mySerial.print(soundLevel1); mySerial.print(",");
    mySerial.print("\"anchor_dist\":"); mySerial.print(anchorDist1);
    mySerial.println("}");
    // Send set 2
    mySerial.print("{");
    mySerial.print("\"set\":\"2\",");
    mySerial.print("\"timestamp\":"); mySerial.print(millis()); mySerial.print(",");
    mySerial.print("\"state\":\""); mySerial.print(stateToString(currentState2)); mySerial.print("\",");
    mySerial.print("\"distance\":"); mySerial.print(lastValidDist2); mySerial.print(",");
    mySerial.print("\"pir\":"); mySerial.print(debouncedPIR2 ? "true" : "false"); mySerial.print(",");
    mySerial.print("\"presence\":"); mySerial.print(presence2 ? "true" : "false"); mySerial.print(",");
    mySerial.print("\"last_change_time\":"); mySerial.print(lastChangeTime2); mySerial.print(",");
    mySerial.print("\"last_change_dist\":"); mySerial.print(lastChangeDist2); mySerial.print(",");
    mySerial.print("\"trend_buffer\":"); mySerial.print(trendBuffer2); mySerial.print(",");
    mySerial.print("\"loiter_count\":"); mySerial.print(loiterRepetitionCount2); mySerial.print(",");
    mySerial.print("\"last_pir_trigger\":"); mySerial.print(lastPirTrigger2); mySerial.print(",");
    mySerial.print("\"sound_level\":"); mySerial.print(soundLevel2); mySerial.print(",");
    mySerial.print("\"anchor_dist\":"); mySerial.print(anchorDist2);
    mySerial.println("}");
  }

  delay(STABILITY_MS);
}

// Helper function to convert state to string
String stateToString(SystemState state) {
  switch (state) {
    case CLEAR: return "CLEAR";
    case WALK_IN: return "WALK_IN";
    case WALK_OUT: return "WALK_OUT";
    case LINGER: return "LINGER";
    case LOITER: return "LOITER";
    case SLEEP: return "SLEEP";
    default: return "UNKNOWN";
  }
}