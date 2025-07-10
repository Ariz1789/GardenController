// FUNCTIONS PROTOTYPES
// Function to control the built-in LED directly (non-blocking if not delaying)
void setBoardLEDState(uint8_t led, bool isOn);
// FreeRTOS Task for LED control
void ledControlTask(void *pvParameters);

void offAllLeds();

// Enum for LED modes controlled by the task
enum LedMode {
  LED_MODE_OFF,
  LED_MODE_ON,
  LED_MODE_BLINK_SLOW, // E.g., 1000ms on, 1000ms off
  LED_MODE_BLINK_FAST  // E.g., 200ms on, 200ms off
};

// Individual LED control functions
void setMainLed(LedMode mode);
void setRedLed(LedMode mode);
void setGreenLed(LedMode mode);
void setBlueLed(LedMode mode);


// Struct for LED control (removed const members and volatile)
struct LedStruct {
  uint8_t ledPin;
  LedMode ledMode;
};

// Global variable for LED control
LedStruct redLed;
LedStruct greenLed;
LedStruct blueLed;
LedStruct mainLed;

void ledInit(){
  
   // Initialize pins as outputs
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize LED structure
  redLed.ledPin = LEDR;
  redLed.ledMode = LED_MODE_OFF;

  greenLed.ledPin = LEDG;
  greenLed.ledMode = LED_MODE_OFF;

  blueLed.ledPin = LEDB;
  blueLed.ledMode = LED_MODE_OFF;
  
  mainLed.ledPin = LED_BUILTIN;
  mainLed.ledMode = LED_MODE_OFF;

  // Create the LED control task
  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "RedLED Control",// Name of task
    2048,             // Stack size (bytes) - adjust if needed
    &redLed,          // Parameter to pass to the task
    1,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );

  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "GreenLED Control",// Name of task
    2048,             // Stack size (bytes) - adjust if needed
    &greenLed,          // Parameter to pass to the task
    2,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );

  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "BlueLED Control",// Name of task
    2048,             // Stack size (bytes) - adjust if needed
    &blueLed,          // Parameter to pass to the task
    3,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );

  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "MainLED Control",// Name of task
    2048,             // Stack size (bytes) - adjust if needed
    &mainLed,          // Parameter to pass to the task
    4,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );
  Serial.println("LED Control Task created on Core 0.");

  mainLed.ledMode = LED_MODE_OFF;
  redLed.ledMode = LED_MODE_OFF;
  greenLed.ledMode = LED_MODE_OFF;
  blueLed.ledMode = LED_MODE_OFF;
}


void offAllLeds(){
  mainLed.ledMode = LED_MODE_OFF;
  redLed.ledMode = LED_MODE_OFF;
  greenLed.ledMode = LED_MODE_OFF;
  blueLed.ledMode = LED_MODE_OFF;
}
// Individual LED control functions
void setMainLed(LedMode mode) {
    mainLed.ledMode = mode;
}

void setRedLed(LedMode mode) {
    redLed.ledMode = mode;
}

void setGreenLed(LedMode mode) {
    greenLed.ledMode = mode;
}

void setBlueLed(LedMode mode) {
    blueLed.ledMode = mode;
}


// Function to control the built-in LED directly
void setBoardLEDState(uint8_t led, bool isOn) {
  // Serial.println("Setting LED pin " + String(led) + " to " + (isOn ? "ON (LOW)" : "OFF (HIGH)"));
  digitalWrite(led, isOn ? LOW : HIGH); // Note: ESP32 LEDs are active LOW
}

void ledControlTask(void *pvParameters) {
  // Cast the parameter back to our struct pointer
  LedStruct* ledStr = (LedStruct*)pvParameters;
  
  uint8_t led = ledStr->ledPin;
  bool ledState = false; // Current LED state (false = OFF, true = ON)
  int blinkDelayMs = 0;
  
  for (;;) { // Infinite loop for the task
    switch (ledStr->ledMode) {
      case LED_MODE_OFF:
        if (ledState == true) { // Only change if necessary
          // Serial.println("Setting LED pin " + String(led) + " to OFF");
          setBoardLEDState(led, false);
          ledState = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to yield CPU
        break;

      case LED_MODE_ON:
        if (ledState == false) { // Only change if necessary
          // Serial.println("Setting LED pin " + String(led) + " to ON");
          setBoardLEDState(led, true);
          ledState = true;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to yield CPU
        break;

      case LED_MODE_BLINK_SLOW:
        // Serial.println("Setting LED pin " + String(led) + " to SLOW");
        blinkDelayMs = 1000;
        ledState = !ledState; // Toggle LED state
        setBoardLEDState(led, ledState);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMs));
        break;

      case LED_MODE_BLINK_FAST:
        // Serial.println("Setting LED pin " + String(led) + " to FAST");
        blinkDelayMs = 200;
        ledState = !ledState; // Toggle LED state
        setBoardLEDState(led, ledState);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMs));
        break;
    }
  }
}