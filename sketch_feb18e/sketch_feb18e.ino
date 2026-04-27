const int btn = 2;
const int led1 = 8, led2 = 9, led3 = 10;
int state = 0;
bool last = LOW;

void setup() {
  pinMode(btn, INPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
}

void loop() {
  bool current = digitalRead(btn);
  if (current && !last) {
    digitalWrite(led1, state == 0);
    digitalWrite(led2, state == 1);
    digitalWrite(led3, state == 2);
    state = (state + 1) % 3;
    delay(200);
  }
  last = current;
}