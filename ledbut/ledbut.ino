int buttonPin = 2;
int ledPin = 8;./m

void setup() {
  // put your setup code here, to run once:
pinMode(buttonPin, INPUT);
pinMode(ledPin, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
int buttonState = digitalRead(buttonPin);

if (buttonState == HIGH) {
  digitalWrite(ledPin, HIGH);

} else {
  digitalWrite(ledPin,LOW);

}


}
