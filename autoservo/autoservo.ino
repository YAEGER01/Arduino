#include <Servo.h>
Servo myServo;  
void setup() {
  myServo.attach(9);  
}
void loop() {
  myServo.write(0);   
  delay(1000);        
  myServo.write(90);  
  delay(1000);        
  myServo.write(170); 
  delay(1000);        
}