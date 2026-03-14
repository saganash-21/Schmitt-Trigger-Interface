#include <Arduino.h>

// 1. Define your pins
const int optoPin = 4; // GPIO 4 connected to the 4N35
const int builtInLed = 2; // The ESP32's onboard blue LED
const int LEDdarkPin = 13;
const int LEDbrightPin = 14;

// 2. Volatile variables for the interrupt
volatile bool stateChanged = false;
volatile int currentSensorState = HIGH;

// 3. The Hardware Interrupt Function
void IRAM_ATTR handleOptoTrigger() {
  stateChanged = true; 
  // Instantly read if it went HIGH or LOW
  currentSensorState = digitalRead(optoPin); 
}

void setup() {
  Serial.begin(115200);
  
  pinMode(optoPin, INPUT_PULLUP); 
  pinMode(LEDbrightPin, OUTPUT); // Set the onboard LED as an output
  pinMode(LEDdarkPin, OUTPUT); // Set the onboard LED as an output
  
  // Trigger on CHANGE so we catch both the sunset AND the sunrise
  attachInterrupt(digitalPinToInterrupt(optoPin), handleOptoTrigger, CHANGE);
  
  Serial.println("Schmitt Trigger DAQ Online. Awaiting light changes...");
  
  // Make sure the LED starts in the off position
  currentSensorState = digitalRead(optoPin);
  if(currentSensorState == LOW){
    digitalWrite(LEDdarkPin, HIGH);
    digitalWrite(LEDbrightPin, LOW);
  }else{
    digitalWrite(LEDdarkPin, LOW);
    digitalWrite(LEDbrightPin, HIGH);
  
  }
}

void loop() {
  // 4. Handle the event only when the hardware flag is tripped
  if (stateChanged) {
    
    // Remember: LOW on the optoPin means the 741 triggered (it is DARK)
    if (currentSensorState == LOW) {
      digitalWrite(LEDdarkPin, HIGH);  // Turn ON the Dark LED
      digitalWrite(LEDbrightPin, LOW);  // Turn OFF the Light LED
      Serial.println("Event: DARKNESS DETECTED -> Dark LED ON");
    } 
    // HIGH means the 741 turned off (it is BRIGHT)
    else {
      digitalWrite(LEDdarkPin, LOW);   // Turn OFF the Dark LED
      digitalWrite(LEDbrightPin, HIGH); // Turn ON the Light LED
      Serial.println("Event: LIGHT DETECTED -> Light LED ON");
    }
    
    // Reset the flag to wait for the next event
    stateChanged = false; 
  }
}