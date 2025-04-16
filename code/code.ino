#include <WiFi.h>

#define TRIG_PIN 5   // Connect to Trig pin of HC-SR04
#define ECHO_PIN 18  // Connect to Echo pin (through voltage divider)

// Tank parameters
const float TANK_EMPTY_DISTANCE = 15.11; // Distance reading when tank is empty (in cm)
const float TANK_FULL_DISTANCE = .0;   // Distance reading when tank is full (in cm)

// Timer interval (milliseconds)
const unsigned long MEASUREMENT_INTERVAL = 1000;
unsigned long lastMeasurementTime = 0;

long duration;
float distance; // in cm
float waterLevelPercent; // Water level in percentage

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("Water Level Monitoring System");
  Serial.println("-----------------------------");
}

void measureWaterLevel() {
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo duration
  duration = pulseIn(ECHO_PIN, HIGH);

  // Convert duration to distance
  distance = duration * 0.0343 / 2; // Speed of sound = 343 m/s

  // Calculate water level percentage
  // As the distance decreases, the water level increases
  if (distance >= TANK_EMPTY_DISTANCE) {
    waterLevelPercent = 0; // Empty tank
  } else if (distance <= TANK_FULL_DISTANCE) {
    waterLevelPercent = 100; // Full tank
  } else {
    // Map the distance to percentage (inverted relationship)
    waterLevelPercent = 100 * (TANK_EMPTY_DISTANCE - distance) / (TANK_EMPTY_DISTANCE - TANK_FULL_DISTANCE);
  }

  // Print the distance and water level
  Serial.print("Distance from sensor: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  Serial.print("Water Level: ");
  Serial.print(waterLevelPercent);
  Serial.println("%");
  
  Serial.print("Water Reduction: ");
  Serial.print(100 - waterLevelPercent);
  Serial.println("%");
  
  Serial.println(); // Empty line for better readability
}

void loop() {
  // Check if it's time to measure water level
  unsigned long currentTime = millis();
  if (currentTime - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
    measureWaterLevel();
    lastMeasurementTime = currentTime;
  }
}
