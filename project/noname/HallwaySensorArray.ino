/*
 * HALLWAY SENSOR ARRAY - TEST VERSION (Set 1 Only)
 * Monitors left hallway zone for human presence
 */

// === SET 1 - Left Hallway Zone ===
const int PIR_PIN = 2;
const int TRIG_PIN = 3;
const int ECHO_PIN = 4;
const int SOUND_PIN = A0;
const int WHITE_LED_PIN = A2;
const int RED_LED_PIN = A3;

// === Central Buzzer ===
const int BUZZER_PIN = 9;

// === Editable Variables ===
// Duration in seconds before 2 chirps (default: 5)
// MIN: 1, MAX: 30, DEFAULT: 5
unsigned int WARNING_TIMEOUT = 5;

// Duration in seconds before alarm (default: 10)
// MIN: 5, MAX: 30, DEFAULT: 10
unsigned int ALARM_TIMEOUT = 10;

// Ultrasonic maximum detection distance in cm (default: 100)
// MIN: 20, MAX: 400, DEFAULT: 100
unsigned int MAX_DETECTION_DISTANCE = 100;

// Ultrasonic minimum detection distance in cm (default: 2)
// MIN: 2, MAX: 50, DEFAULT: 2
unsigned int MIN_DETECTION_DISTANCE = 2;

// === Sound sensor threshold (ADC value) ===
const int SOUND_THRESHOLD = 500;

// === Timing constants ===
const unsigned long CHIRP_DURATION = 100;
const unsigned long FLASH_DURATION = 2000;

// === State variables ===
enum SystemState {
  STATE_MONITOR,
  STATE_HUMAN_DETECTED,
  STATE_WARNING,
  STATE_ALARM
};

SystemState currentState = STATE_MONITOR;

// Timing
unsigned long stateStartTime = 0;
unsigned long detectionStartTime = 0;
unsigned long lastFlashTime = 0;

// Sensor states
bool humanPresent = false;
bool soundDetected = false;
bool pirDetected = false;
bool pirSoundConfirm = false;
float distance = 0;
float lastDistance = 0;

// Warning state tracking
bool warningChirp1Done = false;
bool warningChirp2Done = false;
unsigned long warningChirp1Time = 0;

// Alarm state tracking
bool alarmLedState = false;

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(WHITE_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  digitalWrite(WHITE_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.begin(9600);
  Serial.println("Hallway Sensor Array - Set 1 Test Mode Initialized");
  Serial.println("Waiting for PIR sensor warmup (30-60 seconds)...");
  delay(1000);
}

void loop() {
  readSensors();
  processStateMachine();
  delay(100);
}

void readSensors() {
  distance = readUltrasonic(TRIG_PIN, ECHO_PIN);
  pirDetected = (digitalRead(PIR_PIN) == HIGH);
  int soundValue = analogRead(SOUND_PIN);
  soundDetected = (soundValue > SOUND_THRESHOLD);
  
  bool distanceChange = detectDistanceChange(distance, lastDistance);
  bool pirSoundEdge = pirDetected && soundDetected && !pirSoundConfirm;
  pirSoundConfirm = pirDetected && soundDetected;
  
  humanPresent = distanceChange || pirSoundEdge;
  lastDistance = distance;
}

float readUltrasonic(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  float dist = duration * 0.034 / 2;
  if (duration == 0) dist = 0;
  return dist;
}

bool detectDistanceChange(float current, float last) {
  bool entered = (current > MIN_DETECTION_DISTANCE && current < MAX_DETECTION_DISTANCE && 
                  (last == 0 || last > MAX_DETECTION_DISTANCE));
  return entered;
}

void processStateMachine() {
  switch (currentState) {
    case STATE_MONITOR:
      handleMonitorState();
      break;
    case STATE_HUMAN_DETECTED:
      handleHumanDetectedState();
      break;
    case STATE_WARNING:
      handleWarningState();
      break;
    case STATE_ALARM:
      handleAlarmState();
      break;
  }
}

void handleMonitorState() {
  turnOffAll();
  
  if (humanPresent) {
    Serial.print("D:");
    Serial.print(distance, 1);
    Serial.print("cm PIR:");
    Serial.print(pirDetected ? 1 : 0);
    Serial.print(" S:");
    Serial.print(soundDetected ? 1 : 0);
    Serial.println(" -> HUMAN_DETECTED");
    currentState = STATE_HUMAN_DETECTED;
    stateStartTime = millis();
    detectionStartTime = millis();
    digitalWrite(WHITE_LED_PIN, HIGH);
    tone(BUZZER_PIN, 1000, 100);
  }
}

void handleHumanDetectedState() {
  unsigned long totalTime = millis() - detectionStartTime;
  
  Serial.print("State:HUMAN_DETECTED D:");
  Serial.print(distance, 1);
  Serial.print(" PIR:");
  Serial.print(pirDetected ? 1 : 0);
  Serial.print(" S:");
  Serial.print(soundDetected ? 1 : 0);
  Serial.print(" t:");
  Serial.println(totalTime / 1000.0, 1);
  
  if (!humanPresent) {
    // Reset confirm flags when human leaves
    pirSoundConfirm = false;
    turnOffAll();
    Serial.println("State:MONITOR (cleared)");
    currentState = STATE_MONITOR;
    return;
  }
  
  digitalWrite(WHITE_LED_PIN, HIGH);
  
  if (totalTime >= WARNING_TIMEOUT * 1000) {
    currentState = STATE_WARNING;
    stateStartTime = millis();
    warningChirp1Done = false;
    warningChirp2Done = false;
    Serial.println("State:WARNING");
  }
}

void handleWarningState() {
  unsigned long totalTime = millis() - detectionStartTime;
  unsigned long elapsed = millis() - stateStartTime;
  
  if (!humanPresent) {
    pirSoundConfirm = false;
    turnOffAll();
    currentState = STATE_MONITOR;
    warningChirp1Done = false;
    warningChirp2Done = false;
    Serial.println("State:MONITOR");
    return;
  }
  
  Serial.print("State:WARNING D:");
  Serial.print(distance, 1);
  Serial.print(" t:");
  Serial.println(totalTime / 1000.0, 1);
  
  if (totalTime >= ALARM_TIMEOUT * 1000) {
    currentState = STATE_ALARM;
    stateStartTime = millis();
    warningChirp1Done = false;
    warningChirp2Done = false;
    Serial.println("State:ALARM");
    return;
  }
  
  // Flash white LED for 2 seconds
  if (elapsed < FLASH_DURATION) {
    if (millis() - lastFlashTime > 250) {
      digitalWrite(WHITE_LED_PIN, !digitalRead(WHITE_LED_PIN));
      lastFlashTime = millis();
    }
  } else if (elapsed >= FLASH_DURATION && elapsed < FLASH_DURATION + 200 && !warningChirp1Done) {
    tone(BUZZER_PIN, 1000, 100);
    warningChirp1Done = true;
    warningChirp1Time = millis();
  } else if (warningChirp1Done && !warningChirp2Done && millis() - warningChirp1Time >= 200) {
    tone(BUZZER_PIN, 1000, 100);
    warningChirp2Done = true;
  }
}

void handleAlarmState() {
  unsigned long elapsed = millis() - stateStartTime;
  
  Serial.print("State:ALARM t:");
  Serial.println(elapsed / 1000.0, 1);
  
  // Flash both LEDs and sound alarm
  if (millis() - lastFlashTime > 100) {
    alarmLedState = !alarmLedState;
    digitalWrite(WHITE_LED_PIN, alarmLedState ? HIGH : LOW);
    digitalWrite(RED_LED_PIN, alarmLedState ? LOW : HIGH);
    lastFlashTime = millis();
  }
  tone(BUZZER_PIN, 1000);
  
  // Reset after alarm completes
  if (millis() - stateStartTime > 2000) {
    pirSoundConfirm = false;
    turnOffAll();
    alarmLedState = false;
    currentState = STATE_MONITOR;
    Serial.println("State:MONITOR (alarm reset)");
  }
}

void turnOffAll() {
  digitalWrite(WHITE_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  noTone(BUZZER_PIN);
}