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

// Define a specific low fuel event name
#define LOW_FUEL_EVENT "low_fuel"

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
const unsigned long FUEL_CALCULATION_INTERVAL = 10000; // Temporarily reduced to 10 seconds for testing (from 180000)
const unsigned long VIRTUAL_WRITE_INTERVAL = 3000;   // Update Blynk every 3 seconds (increased from 2000)

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
bool serialInitialized = false;

// Task handles
TaskHandle_t blynkTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t serialTaskHandle = NULL;

// Function declarations
void blynkTask(void *pvParameters);
void sensorTask(void *pvParameters);
void serialOutputTask(void *pvParameters);
void measureWaterLevel();

void setup() {
  Serial.begin(115200);
  delay(500); // Give serial monitor time to start
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Wait for serial to be ready
  uint8_t i = 0;
  while (!Serial && i < 100) {
    delay(50);
    i++;
  }
  
  Serial.println("\n\n");
  Serial.println("============================================");
  Serial.println("Water Level Monitoring System with FreeRTOS");
  Serial.println("============================================");
  Serial.flush(); // Make sure the message is sent
  
  // Create tasks on specific cores
  xTaskCreatePinnedToCore(
    sensorTask,          // Task function
    "SensorTask",        // Task name
    10000,               // Stack size (bytes)
    NULL,                // Parameters
    1,                   // Priority
    &sensorTaskHandle,   // Task handle
    SENSOR_CORE);        // Core ID
    
  delay(500); // Give tasks time to initialize
    
  xTaskCreatePinnedToCore(
    blynkTask,           // Task function
    "BlynkTask",         // Task name
    10000,               // Stack size (bytes)
    NULL,                // Parameters
    1,                   // Priority
    &blynkTaskHandle,    // Task handle
    BLYNK_CORE);         // Core ID
    
  delay(500);
  
  // Create a dedicated task for serial output on the same core as sensor task
  xTaskCreatePinnedToCore(
    serialOutputTask,    // Task function
    "SerialTask",        // Task name
    10000,               // Stack size (bytes)
    NULL,                // Parameters
    2,                   // Higher priority
    &serialTaskHandle,   // Task handle
    SENSOR_CORE);        // Core ID
    
  Serial.print("Blynk task running on core: ");
  Serial.println(BLYNK_CORE);
  
  Serial.print("Sensor task running on core: ");
  Serial.println(SENSOR_CORE);
  
  Serial.println("Setup complete!");
  Serial.flush();
}

// Dedicated task for serial output
void serialOutputTask(void *pvParameters) {
  delay(2000); // Wait for other tasks to initialize
  Serial.println("Serial output task started");
  Serial.flush();
  
  for(;;) {
    // Send data at regular intervals
    unsigned long currentMillis = millis();
    if (currentMillis - lastVirtualWriteTime >= VIRTUAL_WRITE_INTERVAL) {
      lastVirtualWriteTime = currentMillis;
      
      // Acquire the mutex before accessing shared variables
      portENTER_CRITICAL(&dataMutex);
      
      // Print to serial monitor
      Serial.println("\n--- MONITORING VALUES ---");
      Serial.flush();
      
      Serial.print("Water Level: ");
      Serial.print(waterLevelPercent);
      Serial.println("%");
      Serial.flush();
      
      Serial.print("Current Volume: ");
      Serial.print(currentVolume);
      Serial.println(" L");
      Serial.flush();
      
      Serial.print("Remaining Capacity: ");
      Serial.print(TOTAL_VOLUME_LITERS - currentVolume);
      Serial.println(" L");
      Serial.flush();
      
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
      Serial.flush();
      
      // Always display the last calculated fuel usage rate
      Serial.print("Fuel Usage Rate: ");
      Serial.print(fuelConsumptionPercent);
      Serial.println("% per monitoring interval");
      Serial.flush();
      
      // Check for low fuel alert (below 20%)
      if (waterLevelPercent < 20) {
        Serial.println("⚠️ ALERT: Fuel level below 20%!");
        Serial.flush();
        
        // Send the alert if not sent already
        if (!lowFuelSent) {
          Serial.println("Sending low fuel alert notification");
          Serial.flush();
          
          // This will be handled in the Blynk task to avoid mutex issues
          lowFuelSent = true; // Mark as sent
        }
      }
      else if (waterLevelPercent >= 20 && lowFuelSent) {
        Serial.println("Fuel level normal again. Resetting alert status.");
        Serial.flush();
        lowFuelSent = false; // Reset trigger when refilled
      }
      
      Serial.println("---------------------------");
      Serial.flush();
      
      // Release the mutex
      portEXIT_CRITICAL(&dataMutex);
    }
    
    // Small delay to prevent this task from hogging CPU
    delay(200);
  }
}

// Blynk communication task runs on Core 0
void blynkTask(void *pvParameters) {
  Serial.println("Initializing Blynk... (Core 0)");
  Serial.flush();
  
  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk connected successfully");
  Serial.flush();

  // Create a variable to track alert state in this task
  bool localLowFuelSent = false;

  for(;;) {
    Blynk.run();
    
    // Send data to Blynk at regular intervals
    unsigned long currentMillis = millis();
    if (currentMillis - lastVirtualWriteTime >= VIRTUAL_WRITE_INTERVAL) {
      // No need to update lastVirtualWriteTime here, the serial task does it
      
      // Acquire the mutex before accessing shared variables
      portENTER_CRITICAL(&dataMutex);
      
      // Send data to Blynk
      Blynk.virtualWrite(VPIN_WATER_LEVEL, waterLevelPercent);
      Blynk.virtualWrite(VPIN_VOLUME, currentVolume);
      Blynk.virtualWrite(VPIN_REMAINING, TOTAL_VOLUME_LITERS - currentVolume);
      Blynk.virtualWrite(VPIN_DISTANCE, distance);
      Blynk.virtualWrite(VPIN_FUEL_USAGE_RATE, fuelConsumptionPercent);
      
      // Check for low fuel alert (below 20%)
      if (waterLevelPercent < 20 && !localLowFuelSent) {
        // Log event to Blynk cloud
        Blynk.logEvent(LOW_FUEL_EVENT, "⚠️ WARNING: Fuel level below 20%! Current level: " + String(waterLevelPercent) + "%");
        localLowFuelSent = true; // Track locally to avoid constant alerts
      }
      else if (waterLevelPercent >= 20) {
        localLowFuelSent = false; // Reset local trigger when refilled
      }
      
      // Update the shared alert state variable
      lowFuelSent = localLowFuelSent;
      
      // Release the mutex
      portEXIT_CRITICAL(&dataMutex);
    }
    
    // Small delay to prevent this task from hogging CPU
    delay(100);
  }
}

// Sensor measurement task runs on Core 1
void sensorTask(void *pvParameters) {
  Serial.println("Starting sensor monitoring... (Core 1)");
  Serial.flush();
  
  // Initial reading
  measureWaterLevel();
  lastMeasurementTime = millis();
  
  // Initialize previous volume with first reading
  previousVolume = currentVolume;
  lastFuelCalculationTime = millis();
  
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
  localDuration = pulseIn(ECHO_PIN, HIGH, 26000); // Add timeout of 26ms (max ~4.5m)

  // Check for timeout/invalid reading
  if (localDuration == 0) {
    localDistance = TANK_EMPTY_DISTANCE; // Assume empty tank if timeout
  } else {
    // Convert duration to distance
    localDistance = localDuration * 0.0343 / 2; // Speed of sound = 343 m/s
  }

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

  // Calculate fuel consumption every interval
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