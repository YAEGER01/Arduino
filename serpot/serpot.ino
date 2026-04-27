#include <Servo.h>

Servo steeringRack;
const int potPin = A0;
const int upBtn = 2;
const int downBtn = 3;
const int ledG = 10, ledY = 11, ledR = 12;

int gear = 1;
float rpm = 0;
unsigned long lastUpdate = 0;

void setup() {
  steeringRack.attach(9);
  pinMode(upBtn, INPUT);   // Use external 10k resistor to GND
  pinMode(downBtn, INPUT); // Use external 10k resistor to GND
  pinMode(ledG, OUTPUT); pinMode(ledY, OUTPUT); pinMode(ledR, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  int potVal = analogRead(potPin);
  
  // 1. Steering (Active regardless of gear)
  steeringRack.write(map(potVal, 0, 1023, 0, 180));

  // 2. RPM Logic (Speed depends on Pot center-offset)
  // Straighter wheel = Faster RPM climb
  int steeringStrain = abs(potVal - 512);
  float rpmGain = map(steeringStrain, 0, 512, 15, 2) / 10.0; 
  
  if (gear < 8) rpm += rpmGain; 
  if (rpm > 100) rpm = 100; // Redline hit

  // 3. Manual Shifting
  if (digitalRead(upBtn) == HIGH) {
    if (gear < 8 && rpm > 70) { // Can only upshift at high RPM
      gear++;
      rpm = 30; // RPM drops after shift
      Serial.print("UPSHIFT: Gear "); Serial.println(gear);
      delay(250); // Debounce
    }
  }

  if (digitalRead(downBtn) == HIGH) {
    if (gear > 1) {
      gear--;
      rpm = 80; // RPM spikes after downshift
      Serial.print("DOWNSHIFT: Gear "); Serial.println(gear);
      delay(250); // Debounce
    }
  }

  // 4. LED Output
  digitalWrite(ledG, (rpm > 30));
  digitalWrite(ledY, (rpm > 60));
  digitalWrite(ledR, (rpm > 90));

  delay(20);
}