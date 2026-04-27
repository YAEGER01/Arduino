#include <Keypad.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', '+'},
  {'4', '5', '6', '-'},
  {'7', '8', '9', '*'},
  {'C', '0', '=', '/'}5
};


byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String input = "";
float num1 = 0, num2 = 0;
char op = 0;

// Timing variables
unsigned long lastDisplayTime = 0;
const unsigned long clearDelay = 1000; 
bool resultDisplayed = false;          

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.print("Ready");
  lastDisplayTime = millis();
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    lcd.clear();
    lastDisplayTime = millis(); 
    resultDisplayed = false;    

    if (key >= '0' && key <= '9') {
      input += key;
      lcd.print(input);
    }
    else if (key == 'C') {
      
      input = "";
      num1 = num2 = 0;
      op = 0;
      lcd.print("Cleared");
    }
    else if (key == '+' || key == '-' || key == '*' || key == '/') {
      num1 = input.toFloat();
      op = key;
      input = "";
      lcd.setCursor(0,0);
      lcd.print(num1);
      lcd.print(op);
    }
    else if (key == '=') {
      num2 = input.toFloat();
      float result = 0;

      if (op == '+') result = num1 + num2;
      else if (op == '-') result = num1 - num2;
      else if (op == '*') result = num1 * num2;
      else if (op == '/') result = (num2 != 0) ? num1 / num2 : 0;

      lcd.print(result);
      input = String(result);

      resultDisplayed = true;   
      lastDisplayTime = millis();
    }
  }

  // Auto-clear + auto-reset only if a result was displayed
  if (resultDisplayed && (millis() - lastDisplayTime > clearDelay)) {
    lcd.clear();
    lcd.print("Calculator");   
    input = "";
    num1 = num2 = 0;
    op = 0;
    resultDisplayed = false;    
    lastDisplayTime = millis();
  }
}