void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  int potValue = analogRead(A0);
  Serial.print("Potentiometer Value :  ");
  Serial.println(potValue);
  if (potValue > 512) {
    digitalWrite(13, HIGH);
    digitalWrite(12, LOW);
  } else { 
    digitalWrite(13, LOW);
    digitalWrite(12, HIGH);
  }
  delay(500); 
}
