// Function to control the built-in LED directly (non-blocking if not delaying)
void setBoardLEDState(bool state);
// FreeRTOS Task for LED control
void ledControlTask(void *pvParameters);

// Enum for LED modes controlled by the task
enum LedMode {
  LED_MODE_OFF,
  LED_MODE_ON,
  LED_MODE_BLINK_SLOW, // E.g., 1000ms on, 1000ms off
  LED_MODE_BLINK_FAST  // E.g., 200ms on, 200ms off
};

// Global variable to share LED mode with the FreeRTOS task
// 'volatile' is crucial as it's modified by one context (loop) and read by another (task)
volatile LedMode mainLedMode;
volatile LedMode redLedMode;
volatile LedMode greenLedMode;
volatile LedMode bluLedMode;

void ledInit(){
  
  // put your setup code here, to run once:
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT); // Set pin 8 (D8) as an output
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off initially

  // --- Create the LED control FreeRTOS task ---
  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "MainLED Control",    // Name of task
    2048,             // Stack size (bytes) - adjust if needed
    NULL,             // Parameter to pass to the task
    1,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );
  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "RedLED Control",    // Name of task
    2048,             // Stack size (bytes) - adjust if needed
    NULL,             // Parameter to pass to the task
    1,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );
  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "GreenLED Control",    // Name of task
    2048,             // Stack size (bytes) - adjust if needed
    NULL,             // Parameter to pass to the task
    1,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );
  xTaskCreatePinnedToCore(
    ledControlTask,   // Task function
    "BlueLED Control",    // Name of task
    2048,             // Stack size (bytes) - adjust if needed
    NULL,             // Parameter to pass to the task
    1,                // Task priority (0 is lowest, configMAX_PRIORITIES-1 is highest)
    NULL,             // Task handle (not used here)
    0                 // Core to run on (Core 0 for background tasks)
  );
  Serial.println("LED Control Task created on Core 0.");

  mainLedMode = LED_MODE_OFF;
  redLedMode = LED_MODE_ON;
  greenLedMode = LED_MODE_OFF;
  bluLedMode = LED_MODE_OFF;
}

// Function to control the built-in LED directly (non-blocking if not delaying)
void setBoardLEDState(bool state) {
  digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
}

// FreeRTOS Task for LED control
void ledControlTask(void *pvParameters) {
  (void) pvParameters; // Cast to void to suppress unused parameter warning

  bool ledState = LOW;
  int blinkDelayMs = 0;

  for (;;) { // Infinite loop for the task
    switch (mainLedMode) {
      case LED_MODE_OFF:
        if (ledState == HIGH) { // Only change if necessary
          setBoardLEDState(false);
          ledState = LOW;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to yield CPU
        break;

      case LED_MODE_ON:
        if (ledState == LOW) { // Only change if necessary
          setBoardLEDState(true);
          ledState = HIGH;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to yield CPU
        break;

      case LED_MODE_BLINK_SLOW:
        blinkDelayMs = 1000;
        setBoardLEDState(!ledState); // Toggle LED state
        ledState = !ledState;
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMs));
        break;

      case LED_MODE_BLINK_FAST:
        blinkDelayMs = 200;
        setBoardLEDState(!ledState); // Toggle LED state
        ledState = !ledState;
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMs));
        break;
    }
  }
}