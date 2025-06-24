#include <ESP32Time.h>
#include <HardwareSerial.h> // For Serial.println, ensure it's available after deep sleep
#include <WiFi.h>           // For WiFi connection
#include <time.h>           // For NTP functions (time.h standard C library)
#include "Arduino.h"

// FreeRTOS includes for tasks
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


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

// Functions prototypes
// Function to control the built-in LED directly (non-blocking if not delaying)
void setBoardLEDState(bool state);
// FreeRTOS Task for LED control
void ledControlTask(void *pvParameters);
void goIntoSpleep();
void activateRelay();
void print_wakeup_reason();
bool syncTimeWithNTP();
void setWebServer(void *parameter);

const int RELAY_PIN  = 8;

// WiFi credentials for your smartphone hotspot
const char* WIFI_SSID = "Ariz";
const char* WIFI_PASSWORD = "Ariz1789?";

// NTP Server settings
const char* NTP_SERVER_1 = "pool.ntp.org"; // Primary NTP server
const char* NTP_SERVER_2 = "time.nist.gov"; // Secondary NTP server (optional, for redundancy)

// Timezone settings for Italy (CEST currently)
// Format: "TZ_NAME_STANDARD_TIME_OFFSET_FROM_UTC_IN_HOURS_DAYLIGHT_SAVING_TIME_RULE"
// Example for Central European Time (CET) / Central European Summer Time (CEST):
// "CET-1CEST,M3.5.0,M10.5.0/3"
// CET = Central European Time (UTC+1)
// CEST = Central European Summer Time (UTC+2)
// M3.5.0 = Last Sunday in March (start of DST)
// M10.5.0/3 = Last Sunday in October at 3 AM (end of DST)
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

ESP32Time rtc(3600); // Initial offset. If you use configTzFile, this might become less critical for direct rtc.adjust


// Define activation times
const int MORNING_H = 5;
const int EVENING_H = 22;

const long ACTIVATION_MINUTES = 5; // 5 minutes in seconds
const long SPLEEP_MINUTES = 10;

const int BOOT_BUTTON_PIN = 0;

RTC_DATA_ATTR int bootCount = 0; // To track how many times the ESP32 has woken up
RTC_DATA_ATTR bool morningActivation = false;
RTC_DATA_ATTR bool eveningActivation = false;

void setup() {

  Serial.begin(9600);
  delay(100); //wait for Serial to initialize
  Serial.println("--- Setup Start ---");

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

  // Increment boot count every time the ESP32 starts (after reset or deep sleep wake)
  bootCount++;
  Serial.printf("Boot number: %d\n", bootCount);

  // Initialize RTC time only if it's the very first boot or after a power cycle
  // (RTC_DATA_ATTR variables are reset on power cycle, but not deep sleep)
  if (bootCount == 1) {
    Serial.println("Setting initial RTC time...");
    // Use rtc.setTime(second, minute, hour, day, month, year)
    rtc.setTime(0, 0, 0, 13, 6, 2025);
    syncTimeWithNTP();
  }

  // Print wake up reason for debugging
  print_wakeup_reason();

  // activation bool reset
  eveningActivation = false;
  morningActivation = false;

  // --- Configure button for deep sleep wakeup ---
  // Enable EXT0 wakeup on GPIO0 (BOOT button) to trigger on a LOW level
  // This means pressing the button will wake the ESP32.
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  Serial.println("Button wakeup enabled on GPIO0 (BOOT button).");
  redLedMode = LED_MODE_OFF;
  Serial.println("--- Setup End ---");
}

void loop() {
  Serial.println("--- Loop Start ---");
  
  mainLedMode = LED_MODE_ON; // Keep LED on while main loop is active
  greenLedMode = LED_MODE_BLINK_SLOW;

  // Get current time
  int currentHour = rtc.getHour();
  int currentMinute = rtc.getMinute();
  int currentSecond = rtc.getSecond();

  Serial.println("Current time: ");
  Serial.println(rtc.getTime());
  Serial.println("Current date: ");
  Serial.println(rtc.getDate());

  bool morningActivation = false;
  bool eveningActivation = false;

  // Check for 5 AM activation window
  if (currentHour == MORNING_H && currentMinute >= 0 && currentMinute < 30 && !morningActivation )
  {
    morningActivation = true;
    Serial.println("Within 5 AM activation window.");
    activateRelay();
  }
  // Check for 10 PM activation window
  else if (currentHour == EVENING_H && currentMinute >= 0 && currentMinute < 30 && !eveningActivation) 
  {
    eveningActivation = true;
    Serial.println("Within 10 PM activation window.");
    activateRelay();
  }else if (currentHour == EVENING_H && currentMinute > 35 && morningActivation && eveningActivation) 
  {
    eveningActivation = false;
    morningActivation = false;
    Serial.println("Resetting activation flags after evening window.");
    delay(1000); // Wait for 1 second before going to sleep
  }else{
    Serial.println("Waiting a minute since not in sctivation time");
    // wait for a minute to keep board on for some times
    delay(1 * 60 * 1000);
    for (int i = 0; i < 6; i++)
    {
      if(syncTimeWithNTP()){break;}
      delay(50);
    }
    Serial.println("All attempts to update time done");
  }

  Serial.println("--- Loop End ---");
  
  greenLedMode = LED_MODE_OFF;
  goIntoSpleep();
}

void goIntoSpleep()
{
  digitalWrite(LED_BUILTIN, LOW);

  long sleepTimeUs = SPLEEP_MINUTES * 60 * 1000000ULL; // Convert seconds to microseconds
  Serial.println("Start sleeping for " + String(SPLEEP_MINUTES) + " minutes.");
  esp_sleep_enable_timer_wakeup(sleepTimeUs);
  Serial.flush(); // Ensure all serial output is sent before sleeping
  esp_deep_sleep_start();
}

void activateRelay()
{ 
  bluLedMode = LED_MODE_BLINK_FAST;
  Serial.println("Activating relay for " + String(ACTIVATION_MINUTES) + " minutes.");
  digitalWrite(RELAY_PIN, HIGH); // Activate the relay

  delay(ACTIVATION_MINUTES * 60 * 1000); // Stay awake for ACTIVATION_MINUTES minutes
  
  digitalWrite(RELAY_PIN, LOW); // Deactivate the relay
  Serial.println("Relay deactivated after " + String(ACTIVATION_MINUTES) + " minutes.");
  bluLedMode = LED_MODE_OFF;
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

// Function to connect to WiFi and synchronize time via NTP
bool syncTimeWithNTP() {
  bool retValue = false;
  Serial.println("\nConnecting to WiFi for time sync...");
  redLedMode = LED_MODE_BLINK_FAST;
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int connectionAttempts = 0;
  // Wait for connection or timeout
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) { // Try for ~10 seconds (20 * 500ms)
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected! IP address: " + WiFi.localIP().toString());
    
    // Configure NTP client
    // Set the timezone string for Italy. This will handle DST automatically.
    // Europe/Rome is the correct timezone for Italy.
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
    // setTZ("CET-1CEST,M3.5.0,M10.5.0/3"); // Old way, less flexible. configTzFile is better.

    Serial.println("Synchronizing time with NTP server...");
    // Give time for NTP to sync
    long current_epoch_seconds = time(nullptr); // Get current epoch time (will be 0 if not synced yet)
    int sync_attempts = 0;
    while (current_epoch_seconds < 1000000000 && sync_attempts < 10) { // Wait for a valid epoch time (e.g., > 2001)
      delay(1000); // Wait for 1 second
      current_epoch_seconds = time(nullptr);
      Serial.print("*");
      sync_attempts++;
    }
    Serial.println();

    if (current_epoch_seconds > 1000000000) { // Check if time is actually synchronized
      Serial.println("Time synchronized successfully via NTP!");
      rtc.setTime(current_epoch_seconds); // Update ESP32Time RTC
      Serial.print("RTC time updated: ");
      Serial.println(rtc.getDateTime());
      retValue = true;
    } else {
      Serial.println("NTP time synchronization failed or timed out.");
    }
    WiFi.disconnect(true); // Disconnect from WiFi to save power
    Serial.println("WiFi disconnected.");
  } else {
    Serial.println("Failed to connect to WiFi hotspot.");
  }
  WiFi.mode(WIFI_OFF); // Turn off WiFi radio completely
  redLedMode = LED_MODE_OFF;
  return retValue;
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

// --- End LED Control Functions & Task ---