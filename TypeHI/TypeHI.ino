String cmd;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("");
  Serial.println("Type 'HI' to see Hello World");
}

void loop() {
  // put your main code here, to run repeatedly:
 if (Serial.available()) {
  cmd = Serial.readStringUntil('\n');

  cmd.trim();
  if (cmd == "HI") {
    Serial.println("Hello World");
  } else {
    Serial.println("Unknown Input");
      }
 }
}
