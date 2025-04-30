#include <DHT.h>
#include <ESP32Servo.h>

// === Pin setup ===
#define DHTPIN 4              // DHT22 sensor connected to pin 4
#define DHTTYPE DHT22         // DHT sensor type

DHT dht(DHTPIN, DHTTYPE);     // Create DHT object

const int ledPin = 13;        // LED pin
const int buttonPin = 5;      // Button pin
const int potPin = 34;        // Potentiometer pin (analog)
const int servoPin = 18;      // Servo motor pin

const float tempThreshold = 32; // Lowered temperature limit
const float hysteresis = 0.2;     // Smaller hysteresis for quicker switching

Servo myServo;               // Servo object

// === Moving average setup ===
#define TEMP_HISTORY 10      // Number of temperature samples
float tempReadings[TEMP_HISTORY];
int tempIndex = 0;

// === LED blinking ===
unsigned long previousMillis = 0;
const unsigned long blinkInterval = 500;
bool ledState = false;
bool overheating = false;

// === Mode switching ===
volatile int mode = 0;                 // Current mode (0, 1, 2)
unsigned long lastDebounceTime = 0;    // Last button press time
const unsigned long debounceDelay = 200; // Debounce delay

// Change mode when button is pressed
void IRAM_ATTR changeMode() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    mode = (mode + 1) % 3;            // Go to next mode
    lastDebounceTime = millis();
  }
}

void setup() {
  Serial.begin(115200);       // Start serial monitor
  dht.begin();                // Start DHT sensor
  myServo.attach(servoPin);   // Attach servo motor

  pinMode(ledPin, OUTPUT);           // Set LED as output
  pinMode(buttonPin, INPUT_PULLUP);  // Set button with pull-up

  attachInterrupt(digitalPinToInterrupt(buttonPin), changeMode, FALLING); // Interrupt on button press

  // Fill temp readings with 0
  for (int i = 0; i < TEMP_HISTORY; i++) {
    tempReadings[i] = 0;
  }

  Serial.println("System Ready ✅");
}

void loop() {
  static unsigned long lastReadTime = 0;
  unsigned long currentMillis = millis();

  // Read sensor every 1 second
  if (currentMillis - lastReadTime >= 1000) {
    lastReadTime = currentMillis;

    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Check if sensor failed
    if (isnan(temp) || isnan(humidity)) {
      Serial.println("❌ Sensor Read Failure!");
      return;
    }

    // === Apply moving average to temperature ===
    tempReadings[tempIndex] = temp;
    tempIndex = (tempIndex + 1) % TEMP_HISTORY;

    float avgTemp = 0;
    for (int i = 0; i < TEMP_HISTORY; i++) {
      avgTemp += tempReadings[i];
    }
    avgTemp /= TEMP_HISTORY;

    float heatIndex = dht.computeHeatIndex(temp, humidity, false);

    // Check if overheating
    if (avgTemp > tempThreshold + hysteresis) {
      overheating = true;
    } else if (avgTemp < tempThreshold - hysteresis) {
      overheating = false;
    }

    // Blink LED if overheating
    if (overheating) {
      if (currentMillis - previousMillis >= blinkInterval) {
        previousMillis = currentMillis;
        ledState = !ledState;
        digitalWrite(ledPin, ledState ? HIGH : LOW);
      }
    } else {
      digitalWrite(ledPin, LOW);
    }

    // Debug Output
    Serial.println("----------");
    Serial.printf("🌡 Raw Temp: %.2f °C\n", temp);
    Serial.printf("🌡 Avg Temp: %.2f °C\n", avgTemp);
    Serial.printf("🚨 Overheating: %s\n", overheating ? "YES" : "NO");

    switch (mode) {
      case 0:
        Serial.println("📘 Mode 0: Temperature Only");
        break;

      case 1:
        Serial.println("📗 Mode 1: Temp + Humidity + Heat Index");
        Serial.printf("💧 Humidity: %.2f %%\n", humidity);
        Serial.printf("🔥 Heat Index: %.2f °C\n", heatIndex);
        break;

      case 2:
        Serial.println("📙 Mode 2: Potentiometer Controls Servo");
        int potValue = analogRead(potPin);
        int servoAngle = map(potValue, 0, 2640, 180, 0);  // Adjust if your ADC range is different
        myServo.write(servoAngle);
        Serial.printf("🎚 Pot: %d -> Servo Angle: %d°\n", potValue, servoAngle);
        break;
    }
  }

  delay(50);  // Small delay for smoother loop
}
