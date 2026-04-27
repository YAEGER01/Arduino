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

// ATM Variables
enum State { LOGIN, MENU, BALANCE, WITHDRAW };
State currentState = LOGIN;

String inputBuffer = "";
const String PIN = "1234"; 
long balance = 5000;

void setup() {
  Serial.begin(9600);
  showLoginScreen();
}

void loop() {
  char key = keypad.getKey();
  if (!key) return;

  // Global Actions
  if (key == '*') { logout(); return; }
  if (key == 'D') { deleteLast(); return; }

  switch (currentState) {
    case LOGIN:
      handleLogin(key);
      break;
    case MENU:
      handleMenu(key);
      break;
    case WITHDRAW:
      handleWithdraw(key);
      break;
    case BALANCE:
      if (key == '#') currentState = MENU; showMenu();
      break;
  }
}

// --- Logic Handlers ---

void handleLogin(char key) {
  if (key >= '0' && key <= '9' && inputBuffer.length() < 4) {
    inputBuffer += key;
    Serial.print("*"); // Masked PIN
  } else if (key == '#') {
    if (inputBuffer == PIN) {
      currentState = MENU;
      inputBuffer = "";
      showMenu();
    } else {
      Serial.println("\n[!] INVALID PIN. TRY AGAIN.");
      inputBuffer = "";
      showLoginScreen();
    }
  }
}

void handleMenu(char key) {
  if (key == '1') { currentState = BALANCE; showBalance(); }
  else if (key == '2') { currentState = WITHDRAW; showWithdrawScreen(); }
  else if (key == '3') logout();
}

void handleWithdraw(char key) {
  if (key >= '0' && key <= '9') {
    inputBuffer += key;
    Serial.print(key);
  } else if (key == '#') {
    long amount = inputBuffer.toInt();
    if (amount > 0 && amount <= balance) {
      balance -= amount;
      Serial.println("\n[SUCCESS] Dispensing P" + String(amount));
      Serial.println("New Balance: P" + String(balance));
    } else {
      Serial.println("\n[ERROR] Insufficient funds or invalid amount.");
    }
    inputBuffer = "";
    currentState = MENU;
    delay(2000);
    showMenu();
  }
}

// --- UI Screens (ASCII) ---

void showLoginScreen() {
  Serial.println("\n============================");
  Serial.println("|     SECURE ATM LOGIN     |");
  Serial.println("============================");
  Serial.print("ENTER 4-DIGIT PIN: ");
}

void showMenu() {
  Serial.println("\n============================");
  Serial.println("|        MAIN MENU         |");
  Serial.println("============================");
  Serial.println("| 1. Check Balance         |");
  Serial.println("| 2. Withdraw Cash         |");
  Serial.println("| 3. Logout                |");
  Serial.println("============================");
  Serial.print("Select Action: ");
}

void showBalance() {
  Serial.println("\n============================");
  Serial.println("|     ACCOUNT BALANCE      |");
  Serial.println("============================");
  Serial.println(" Current: P" + String(balance));
  Serial.println(" Press [#] to return Menu  ");
  Serial.println("============================");
}

void showWithdrawScreen() {
  inputBuffer = "";
  Serial.println("\n============================");
  Serial.println("|      WITHDRAW CASH       |");
  Serial.println("============================");
  Serial.print("Enter Amount & [#]: P");
}

void deleteLast() {
  if (inputBuffer.length() > 0) {
    inputBuffer.remove(inputBuffer.length() - 1);
    Serial.print("\b \b");
  }
}

void logout() {
  inputBuffer = "";
  currentState = LOGIN;
  Serial.println("\n[LOGGED OUT] THANK YOU.");
  delay(1000);
  showLoginScreen();
}