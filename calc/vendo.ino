#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String inputBuffer = "";

void setup() {
  Serial.begin(9600);
  displayMenu();
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    if (key == 'D') { // LIFO Delete
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        refreshDisplay();
      }
    } 
    else if (key == '*') { // Reset All
      inputBuffer = "";
      refreshDisplay();
    }
    else if (key == '#') { // Process
      if (inputBuffer.length() == 2) {
        dispenseItem(inputBuffer);
        inputBuffer = ""; // Clear for next
        delay(3000);      // Wait before showing menu again
        displayMenu();
      }
    }
    else if (inputBuffer.length() < 2) {
      inputBuffer += key;
      refreshDisplay();
    }
  }
}

void displayMenu() {
  Serial.println("\n==========================================");
  Serial.println("|         SNACKBOT VENDING MENU          |");
  Serial.println("==========================================");
  Serial.println("|  [A] SODAS (1-5)  |  [B] CHIPS (1-7)   |");
  Serial.println("|  1. Coke          |  1. Lays           |");
  Serial.println("|  2. Pepsi         |  2. Doritos        |");
  Serial.println("|  3. Sprite        |  3. Pringles       |");
  Serial.println("|  4. Root Beer     |  4. Cheetos        |");
  Serial.println("|  5. Mtn Dew       |  5. Ruffles        |");
  Serial.println("|                   |  6. Tostitos       |");
  Serial.println("|                   |  7. Sun Chips      |");
  Serial.println("------------------------------------------");
  Serial.println("|  [C] CANDY (1-9)                       |");
  Serial.println("|  1.Snickers 2.M&Ms   3.Skittles        |");
  Serial.println("|  4.Twix     5.KitKat 6.Reese's         |");
  Serial.println("|  7.MilkyWay 8.Hershey 9.SourPatch      |");
  Serial.println("==========================================");
  Serial.println("| [D] Delete | [#] Buy | [*] Clear All   |");
  Serial.println("==========================================");
  Serial.print("Selection: ");
}

void refreshDisplay() {
  // Clears the current line and shows updated input
  Serial.print("\rSelection: " + inputBuffer + "   ");
}

void dispenseItem(String code) {
  char cat = code[0];
  int idx = (code[1] - '0') - 1;

  Serial.println("\n\n>>> PROCESSING...");
  
  String output = "INVALID CODE";
  
  if (cat == 'A' && idx >= 0 && idx < 5) {
    String list[] = {"Coke", "Pepsi", "Sprite", "Root Beer", "Mtn Dew"};
    output = list[idx];
  } 
  else if (cat == 'B' && idx >= 0 && idx < 7) {
    String list[] = {"Lays", "Doritos", "Pringles", "Cheetos", "Ruffles", "Tostitos", "Sun Chips"};
    output = list[idx];
  } 
  else if (cat == 'C' && idx >= 0 && idx < 9) {
    String list[] = {"Snickers", "M&Ms", "Skittles", "Twix", "KitKat", "Reese's", "Milky Way", "Hershey's", "Sour Patch"};
    output = list[idx];
  }

  Serial.println("+----------------------------------------+");
  Serial.println("| DISPENSING: " + output);
  Serial.println("+----------------------------------------+");
}