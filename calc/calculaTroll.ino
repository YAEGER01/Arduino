#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'}, // A = +
  {'4', '5', '6', 'B'}, // B = -
  {'7', '8', '9', 'C'}, // C = *
  {'*', '0', '#', 'D'}  // * = Clear, # = Equal, D = Delete
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variables
long num1 = 0, num2 = 0, result = 0;
char op = ' ';
String pinBuffer = "";
const String MASTER_PIN = "696969"; 

enum State { GET_NUM1, GET_OP, GET_NUM2, LOCKED };
State systemState = GET_NUM1;

void setup() {
  Serial.begin(9600);
  printHeader("STANDARD CALCULATOR");
  Serial.print("Input: ");
}

void loop() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == '*') { resetSystem(); return; } // Global Clear

  switch (systemState) {
    case GET_NUM1:
      if (isDigit(key)) {
        num1 = (num1 * 10) + (key - '0');
        Serial.print(key);
      } else if (key >= 'A' && key <= 'C') {
        op = (key == 'A') ? '+' : (key == 'B') ? '-' : '*';
        systemState = GET_NUM2;
        Serial.print(" "); Serial.print(op); Serial.print(" ");
      }
      break;

    case GET_NUM2:
      if (isDigit(key)) {
        num2 = (num2 * 10) + (key - '0');
        Serial.print(key);
      } else if (key == '#') {
        calculate();
        systemState = LOCKED;
        showLockScreen();
      }
      break;

    case LOCKED:
      if (isDigit(key) && pinBuffer.length() < 6) {
        pinBuffer += key;
        Serial.print("*");
      } else if (key == 'D') { // LIFO DELETE
        if (pinBuffer.length() > 0) {
          pinBuffer.remove(pinBuffer.length() - 1);
          Serial.print("\b \b");
        }
      } else if (key == '#') {
        verifyPin();
      }
      break;
  }
}

void calculate() {
  if (op == '+') result = num1 + num2;
  else if (op == '-') result = num1 - num2;
  else if (op == '*') result = num1 * num2;
}

void verifyPin() {
  if (pinBuffer == MASTER_PIN) {
    Serial.println("\n\n[!] ACCESS GRANTED [!]");
    Serial.println("============================");
    Serial.print("  FINAL RESULT: "); Serial.println(result);
    Serial.println("============================");
    delay(5000);
    resetSystem();
  } else {
    Serial.println("\n\n[X] WRONG PIN - DATA PURGED");
    Serial.println("      (Nice try, bud)       ");
    delay(2000);
    resetSystem();
  }
}

void showLockScreen() {
  Serial.println("\n\n****************************");
  Serial.println("* ENCRYPTION ENABLED    *");
  Serial.println("* ENTER 6-DIGIT AUTH CODE *");
  Serial.println("****************************");
  Serial.print("PIN: ");
}

void resetSystem() {
  num1 = 0; num2 = 0; result = 0; op = ' '; pinBuffer = "";
  systemState = GET_NUM1;
  Serial.println("\n\n\n\n");
  printHeader("STANDARD CALCULATOR");
  Serial.print("Input: ");
}

void printHeader(String title) {
  Serial.println("----------------------------");
  Serial.println("| " + title + " |");
  Serial.println("----------------------------");
}