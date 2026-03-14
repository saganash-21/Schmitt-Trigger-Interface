# Schmitt-Trigger-Interface
# Hardware-Level Signal Debouncing and Galvanic Isolation using a 741 Schmitt Trigger

## 1. Executive Summary & Objective
Environmental analog sensors—such as Light Dependent Resistors (LDRs)—produce inherently noisy and fluctuating voltage signals. When interfacing these raw analog signals directly with digital microcontrollers, minor environmental changes near the logic threshold cause signal "chatter" (rapid, erratic switching). Furthermore, operating analog sensor arrays at higher voltages introduces a critical risk of overvoltage damage to modern 3.3V logic controllers.

The objective of this project is to design an isolated, zero-latency analog signal conditioning front-end. This system utilizes a UA741CPE4 Op-Amp configured as a Schmitt Trigger to provide hardware-level debouncing via hysteresis. The clean logic state is then passed across a galvanic barrier using a 4N35 optocoupler to safely trigger a non-blocking hardware interrupt on an ESP32, completely offloading the signal processing from the microcontroller.

## 2. System Power Architecture
To bridge the analog and digital domains, the system requires strict power segregation:
* **The Power Source:** The system is driven by a standard 5V, 2A adapter feeding into a breadboard power module.
* **The Analog Island (8.5V):** To satisfy the common-mode voltage requirements of the 741 Op-Amp, a boost converter steps the 5V input up to an 8.5V single-supply rail. *(Note: For clarity, this is abstracted simply as VCC in the schematic).*
* **The Digital Island (3.3V):** The ESP32 operates entirely on its independent 3.3V logic, sharing only a common ground with the analog circuitry.

## 3. Analog-to-Digital Signal Conditioning & Hysteresis
Instead of relying on a microcontroller to continuously sample an analog pin and calculate software debouncing, the debouncing is handled entirely in hardware.

The baseline reference voltage ($V_{ref}$) is established using a simple voltage divider (R2 and R3):
$$V_{ref} = 8.5\text{V} \times \frac{10\text{k}\Omega}{10\text{k}\Omega + 10\text{k}\Omega} = 4.25\text{V}$$

A 100k$\Omega$ positive feedback resistor routes the Op-Amp's output back to the non-inverting input. Because the 741's output swings between roughly 1.7V (LOW) and 7V (HIGH), this feedback actively shifts the 4.25V reference point.
* When the output is HIGH, it pulls the threshold slightly up ($V_{upper}$).
* When the output is LOW, it pulls the threshold slightly down ($V_{lower}$).

This dynamic threshold shift creates a rigid hysteresis window. The sensor voltage must cross the entire gap to trigger a state change, effectively eliminating all sensor chatter.

## 4. Hardware Protection & A/D Bridging
A 7V high state from the Op-Amp would instantly destroy the ESP32's 3.3V GPIO pins. To safely bridge the Analog-to-Digital (A/D) gap, a 4N35 optocoupler is utilized. The analog circuit simply powers the internal infrared LED of the optocoupler, while the internal phototransistor acts as an isolated switch on the ESP32's 3.3V logic side. This guarantees complete hardware protection through galvanic isolation.

## 5. Hardware Debugging & Leakage Mitigation
During testing, a critical real-world limitation of the older bipolar UA741CP architecture was identified. The chip does not swing rail-to-rail; its LOW state bottoms out at approximately 1.7V.

This 1.7V base level was enough to partially bias the optocoupler's internal LED, creating a 1.02V residual leakage that prevented the digital logic from ever registering a definitive "OFF" state.

**The Solution:** Instead of modifying the logic thresholds, a hardware-level voltage drop was engineered. Two 1N4004 standard rectifier diodes were placed in series between the Op-Amp output and the 4N35. Since each diode introduces a roughly 0.7V forward voltage drop, the combined 1.4V drop successfully crushed the 1.7V leakage down to ~0.3V, ensuring a hard, reliable "OFF" state for the optocoupler without impacting the 7V "HIGH" state.

## 6. Firmware Optimization & Latency
Because the signal is perfectly debounced in hardware, the ESP32 is freed from resource-heavy polling loops.

The firmware utilizes an Interrupt Service Routine (ISR) attached to the optocoupler's GPIO pin. The processor remains at 0% load regarding the environmental sensor until a physical `CHANGE` edge violently triggers the interrupt. This event-driven architecture guarantees near-zero latency execution when a lighting change occurs, leaving the dual-core processor entirely free for complex IoT communication or mechatronic actuation.

**Visual Validation:** To prove the zero-latency routing and the absence of overlapping states, two external LEDs were integrated into the digital island (GPIO 13 and GPIO 14). The ISR actively toggles these state indicators: a Red LED activates during the "Dark" state, and a Green LED activates during the "Light" state. The immediate, flicker-free toggling of these LEDs during lighting transitions serves as undeniable visual proof of the system's flawless hardware debouncing and optimized software routing.

---

## 7. Bill of Materials (BOM)
### Active & Microcontroller
* 1x ESP32 Development Board
* 1x UA741CPE4 Operational Amplifier
* 1x 4N35 Optocoupler
* 1x Light Dependent Resistor (LDR)
* 2x 1N4004 Rectifier Diodes
* 2x 5mm LEDs (1x Blue, 1x Yellow)

### Passives
* 3x 10kΩ Resistors (Voltage divider & Pull-up)
* 1x 100kΩ Resistor (Positive feedback)
* 2x 220Ω Resistors (Current limiting for LEDs)
* 1x 330Ω Resistor (Current limiting for 4N35 LED)

### Power
* 1x 5V 2A Power Adapter
* 1x 5V Power Module
* 1x Breadboard Boost Converter Module (Tuned to 8.5V)

---

## 8. Circuit Schematic
*(Note: View the `Hardware/` directory for the raw EAGLE `.sch` file and full-resolution PDF).*

![Circuit Schematic](Hardware/schem.png) 
*(Replace this link with the actual path to your schematic image in the repo)*

---

## 9. Firmware Implementation
The following code demonstrates the non-blocking architecture, utilizing the `IRAM_ATTR` flag to store the ISR in the ESP32's fastest internal memory for instantaneous execution.

```cpp
#include <Arduino.h>

// 1. Define hardware pins
const int optoPin = 4;      // GPIO 4 connected to the 4N35 Collector
const int ledDarkPin = 13;  // Yellow LED (Active when Dark)
const int ledLightPin = 14; // Blue LED (Active when Light)

// 2. Volatile variables for the ISR
volatile bool stateChanged = false;
volatile int currentSensorState = HIGH;

// 3. The Hardware Interrupt Function (Lightning Fast)
void IRAM_ATTR handleOptoTrigger() {
  stateChanged = true; 
  currentSensorState = digitalRead(optoPin); 
}

void setup() {
  Serial.begin(115200);
  
  pinMode(optoPin, INPUT_PULLUP); 
  pinMode(ledDarkPin, OUTPUT); 
  pinMode(ledLightPin, OUTPUT); 
  
  // Attach interrupt: Triggers on both sunset AND sunrise
  attachInterrupt(digitalPinToInterrupt(optoPin), handleOptoTrigger, CHANGE);
  
  Serial.println("Schmitt Trigger DAQ Online. Awaiting lighting changes...");
  
  // Initialize baseline state
  currentSensorState = digitalRead(optoPin);
  if (currentSensorState == LOW) {
      digitalWrite(ledDarkPin, HIGH);
      digitalWrite(ledLightPin, LOW);
  } else {
      digitalWrite(ledDarkPin, LOW);
      digitalWrite(ledLightPin, HIGH);
  }
}

void loop() {
  // 4. Handle event logic strictly when the hardware flag trips
  if (stateChanged) {
    
    // LOW on the optoPin means the 741 triggered (DARK state)
    if (currentSensorState == LOW) {
      digitalWrite(ledDarkPin, HIGH);  
      digitalWrite(ledLightPin, LOW);  
      Serial.println("Event: DARKNESS DETECTED -> System Logic Triggered");
    } 
    // HIGH means the 741 turned off (LIGHT state)
    else {
      digitalWrite(ledDarkPin, LOW);   
      digitalWrite(ledLightPin, HIGH); 
      Serial.println("Event: LIGHT DETECTED -> System Logic Triggered");
    }
    
    // Reset flag, processor returns to idle/other tasks
    stateChanged = false; 
  }
}
