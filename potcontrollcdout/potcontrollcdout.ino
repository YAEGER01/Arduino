#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

// --- Keypad Setup ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Systems & States ---
enum State { MAIN_MENU, LED_MENU, LED_BRIGHTNESS, SERVO_INPUT };
State currentState = MAIN_MENU;

const int ledPin = 11;
int menuIndex = 0;
int lastMenuIndex = -1;
int lastPotVal = -1;
String inputBuffer = "";
bool ledState = false;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  
  myServo.attach(10);
  pinMode(ledPin, OUTPUT);
  myServo.write(0); 
  
  updateDisplay();
}

void loop() {
  char key = keypad.getKey();
  int rawPot = analogRead(A0);

  // 1. Menu Scrolling Logic (Inverted for CW focus)
  if (currentState == MAIN_MENU || currentState == LED_MENU) {
    // Map 1023 to 0 so CW turn goes from index 0 to 1
    int currentMenu = map(constrain(rawPot, 50, 970), 970, 50, 0, 1);
    if (currentMenu != lastMenuIndex) {
      menuIndex = currentMenu;
      updateDisplay();
      lastMenuIndex = menuIndex;
    }
  }

  // 2. Real-time Brightness Modulation (Low Latency)
  if (currentState == LED_BRIGHTNESS) {
    // CW turn (Low Analog) = High Brightness
    int brightness = map(rawPot, 1023, 0, 0, 255);
    analogWrite(ledPin, brightness);
    
    // Only update LCD if change is significant to prevent lag
    if (abs(rawPot - lastPotVal) > 10) {
      lcd.setCursor(7, 1);
      int pct = map(brightness, 0, 255, 0, 100);
      lcd.print(pct); lcd.print(F("%   "));
      lastPotVal = rawPot;
    }
  }

  // 3. Keypad Handling
  if (key) {
    handleKey(key);
  }
}

void updateDisplay() {
  // Clear Serial (ANSI)
  Serial.print("\033[2J\033[H");
  lcd.clear();
  
  switch (currentState) {
    case MAIN_MENU:
      lcd.print(F("1.LED   2.SERVO"));
      lcd.setCursor(0, 1);
      lcd.print(menuIndex == 0 ? F("> LED CONTROL") : F("> SERVO CONTROL"));
      break;

    case LED_MENU:
      lcd.print(F("LED OPTIONS:"));
      lcd.setCursor(0, 1);
      lcd.print(menuIndex == 0 ? F("> TOGGLE ON/OFF") : F("> DIMMER MODE"));
      break;

    case LED_BRIGHTNESS:
      lcd.print(F("DIMMING MODE"));
      lcd.setCursor(0, 1);
      lcd.print(F("LEVEL: "));
      break;

    case SERVO_INPUT:
      lcd.print(F("ANGLE (0-180):"));
      lcd.setCursor(0, 1);
      lcd.print(F("DEG: ")); lcd.print(inputBuffer);
      break;
  }
}

void handleKey(char key) {
  // Global Back Key
  if (key == '*') {
    if (currentState == LED_BRIGHTNESS) {
      // Revert to the last binary state when leaving dimmer
      digitalWrite(ledPin, ledState ? HIGH : LOW);
    }
    currentState = MAIN_MENU;
    inputBuffer = "";
    updateDisplay();
    return;
  }

  // Action Key
  if (key == '#') {
    if (currentState == MAIN_MENU) {
      currentState = (menuIndex == 0) ? LED_MENU : SERVO_INPUT;
    } 
    else if (currentState == LED_MENU) {
      if (menuIndex == 0) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState ? HIGH : LOW);
        lcd.setCursor(0, 1);
        lcd.print(ledState ? F("STATUS: ON ") : F("STATUS: OFF"));
        return; // Skip updateDisplay to keep status visible
      } else {
        currentState = LED_BRIGHTNESS;
      }
    }
    else if (currentState == SERVO_INPUT) {
      int angle = inputBuffer.toInt();
      if (inputBuffer.length() > 0 && angle <= 180) {
        myServo.write(angle);
        lcd.setCursor(0, 1);
        lcd.print(F("MOVING...      "));
      } else {
        lcd.setCursor(0, 1);
        lcd.print(F("INVALID! 0-180 "));
      }
      inputBuffer = ""; 
      // Delay briefly so user sees the message
      for(int i=0; i<500; i++) { delay(1); } 
    }
    updateDisplay();
  } 

  // Number Entry for Servo
  else if (isdigit(key) && currentState == SERVO_INPUT) {
    if (inputBuffer.length() < 3) {
      inputBuffer += key;
      lcd.setCursor(5, 1);
      lcd.print(inputBuffer);
    }
  }
  
  // Clear buffer if 'C' or 'B' is pressed (Optional utility)
  else if (key == 'C' && currentState == SERVO_INPUT) {
    inputBuffer = "";
    updateDisplay();
  }
}