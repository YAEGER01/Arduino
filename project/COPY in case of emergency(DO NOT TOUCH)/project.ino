#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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

/* --- PINOUT --- */
const int PIR_PIN = 2, TRIG_PIN = 3, ECHO_PIN = 4, BUZZ_PIN = 9; 
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* --- UI ICONS --- */
byte charHeart[8]   = {0x0,0xA,0x1F,0x1F,0xE,0x4,0x0,0x0};   
byte charArrowUp[8] = {0x4,0xE,0x1F,0x4,0x4,0x4,0x4,0x0};    
byte charArrowDn[8] = {0x4,0x4,0x4,0x4,0x1F,0xE,0x4,0x0};    

/* --- SYSTEM VARIABLES --- */
enum SystemState { CLEAR, WALK_IN, WALK_OUT, LINGER, LOITER, SLEEP };
SystemState currentState = CLEAR;

unsigned long stateStartTime = 0;
unsigned long lastChangeTime = 0;
unsigned long lastPirTrigger = 0; 
long lastValidDist = 150;
long lastChangeDist = 150;
bool presence = false;
bool debouncedPIR = false;
bool isLCDOn = true;
int trendBuffer = 0;
int loiterRepetitionCount = 0; 

/* --- AUDIO ENGINE --- */
void playStatusSound(SystemState state) {
  switch (state) {
    case SLEEP: // Subtle power-down blip
      tone(BUZZ_PIN, 800, 50); delay(100); tone(BUZZ_PIN, 400, 100);
      break;
    case CLEAR: 
      for(int i = 0; i < 4; i++) { tone(BUZZ_PIN, 1600 + (i * 100), 30); delay(50); }
      break;
    case WALK_IN: 
      for (int i = 900; i < 1300; i += 80) { tone(BUZZ_PIN, i, 15); delay(15); }
      break;
    case WALK_OUT: 
      for (int i = 1300; i > 900; i -= 80) { tone(BUZZ_PIN, i, 15); delay(15); }
      break;
    case LOITER: 
      tone(BUZZ_PIN, 880, 50); delay(100); tone(BUZZ_PIN, 880, 50);
      break;
    case LINGER: 
      tone(BUZZ_PIN, 1100, 20); delay(40); tone(BUZZ_PIN, 1100, 20);
      break;
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin(); Wire.setClock(400000);
  pinMode(PIR_PIN, INPUT); pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  lcd.init(); lcd.backlight();
  lcd.createChar(0, charHeart); lcd.createChar(1, charArrowUp); lcd.createChar(2, charArrowDn);

  lcd.setCursor(0, 0); lcd.print("SEC-CORE v8.0");
  lcd.setCursor(0, 1); lcd.print("SLEEP ENABLED");
  
  tone(BUZZ_PIN, 1000, 100); delay(150); tone(BUZZ_PIN, 1200, 100);
  delay(1000); lcd.clear();
}

long getFilteredDist() {
  long sum = 0; int valid = 0;
  for(int i = 0; i < SENSE_SMOOTHING; i++) {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long dur = pulseIn(ECHO_PIN, HIGH, 20000);
    if (dur > 0) { sum += (dur * 0.034 / 2); valid++; }
    delayMicroseconds(500); 
  }
  return (valid > 0) ? (sum / valid) : lastValidDist;
}

void updateLogic() {
  long d = getFilteredDist();
  
  if (digitalRead(PIR_PIN) == HIGH) lastPirTrigger = millis();
  debouncedPIR = (millis() - lastPirTrigger < PIR_HOLD_MS);

  // ACTIVITY CHECK: If distance changes or PIR triggers, reset the activity timer
  if (abs(d - lastChangeDist) > 1 || debouncedPIR) {
    lastChangeDist = d;
    lastChangeTime = millis();
    
    // Wake up if sleeping
    if (currentState == SLEEP) {
      currentState = CLEAR;
      lcd.backlight();
      isLCDOn = true;
      playStatusSound(CLEAR);
      stateStartTime = millis();
    }
  }

  if (d < THRESH_IN) presence = true;
  else if (d > THRESH_OUT) presence = false;

  SystemState nextState = currentState;
  unsigned long elapsed = millis() - stateStartTime;

  // Auto-Sleep trigger
  if (millis() - lastChangeTime > SLEEP_TIMEOUT && currentState != SLEEP && currentState == CLEAR) {
    nextState = SLEEP;
  }

  switch (currentState) {
    case SLEEP: 
      // Handled by activity check wake-up
      break;

    case CLEAR:
      if (presence || (debouncedPIR && d < PIR_LIMIT_CM)) nextState = WALK_IN;
      break;

    case WALK_IN:
      if (!presence && !debouncedPIR) nextState = CLEAR;
      else if ((lastValidDist - d) < -MOVE_MARGIN) {
        trendBuffer--; if (trendBuffer <= -2) nextState = WALK_OUT;
      } 
      else if (abs(lastValidDist - d) < MOVE_MARGIN && elapsed > 1000) {
        nextState = LINGER; anchorDist = d; trendBuffer = 0;
      }
      break;

    case WALK_OUT:
      if ((lastValidDist - d) > MOVE_MARGIN) {
        trendBuffer++; if (trendBuffer >= 2) nextState = WALK_IN;
      } else if (!presence && elapsed > 1000) nextState = CLEAR;
      break;

    case LINGER:
      if (abs(d - anchorDist) > LOITER_RADIUS) {
        if (d < anchorDist) nextState = WALK_IN; else nextState = WALK_OUT;
      }
      else if (elapsed > LOITER_MS) nextState = LOITER;
      else if (!presence) nextState = WALK_OUT;
      break;

    case LOITER:
      if (!presence) nextState = WALK_OUT;
      else if (abs(d - anchorDist) > LOITER_RADIUS) {
        if (d < anchorDist) nextState = WALK_IN; else nextState = WALK_OUT;
      }
      else {
        long nextTrigger = LOITER_MS + (loiterRepetitionCount * (loiterRepetitionCount + 1) / 2 * TIME_STEP_MS);
        if (elapsed > nextTrigger) {
          loiterRepetitionCount++;
          playStatusSound(LOITER); 
        }
      }
      break;
  }

  // Watchdog reset (In case sensors get stuck but PIR is dead)
  if (millis() - lastChangeTime > 15000 && !debouncedPIR && currentState != SLEEP && presence) {
    nextState = CLEAR;
  }

  if (nextState != currentState && elapsed > STATE_HOLD) {
    currentState = nextState;
    stateStartTime = millis();
    trendBuffer = 0; 
    loiterRepetitionCount = 0;
    
    if (currentState == SLEEP) {
      lcd.noBacklight();
      isLCDOn = false;
    }
    playStatusSound(currentState); 
  }
  lastValidDist = d;
}

void updateLCD() {
  if (!isLCDOn && currentState == SLEEP) return;

  static unsigned long lastLCD = 0;
  if (millis() - lastLCD < 200) return; 
  lastLCD = millis();

  lcd.setCursor(0, 0);
  lcd.print("ST: ");
  switch (currentState) {
    case SLEEP:    lcd.print("SLEEPING  "); break;
    case CLEAR:    lcd.print("READY     "); break;
    case WALK_IN:  lcd.print("APPRCH  "); lcd.write(1); lcd.print(" "); break;
    case WALK_OUT: lcd.print("DEPART  "); lcd.write(2); lcd.print(" "); break;
    case LINGER:   lcd.print("LINGER    "); break;
    case LOITER:   lcd.print("STATIC+"); lcd.print(loiterRepetitionCount); lcd.print("  "); break;
  }

  lcd.setCursor(0, 1);
  lcd.print("RNG:");
  if(lastValidDist < 100) lcd.print(" ");
  if(lastValidDist < 10)  lcd.print(" ");
  lcd.print(lastValidDist); lcd.print("cm "); 

  lcd.setCursor(11, 1);
  lcd.print(debouncedPIR ? "[P]" : "[ ]");
  
  lcd.setCursor(15, 1);
  if(abs(lastValidDist - lastChangeDist) > MOVE_MARGIN) lcd.write(0);
  else lcd.print(" ");
}

void loop() {
  updateLogic();
  updateLCD();

  // Send data to ESP via Serial every 5 seconds
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();
    Serial.print("{");
    Serial.print("\"timestamp\":"); Serial.print(millis()); Serial.print(",");
    Serial.print("\"state\":\""); Serial.print(stateToString(currentState)); Serial.print("\",");
    Serial.print("\"distance\":"); Serial.print(lastValidDist); Serial.print(",");
    Serial.print("\"pir\":"); Serial.print(debouncedPIR ? "true" : "false"); Serial.print(",");
    Serial.print("\"presence\":"); Serial.print(presence ? "true" : "false"); Serial.print(",");
    Serial.print("\"last_change_time\":"); Serial.print(lastChangeTime); Serial.print(",");
    Serial.print("\"last_change_dist\":"); Serial.print(lastChangeDist); Serial.print(",");
    Serial.print("\"trend_buffer\":"); Serial.print(trendBuffer); Serial.print(",");
    Serial.print("\"loiter_count\":"); Serial.print(loiterRepetitionCount); Serial.print(",");
    Serial.print("\"last_pir_trigger\":"); Serial.print(lastPirTrigger); Serial.print(",");
    Serial.print("\"lcd_on\":"); Serial.print(isLCDOn ? "true" : "false");
    Serial.println("}");
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