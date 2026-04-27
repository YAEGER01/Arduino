void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Type 1 for Hello World");
}

void loop() {
  // put your main code here, to run repeatedly:
 if (Serial.available() > 0) {
  char input = Serial.read();

  if (input == '1') {
    Serial.println("Hello World");
  }
 }
}
