#define TRIG_PIN 7
#define ECHO_PIN 6
#define BUZZER_PIN 9

// Adjust this to test the max limit (Max for HC-SR04 is usually 400-500)
int maxThreshold = 400; 

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("Testing Max Range... Point sensor at an open space.");
}

void loop() {
  long duration;
  int distance;

  // Trigger the ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;

  // Print distance for debugging
  if (distance > 0) {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }

  // Detection Logic
  if (distance > 2 && distance < maxThreshold) {
    triggerThreeBeatBurst();
    
    // Split second cooldown (500ms) to prevent constant screaming
    Serial.println("Cooldown active...");
    delay(50); 
  }

  delay(60); // Small delay to prevent sensor interference
}

void triggerThreeBeatBurst() {
  Serial.println("!!! CHIME TRIGGERED !!!");
  
  for (int beat = 0; beat < 3; beat++) {
    // This loop creates a quick rising "chirp" or chime effect
    for (int freq = 800; freq < 1500; freq += 40) {
      tone(BUZZER_PIN, freq);
      delay(5); // Speed of the frequency sweep
    }
    
    noTone(BUZZER_PIN);
    delay(150); // Pause between each chime beat
  }
}