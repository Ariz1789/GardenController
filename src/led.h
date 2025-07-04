
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