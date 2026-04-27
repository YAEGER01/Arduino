#include <Servo.h>
Servo myServo;        
const int buttonPin = 2; 
int pressCount = 0;   
int lastButtonState = LOW; 
void setup() {
  Serial.begin(9600);
  myServo.attach(9);  
  pinMode(buttonPin, INPUT); 
  myServo.write(0); 
}
void loop() {
  int buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH && lastButtonState == LOW) {
    pressCount++; 
    if (pressCount == 1) {
      myServo.write(0);
      Serial.println(" 1st Press 0°");
    } 
    else if (pressCount == 2) {
      myServo.write(90);
      Serial.println(" 2nd Press 90°");
    } 
    else if (pressCount == 3) {
      myServo.write(180);
      Serial.println(" 3rd Press 180°");
    } 
    else {
      pressCount = 0; // reset
      myServo.write(0);
      Serial.println("Next Press 0°");
    }
    delay(300); 
  }
  lastButtonState = buttonState;
}