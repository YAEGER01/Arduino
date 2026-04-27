String cmd;
void setup() {
  pinMode(13, OUTPUT);
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("");
  Serial.println("Type 'ON' LED ON or 'OFF' to LED OFF");
}

void loop() {
  // put your main code here, to run repeatedly:
 if (Serial.available()) {
  cmd = Serial.readStringUntil('\n');

  cmd.trim();
  if (cmd == "ON") {
    digitalWrite(13, HIGH);
    Serial.println("LED ON");
  } else if (cmd == "OFF") {
    digitalWrite(13, LOW);            
    Serial.println("LED OFF");
      } else {
        Serial.println("Unknown Command");
      }

 }
}
