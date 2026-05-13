#include <SoftwareSerial.h>

/**
 * ============================================================================
 * HALLWAY SENSOR ARRAY - v6.7 FIRMWARE
 * ============================================================================
 * PURPOSE: Detects presence, movement, and loitering in hallway using ultrasonic,
 *          PIR, and sound sensors. Triggers alarm if person stays > 5 seconds.
 * SENSORS: HC-SR04 (distance), PIR (motion), analog sound detector
 * OUTPUT:  Serial data to NodeMCU WiFi module via SoftwareSerial
 * ============================================================================
 */

/* ========== USER TUNING ZONE ==========
 * These parameters control sensitivity and timing. Adjust for specific environment.
 */

// PRESENCE DETECTION: Distance thresholds in centimeters
const int THRESH_IN = 30;  // Distance (cm) to register presence detected (person entered)
const int THRESH_OUT = 40; // Distance (cm) to register presence cleared (person left)

// MOVEMENT DETECTION: Distance change margins
const int MOVE_MARGIN = 60; // Minimum distance change (cm) to detect movement (walk-in vs walk-out)

// PIR SENSOR: Passive infrared motion sensor range
const int PIR_LIMIT_CM = 120; // PIR sensor detection range in cm (triggers walk-in when in range)

// TIMING: State and alarm thresholds
const long LOITER_MS = 5000;      // **ALARM TRIGGER**: Time (ms) person must stay still in LINGER state before LOITER/alarm
const long STATE_HOLD = 600;      // Minimum time (ms) to remain in a state before transition (debounce)
const long SLEEP_TIMEOUT = 30000; // Inactivity time (ms) before system enters SLEEP mode (30 seconds)

/* ========== SENSOR & LOGIC CONSTANTS ==========
 * These are optimized values that should not typically be changed.
 */
const int LOITER_RADIUS = 15;  // Distance tolerance (cm) for "loiter detection" - if person moves > this, break loiter state
const int TIME_STEP_MS = 3000; // Reserved for future adaptive timing (currently unused)
const int SENSE_SMOOTHING = 5; // Number of ultrasonic samples to average for noise reduction
const int STABILITY_MS = 150;  // Main loop delay (ms) - controls update frequency (lower = more responsive)
const int PIR_HOLD_MS = 2000;  // PIR debounce time (ms) - how long PIR signal is held after trigger

/* ========== PIN ASSIGNMENTS - WIRING TABLE ==========
 * Connect sensors/outputs to these Arduino pins
 * Verify wiring matches this pinout before powering on!
 */
const int PIR_PIN = 2;    // Digital input - PIR motion sensor (HIGH when motion detected)
const int TRIG_PIN = 3;   // Digital output - HC-SR04 ultrasonic TRIG (pulses to start measurement)
const int ECHO_PIN = 4;   // Digital input - HC-SR04 ultrasonic ECHO (pulse width = distance)
const int SOUND_PIN = A0; // Analog input - Sound detector module (0-1023 = quiet to loud)
const int WHITE_LED = A2; // Digital output - White LED (normal/ready indicator)
const int RED_LED = A3;   // Digital output - Red LED (alert/alarm indicator)
const int BUZZ_PIN = 9;   // PWM digital output - Buzzer for alarm and status sounds
const int SW_RX = 10;     // Software serial RX - receives data from NodeMCU (currently unused)
const int SW_TX = 11;     // Software serial TX - sends data TO NodeMCU WiFi module

// Initialize software serial for NodeMCU communication (9600 baud)
SoftwareSerial nodeMCU(SW_RX, SW_TX);

/* ========== STATE MACHINE DEFINITION ==========
 * System operates in 6 states representing different activity patterns:
 * CLEAR:    No activity detected - system idle, white LED on
 * WALK_IN:  Person detected moving into hallway - red LED blinking
 * WALK_OUT: Person detected moving out of hallway - red LED off
 * LINGER:   Person staying in place but not stationary - red LED steady
 * LOITER:   **ALARM STATE** - Person stayed in place > 5 sec - red LED fast flash + buzz
 * SLEEP:    No activity for 30 sec - low power state, white LED slow pulse
 */
enum SystemState
{
  CLEAR,    // Idle state
  WALK_IN,  // Moving INTO hallway
  WALK_OUT, // Moving OUT of hallway
  LINGER,   // Standing still, waiting for alarm
  LOITER,   // **ALARM** - Unauthorized loitering
  SLEEP     // No activity for 30 seconds
};
SystemState currentState = CLEAR; // Initialize to CLEAR (no activity)

/* ========== TIMING TRACKERS ==========
 * These variables track state transitions and activity history.
 */
unsigned long stateStartTime = 0; // Timestamp when current state was entered (used for elapsed time)
unsigned long lastChangeTime = 0; // Timestamp of last sensor activity (used for SLEEP timeout)
unsigned long lastPirTrigger = 0; // Timestamp when PIR sensor last triggered (used for debouncing)

/* ========== DISTANCE MEASUREMENT VARIABLES ==========
 * These track ultrasonic sensor readings and changes.
 */
long lastValidDist = 150;  // Last successful distance reading (cm) - start at 150cm
long lastChangeDist = 150; // Distance at last activity detection (cm) - used to detect changes
long anchorDist = 0;       // Distance when LINGER starts (cm) - used to detect if person moves away

/* ========== SENSOR STATE VARIABLES ==========
 * These represent current sensor inputs after processing.
 */
bool presence = false;     // TRUE if object detected closer than THRESH_IN (person present)
bool debouncedPIR = false; // TRUE if PIR signal recently triggered (within PIR_HOLD_MS)

/* ========== UTILITY FUNCTION: Convert State to Human-Readable String ==========
 * PURPOSE: Convert enum SystemState to string for logging and data transmission.
 * INPUT: SystemState enum value (CLEAR, WALK_IN, etc.)
 * OUTPUT: String representation of the state
 * USAGE: Used in Serial.print() and when sending data to NodeMCU
 */
String getStateName(SystemState state)
{
  switch (state)
  {
  case CLEAR:
    return "CLEAR"; // No activity
  case WALK_IN:
    return "WALK_IN"; // Person entering
  case WALK_OUT:
    return "WALK_OUT"; // Person leaving
  case LINGER:
    return "LINGER"; // Standing still
  case LOITER:
    return "LOITER (ALARM)"; // ALARM STATE - Loitering detected!
  case SLEEP:
    return "SLEEP"; // System dormant
  default:
    return "UNKNOWN"; // Error state
  }
}

/* ========== ALARM SOUND FUNCTION ==========
 * PURPOSE: Generate alarm tones when loitering is detected (LOITER state).
 * BEHAVIOR: Plays alternating high/low tones in rapid succession for 300ms total
 * FREQUENCY: 2000 Hz (high) for 50ms, 1500 Hz (low) for 50ms
 * Called repeatedly in LOITER state for continuous alarm
 */
void playAlarm()
{
  // Rapid fire alarm pattern for Loitering detection
  tone(BUZZ_PIN, 2000, 50); // High beep: 2000 Hz for 50ms
  delay(100);               // Wait 100ms between tones
  tone(BUZZ_PIN, 1500, 50); // Low beep: 1500 Hz for 50ms
}

/* ========== STATUS SOUND & LED FUNCTION ==========
 * PURPOSE: Play state-specific sounds and control LEDs based on system state
 * INPUT: SystemState - current state of the system
 * OUTPUTS:
 *   - SLEEP:    400 Hz low tone, all LEDs off (power saving mode)
 *   - CLEAR:    1000 Hz beep, WHITE LED on, RED LED off (normal idle)
 *   - WALK_IN:  1000 Hz tone (100ms), indicates movement detected
 * NOTE: This is called once per state change (not continuously)
 */
void playStatusSound(SystemState state)
{
  switch (state)
  {
  case SLEEP:
    // System entering sleep - quiet low tone, lights off
    tone(BUZZ_PIN, 400, 200);     // Low 400 Hz for 200ms (peaceful sleep mode)
    digitalWrite(WHITE_LED, LOW); // Turn off white LED (saving power)
    digitalWrite(RED_LED, LOW);   // Turn off red LED (saving power)
    break;

  case CLEAR:
    // System idle and ready - normal beep, white LED indicates ready
    noTone(BUZZ_PIN);              // Stop any previous tones
    tone(BUZZ_PIN, 1000, 50);      // Ready tone: 1000 Hz for 50ms
    digitalWrite(WHITE_LED, HIGH); // Turn on white LED (system active)
    digitalWrite(RED_LED, LOW);    // Red LED off (no alert)
    break;

  case WALK_IN:
    // Person detected entering - longer beep
    tone(BUZZ_PIN, 1000, 100); // Movement detected: 1000 Hz for 100ms
    break;
  }
}

/* ========== SETUP FUNCTION - INITIALIZATION ==========
 * PURPOSE: Initialize all pins, serial communication, and system startup.
 * CALLED: Once when Arduino powers on or resets
 * SETUP SEQUENCE:
 *   1. Initialize serial communication (debug output and NodeMCU)
 *   2. Configure all pins as INPUT or OUTPUT
 *   3. Play startup sound sequence (double beep)
 *   4. Print startup banner to Serial Monitor
 */
void setup()
{
  // Initialize serial communication
  Serial.begin(9600);  // Hardware serial to USB (debug/monitoring at 9600 baud)
  nodeMCU.begin(9600); // Software serial to NodeMCU WiFi module (9600 baud)

  // Configure all sensor inputs
  pinMode(PIR_PIN, INPUT);    // PIR sensor - digital input
  pinMode(TRIG_PIN, OUTPUT);  // Ultrasonic TRIG - digital output
  pinMode(ECHO_PIN, INPUT);   // Ultrasonic ECHO - digital input
  pinMode(BUZZ_PIN, OUTPUT);  // Buzzer - PWM output (tone function handles)
  pinMode(SOUND_PIN, INPUT);  // Sound sensor - analog input
  pinMode(WHITE_LED, OUTPUT); // White LED - digital output
  pinMode(RED_LED, OUTPUT);   // Red LED - digital output

  // Startup sequence - indicate system is ready
  digitalWrite(WHITE_LED, HIGH); // Turn on white LED
  tone(BUZZ_PIN, 1000, 50);      // First beep: 1000 Hz for 50ms
  delay(100);                    // Wait between beeps
  tone(BUZZ_PIN, 1000, 50);      // Second beep: 1000 Hz for 50ms (double beep = ready)

  // Print startup banner to Serial Monitor (USB debug)
  Serial.println(F("===================================="));
  Serial.println(F("   SYSTEM ACTIVE - ALARM ARMED (5s) "));
  Serial.println(F("===================================="));
  Serial.println(F("Waiting for movement..."));
}

/* ========== DISTANCE MEASUREMENT FUNCTION ==========
 * PURPOSE: Read ultrasonic distance sensor (HC-SR04) with noise filtering
 * METHOD: Takes multiple readings and averages valid results to reduce noise
 * FORMULA: Distance (cm) = pulse_duration (µs) × 0.034 / 2
 *          (Speed of sound is ~340 m/s = 0.034 cm/µs, divide by 2 for round-trip)
 * RETURNS: Average distance in cm, or lastValidDist if all readings fail
 * TIMING: Each reading takes ~50-100ms depending on distance
 *
 * HC-SR04 OPERATION:
 *   1. Send 10µs pulse to TRIG pin
 *   2. ECHO pin goes HIGH when sound sent, LOW when echo received
 *   3. Pulse width on ECHO = time for sound to travel to object and back
 *   4. Calculate distance from pulse duration
 */
long getFilteredDist()
{
  long sum = 0;  // Accumulator for distance samples
  int valid = 0; // Counter for valid (non-zero) readings

  // Take multiple samples and average them for noise filtering
  for (int i = 0; i < SENSE_SMOOTHING; i++) // SENSE_SMOOTHING = 5 samples
  {
    // Trigger pulse: LOW for 2µs, then HIGH for 10µs, then LOW
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2); // Ensure line is LOW
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10); // Send 10µs trigger pulse
    digitalWrite(TRIG_PIN, LOW);

    // Wait for ECHO pulse (timeout after 20ms if no response)
    long dur = pulseIn(ECHO_PIN, HIGH, 20000);

    // Only use valid readings (dur > 0)
    if (dur > 0)
    {
      // Convert pulse duration to distance: dur(µs) × 0.034 / 2 = distance(cm)
      sum += (dur * 0.034 / 2); // 0.034 = speed of sound in cm/µs; /2 for round-trip
      valid++;                  // Increment counter for valid samples
    }
  }

  // Return average if we got valid samples, otherwise use last good reading
  return (valid > 0) ? (sum / valid) : lastValidDist;
}

/* ========== MAIN STATE MACHINE LOGIC ==========
 * PURPOSE: Read all sensors, detect activity, and manage state transitions
 * CALLED: Every STABILITY_MS (150ms) in main loop
 * LOGIC FLOW:
 *   1. Read all sensor values (distance, sound, PIR)
 *   2. Apply debouncing and filtering
 *   3. Detect activity (movement, presence, sound)
 *   4. Evaluate state transitions based on current conditions
 *   5. Update LEDs and sounds
 *   6. Perform state transition if conditions are met
 */
void updateLogic()
{
  // ===== SENSOR READING =====
  long d = getFilteredDist();             // Get averaged ultrasonic distance (cm)
  int soundLevel = analogRead(SOUND_PIN); // Read sound sensor (0-1023, higher = louder)

  // ===== PIR DEBOUNCING =====
  // Update PIR trigger timestamp if motion detected
  if (digitalRead(PIR_PIN) == HIGH) // If PIR sensor outputs HIGH
    lastPirTrigger = millis();      // Update the trigger timestamp

  // Apply debounce: PIR is considered active for PIR_HOLD_MS after last trigger
  debouncedPIR = (millis() - lastPirTrigger < PIR_HOLD_MS);

  // ===== ACTIVITY DETECTION =====
  // Detect ANY significant change: distance, PIR, or sound
  if (abs(d - lastChangeDist) > 2 || // Distance changed >2cm, OR
      debouncedPIR ||                // PIR triggered recently, OR
      soundLevel > 600)              // Sound detected (threshold=600)
  {
    lastChangeDist = d;        // Update distance baseline
    lastChangeTime = millis(); // Update activity timestamp (used for SLEEP)

    // Wake up from SLEEP if any activity detected
    if (currentState == SLEEP)
    {
      currentState = CLEAR;      // Exit sleep mode
      playStatusSound(CLEAR);    // Play status sound and set LEDs
      stateStartTime = millis(); // Reset state timer
    }
  }

  // ===== PRESENCE DETECTION =====
  // Presence is TRUE if object detected closer than entry threshold
  presence = (d < THRESH_IN); // Is distance less than entry threshold (30cm)?

  // Initialize next state as current state (no transition by default)
  SystemState nextState = currentState;

  // Calculate elapsed time in current state (ms)
  unsigned long elapsed = millis() - stateStartTime;

  // ===== SLEEP TIMEOUT =====
  // If no activity for SLEEP_TIMEOUT (30 sec) in CLEAR state, enter SLEEP
  if (millis() - lastChangeTime > SLEEP_TIMEOUT && currentState == CLEAR)
  {
    nextState = SLEEP; // No activity for 30 seconds - enter sleep mode
  }

  // ===== STATE MACHINE SWITCH =====
  // For each state, define LED output, sounds, and transition conditions
  switch (currentState)
  {
  // ===== STATE: CLEAR (Idle, No Activity) =====
  // System ready, no movement or presence detected
  case CLEAR:
    digitalWrite(WHITE_LED, HIGH); // White LED on (system ready)
    digitalWrite(RED_LED, LOW);    // Red LED off (no alert)
    // Transition: Person enters OR PIR detects motion within range
    if (presence || (debouncedPIR && d < PIR_LIMIT_CM))
      nextState = WALK_IN; // Transition to WALK_IN
    break;

  // ===== STATE: WALK_IN (Person Entering) =====
  // Person detected moving into hallway (distance decreasing)
  case WALK_IN:
    // Red LED blinks at 2.5 Hz (400ms period: 200ms ON, 200ms OFF)
    digitalWrite(RED_LED, (millis() % 400 < 200));

    // Transition conditions:
    if (!presence && !debouncedPIR)                  // Person left OR no PIR trigger
      nextState = CLEAR;                             // Exit -> CLEAR
    else if ((lastValidDist - d) < -MOVE_MARGIN)     // Distance increased significantly (moving away)
      nextState = WALK_OUT;                          // Person is leaving -> WALK_OUT
    else if (abs(lastValidDist - d) < MOVE_MARGIN && // Distance stable (not moving much) AND
             elapsed > 1000)                         // Been in WALK_IN for >1 second
    {
      nextState = LINGER; // Person stopped moving -> LINGER
      anchorDist = d;     // Remember this distance as anchor point
    }
    break;

  // ===== STATE: WALK_OUT (Person Exiting) =====
  // Person detected moving out of hallway (distance increasing)
  case WALK_OUT:
    digitalWrite(RED_LED, LOW); // Red LED off
    // Transition conditions:
    if (!presence && elapsed > 1000)            // No presence AND been here >1 sec
      nextState = CLEAR;                        // Person fully exited -> CLEAR
    else if ((lastValidDist - d) > MOVE_MARGIN) // Distance decreased significantly again
      nextState = WALK_IN;                      // Changed direction -> back to WALK_IN
    break;

  // ===== STATE: LINGER (Person Stationary) =====
  // Person standing still - waiting to see if they stay too long
  case LINGER:
    digitalWrite(RED_LED, HIGH); // Red LED solid on (standing still)
    // Transition conditions:
    if (elapsed > LOITER_MS) // **ALARM TRIGGER** - Stayed >5 seconds!
    {
      nextState = LOITER; // Person loitering -> LOITER (ALARM!)
      Serial.println(F("[ALERT] Person stayed for 5s! TRIGGERING ALARM."));
    }
    else if (abs(d - anchorDist) > LOITER_RADIUS) // Person moved >15cm from anchor point
    {
      // Determine if movement was toward or away from sensor
      nextState = (d < anchorDist) ? WALK_IN : WALK_OUT; // Resume movement
    }
    break;

  // ===== STATE: LOITER (ALARM ACTIVE) =====
  // **ALARM STATE** - Unauthorized person detected loitering >5 seconds!
  case LOITER:
    // Red LED rapid flash (200ms period: 100ms ON, 100ms OFF)
    digitalWrite(RED_LED, (millis() % 200 < 100)); // Rapid flash alarm indicator
    playAlarm();                                   // Continuous alarm: 2000/1500 Hz tones
    // Transition conditions:
    if (!presence) // Person left the hallway
    {
      noTone(BUZZ_PIN);     // Stop alarm sound
      nextState = WALK_OUT; // Treat as leaving -> WALK_OUT
    }
    else if (abs(d - anchorDist) > LOITER_RADIUS) // Person moved significantly
    {
      noTone(BUZZ_PIN);                                  // Stop alarm sound
      nextState = (d < anchorDist) ? WALK_IN : WALK_OUT; // Resume normal tracking
    }
    break;

  // ===== STATE: SLEEP (No Activity >30 seconds) =====
  // System dormant to save power (minimal sensor reading)
  case SLEEP:
    // White LED slow pulse (2000ms period: 100ms ON, 1900ms OFF) = dim indicator
    digitalWrite(WHITE_LED, (millis() % 2000 < 100));
    digitalWrite(RED_LED, LOW); // Red LED off
    // Any activity detected above will transition to CLEAR automatically
    break;
  }

  // ===== STATE TRANSITION =====
  // Only allow state change if minimum hold time (debounce) has passed
  // This prevents rapid flickering between states due to sensor noise
  if (nextState != currentState && // State changed AND
      elapsed > STATE_HOLD)        // Held for at least STATE_HOLD (600ms)
  {
    // Log state change to Serial Monitor
    Serial.print(F("[EVENT] State Change: "));
    Serial.print(getStateName(currentState));
    Serial.print(F(" -> "));
    Serial.println(getStateName(nextState));

    // Perform transition
    currentState = nextState;      // Update state
    stateStartTime = millis();     // Reset state timer for elapsed calculation
    playStatusSound(currentState); // Play sound and set LEDs for new state
  }

  // Update last valid distance reading for next cycle
  lastValidDist = d;
}

void loop()
{
  // Run the main state machine logic (sensor reading + transitions)
  updateLogic();

  // ===== DATA TRANSMISSION SECTION =====
  // Send sensor data every 3 seconds (3000ms) to NodeMCU via SoftwareSerial
  static unsigned long lastSend = 0; // Static variable: retains value between loop iterations
  if (millis() - lastSend > 3000)    // If 3+ seconds since last transmission
  {
    lastSend = millis();                    // Update timestamp
    int soundLevel = analogRead(SOUND_PIN); // Read current sound level

    // ===== TRANSMIT TO NODECU (WiFi Module) =====
    // Format: SET1|STATE|DISTANCE|PIR|SOUND_DETECTED|SOUND_LEVEL|ALARM_ACTIVE
    // Example: SET1|LINGER|42|1|0|350|0
    nodeMCU.print("SET1|");                                // Sensor set identifier (SET1)
    nodeMCU.print(getStateName(currentState));             // Current state (CLEAR, WALK_IN, LOITER, etc.)
    nodeMCU.print("|");                                    // Delimiter
    nodeMCU.print(lastValidDist);                          // Distance reading (cm)
    nodeMCU.print("|");                                    // Delimiter
    nodeMCU.print(debouncedPIR ? "1" : "0");               // PIR status: 1=motion, 0=no motion
    nodeMCU.print("|");                                    // Delimiter
    nodeMCU.print(soundLevel > 600 ? "1" : "0");           // Sound detected: 1=loud, 0=quiet
    nodeMCU.print("|");                                    // Delimiter
    nodeMCU.print(soundLevel);                             // Raw sound level (0-1023)
    nodeMCU.print("|");                                    // Delimiter
    nodeMCU.println((currentState == LOITER) ? "1" : "0"); // Alarm active: 1=LOITER/ALARM, 0=no alarm

    // ===== DEBUG OUTPUT TO SERIAL MONITOR (USB) =====
    // For troubleshooting and monitoring via Arduino IDE Serial Monitor
    Serial.print(F("[DATA] State: "));
    Serial.print(getStateName(currentState)); // Current system state
    Serial.print(F(" | Dist: "));
    Serial.print(lastValidDist);
    Serial.print(F("cm"));
    Serial.print(F(" | PIR: "));
    Serial.print(debouncedPIR ? "HIGH" : "LOW ");
    Serial.print(F(" | Sound: "));
    Serial.println(soundLevel); // Raw sound level for debugging
  }

  // Main loop delay - controls update frequency of state machine
  // STABILITY_MS = 150ms = ~6.7 Hz update rate = responsive but stable
  delay(STABILITY_MS);
}