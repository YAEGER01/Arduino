const int buttonPin = 2;
int lastState = HIGH;
int buttonState;
int pressCount = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  buttonState = digitalRead(buttonPin);

  if (lastState == HIGH && buttonState == LOW) {
    pressCount++;
    Serial.print("Count: ");
    digitalWrite(13, HIGH);
    Serial.println(pressCount);
    delay(200);
  }
lastState = buttonState;
}
