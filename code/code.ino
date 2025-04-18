// Blynk template information - MUST COME FIRST before any includes
#define BLYNK_TEMPLATE_ID "TMPL3VxCrLOKR"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "ByDDpNCQeZU6OQpHtYUi1inM5z6Q5dVl"
#define BLYNK_PRINT Serial  // Enables Serial Monitor debugging

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// Define which core to use for which task
#define BLYNK_CORE 0        // Core 0 will handle Blynk communications
#define SENSOR_CORE 1       // Core 1 will handle sensor measurements

// WiFi credentials
char ssid[] = "realme X7 Max";       // Your existing WiFi SSID
char pass[] = "fuh8besa";   // Your existing WiFi password

#define TRIG_PIN 5   // Connect to Trig pin of HC-SR04
#define ECHO_PIN 18  // Connect to Echo pin (through voltage divider)

// Tank parameters
const float TANK_EMPTY_DISTANCE = 15.11; // Distance reading when tank is empty (in cm)
const float TANK_FULL_DISTANCE = 1.0;    // Distance reading when tank is full (in cm)
const float TOTAL_VOLUME_LITERS = 1.0;   // Total volume of the container in liters - ADJUST THIS VALUE

// Timer intervals
const unsigned long MEASUREMENT_INTERVAL = 1000;     // Measurement interval (milliseconds)
const unsigned long FUEL_CALCULATION_INTERVAL = 180000; // 3 minutes in milliseconds

// Blynk virtual pins
#define VPIN_WATER_LEVEL V0     // Water level percentage
#define VPIN_VOLUME V1          // Current volume in liters
#define VPIN_REMAINING V2       // Remaining capacity
#define VPIN_FUEL_USAGE_RATE V3 // Fuel usage rate as percentage
#define VPIN_DISTANCE V4        // Distance reading

// Shared variables (need mutex protection)
portMUX_TYPE dataMutex = portMUX_INITIALIZER_UNLOCKED;
long duration = 0;
float distance = 0;               // in cm
float waterLevelPercent = 0;      // Water level in percentage
float currentVolume = 0;          // Current water volume in liters
float previousVolume = 0;
float fuelConsumptionRate = 0;    // Liters per 3 minutes
float fuelConsumptionPercent = 0; // Percentage of total capacity consumed per 3 minutes
bool lowFuelSent = false;

// Timestamps
unsigned long lastMeasurementTime = 0;
unsigned long lastFuelCalculationTime = 0;
unsigned long lastVirtualWriteTime = 0;
const unsigned long VIRTUAL_WRITE_INTERVAL = 2000; // Update Blynk every 2 seconds

// Task handles
TaskHandle_t blynkTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;

// Function declarations
void blynkTask(void *pvParameters);
void sensorTask(void *pvParameters);
void measureWaterLevel();

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("Water Level Monitoring System");
  Serial.println("----------------------------");
  
  // Create tasks on specific cores
  xTaskCreatePinnedToCore(
    blynkTask,           // Task function
    "BlynkTask",         // Task name
    10000,               // Stack size (bytes)
    NULL,                // Parameters
    1,                   // Priority (1=low, configMAX_PRIORITIES-1=highest)
    &blynkTaskHandle,    // Task handle
    BLYNK_CORE);         // Core ID
    
  xTaskCreatePinnedToCore(
    sensorTask,          // Task function
    "SensorTask",        // Task name
    10000,               // Stack size (bytes)
    NULL,                // Parameters
    1,                   // Priority
    &sensorTaskHandle,   // Task handle
    SENSOR_CORE);        // Core ID
}

// Blynk communication task runs on Core 0
void blynkTask(void *pvParameters) {
  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk connected successfully");

  for(;;) {
    Blynk.run();
    
    // Send data to Blynk at regular intervals
    unsigned long currentMillis = millis();
    if (currentMillis - lastVirtualWriteTime >= VIRTUAL_WRITE_INTERVAL) {
      lastVirtualWriteTime = currentMillis;
      
      // Acquire the mutex before accessing shared variables
      portENTER_CRITICAL(&dataMutex);
      
      // Send data to Blynk and print to serial monitor
      Serial.println("\n--- VIRTUAL WRITE VALUES ---");
      
      Serial.print("Water Level: ");
      Serial.print(waterLevelPercent);
      Serial.println("%");
      Blynk.virtualWrite(VPIN_WATER_LEVEL, waterLevelPercent);
      
      Serial.print("Current Volume: ");
      Serial.print(currentVolume);
      Serial.println(" L");
      Blynk.virtualWrite(VPIN_VOLUME, currentVolume);
      
      Serial.print("Remaining Capacity: ");
      Serial.print(TOTAL_VOLUME_LITERS - currentVolume);
      Serial.println(" L");
      Blynk.virtualWrite(VPIN_REMAINING, TOTAL_VOLUME_LITERS - currentVolume);
      
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
      Blynk.virtualWrite(VPIN_DISTANCE, distance);
      
      if (lastFuelCalculationTime > 0) {
        Serial.print("Fuel Usage Rate: ");
        Serial.print(fuelConsumptionPercent);
        Serial.println("% per 3 minutes");
        Blynk.virtualWrite(VPIN_FUEL_USAGE_RATE, fuelConsumptionPercent);
      }
      
      // Check for low fuel alert (below 20%)
      if (waterLevelPercent < 20 && !lowFuelSent) {
        Serial.println("⚠️ ALERT: Fuel level below 20%!");
        Blynk.logEvent("low_fuel", "⚠️ Fuel level below 20%!");
        lowFuelSent = true; // So we don't spam
      }
      if (waterLevelPercent >= 20) {
        lowFuelSent = false; // Reset trigger when refilled
      }
      
      Serial.println("---------------------------");
      
      // Release the mutex
      portEXIT_CRITICAL(&dataMutex);
    }
    
    // Small delay to prevent this task from hogging CPU
    delay(100);
  }
}

// Sensor measurement task runs on Core 1
void sensorTask(void *pvParameters) {
  for(;;) {
    unsigned long currentTime = millis();
    
    // Check if it's time to measure water level
    if (currentTime - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
      measureWaterLevel();
      lastMeasurementTime = currentTime;
    }
    
    // Small delay to prevent this task from hogging CPU
    delay(50);
  }
}

void measureWaterLevel() {
  // Local variables for calculations
  float localDistance;
  float localWaterLevelPercent;
  float localCurrentVolume;
  long localDuration;
  
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo duration
  localDuration = pulseIn(ECHO_PIN, HIGH);

  // Convert duration to distance
  localDistance = localDuration * 0.0343 / 2; // Speed of sound = 343 m/s

  // Calculate water level percentage
  // As the distance decreases, the water level increases
  if (localDistance >= TANK_EMPTY_DISTANCE) {
    localWaterLevelPercent = 0; // Empty tank
  } else if (localDistance <= TANK_FULL_DISTANCE) {
    localWaterLevelPercent = 100; // Full tank
  } else {
    // Map the distance to percentage (inverted relationship)
    localWaterLevelPercent = 100 * (TANK_EMPTY_DISTANCE - localDistance) / (TANK_EMPTY_DISTANCE - TANK_FULL_DISTANCE);
  }

  // Calculate current volume based on percentage
  localCurrentVolume = (localWaterLevelPercent / 100.0) * TOTAL_VOLUME_LITERS;

  // Acquire the mutex before updating shared variables
  portENTER_CRITICAL(&dataMutex);
  
  // Update shared variables
  duration = localDuration;
  distance = localDistance;
  waterLevelPercent = localWaterLevelPercent;
  currentVolume = localCurrentVolume;

  // Calculate fuel consumption every 3 minutes
  unsigned long currentTime = millis();
  if (currentTime - lastFuelCalculationTime >= FUEL_CALCULATION_INTERVAL) {
    // Only calculate if we have a previous reading
    if (lastFuelCalculationTime > 0) {
      // Calculate consumption (negative value means consumption)
      fuelConsumptionRate = previousVolume - currentVolume;
      
      // Calculate consumption as percentage of total capacity
      fuelConsumptionPercent = (fuelConsumptionRate / TOTAL_VOLUME_LITERS) * 100;
      
      // Make sure we only show actual consumption (not refills)
      if (fuelConsumptionRate < 0) {
        fuelConsumptionRate = 0; // Tank was refilled, not consumed
        fuelConsumptionPercent = 0;
      }
    }
    
    // Store current volume for next calculation
    previousVolume = currentVolume;
    lastFuelCalculationTime = currentTime;
  }
  
  // Release the mutex
  portEXIT_CRITICAL(&dataMutex);
}

void loop() {
  // Empty loop - tasks handle everything
  delay(1000);
}