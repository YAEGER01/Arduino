int potPin = A0;   
int redPin = 9;    
int greenPin = 10; 
int bluePin = 11;  

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  Serial.begin(9600); // Required for Serial Monitor
}

void loop() {
  int val = analogRead(potPin); // Read 0-1023
  Serial.println(val);          // Display in Serial Monitor

  if (val >= 0 && val <= 341) {
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);
  } 
  else if (val >= 342 && val <= 682) {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, HIGH);
    digitalWrite(bluePin, LOW);
  } 
  else {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, HIGH);
  }
  
  delay(100); // Stability
}