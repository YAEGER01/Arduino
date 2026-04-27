#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', '+'},
  {'4', '5', '6', '-'},
  {'7', '8', '9', '*'},
  {'C', '0', '=', '/'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

long firstNum = 0;
long secondNum = 0;
char op = ' ';
bool isSecondNum = false;
bool waitingForChoice = false;

void setup() {
  Serial.begin(9600);
  Serial.println("Calculator Ready. Enter first number:");
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    // Stage 1: Choice after calculation
    if (waitingForChoice) {
      if (key == '1') { // Continue
        secondNum = 0;
        isSecondNum = true;
        waitingForChoice = false;
        Serial.print("Continuing with: ");
        Serial.println(firstNum);
        Serial.println("Select next operator (+, -, *, /)");
      } 
      else if (key == '0' || key == 'C') { // Reset
        firstNum = 0;
        secondNum = 0;
        op = ' ';
        isSecondNum = false;
        waitingForChoice = false;
        Serial.println("--- Reset ---");
        Serial.println("Enter first number:");
      }
      return; 
    }

    // Stage 2: Input and Operation
    if (key >= '0' && key <= '9') {
      if (!isSecondNum) {
        firstNum = (firstNum * 10) + (key - '0');
        Serial.println(firstNum);
      } else {
        secondNum = (secondNum * 10) + (key - '0');
        Serial.println(secondNum);
      }
    } 
    else if (key == '+' || key == '-' || key == '*' || key == '/') {
      if (isSecondNum && secondNum != 0) {
          // Allows chaining without pressing '=' if desired
          performCalculation(); 
      }
      op = key;
      isSecondNum = true;
      Serial.print("Operator: ");
      Serial.println(op);
    } 
    else if (key == '=') {
      performCalculation();
      waitingForChoice = true;
      Serial.println("Continue? (1=Yes / 0=Reset)");
    } 
    else if (key == 'C') {
      resetCalc();
    }
  }
}

void performCalculation() {
  long result = 0;
  if (op == '+') result = firstNum + secondNum;
  else if (op == '-') result = firstNum - secondNum;
  else if (op == '*') result = firstNum * secondNum;
  else if (op == '/') result = (secondNum != 0) ? firstNum / secondNum : 0;

  Serial.print("Result: ");
  Serial.println(result);
  firstNum = result; 
}

void resetCalc() {
  firstNum = 0;
  secondNum = 0;
  op = ' ';
  isSecondNum = false;
  waitingForChoice = false;
  Serial.println("Cleared. Enter first number:");
}