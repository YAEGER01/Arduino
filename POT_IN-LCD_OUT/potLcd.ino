#include <Wire.h> 
#include <LiquidCrystal_I2C.h> 

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2); 

const int potPin = A0; // Potentiometer connected to Analog Pin 0

void setup() { 
  lcd.init(); 
  lcd.backlight(); 
  lcd.setCursor(0, 0); 
  lcd.print("Value:"); 
} 

void loop() { 
  int sensorValue = analogRead(potPin); // Read the value (0-1023)
  
  lcd.setCursor(0, 1); 
  lcd.print(sensorValue); 
  
  // Clear trailing digits when value drops from 1000 to 999
  lcd.print("    "); 
  
  delay(500); // Small delay for stability
}