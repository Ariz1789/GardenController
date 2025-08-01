
#include <HardwareSerial.h> // For Serial.println, ensure it's available after deep sleep
#include <WiFi.h>           // For WiFi connection
#include <time.h>           // For NTP functions (time.h standard C library)
#include "Arduino.h"

// FreeRTOS includes for tasks
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// my libraries
#include "led.h"          // For LED control functions  
#include "boardTime.h"   // For time synchronization functions
#include "boardSleep.h"  // For sleep and wake functions

// FUNCTIONS PROTOTYPES
void activateRelay();

const bool DEBUG = false;
bool timeSet = false;

const int RELAY_PIN  = 8;

// Define activation times
const int MORNING_H = 5;
const int EVENING_H = 22;

const long ACTIVATION_MINUTES = 5;
const long SLEEP_MINUTES = 10;

const int BOOT_BUTTON_PIN = 0;

RTC_DATA_ATTR int bootCount = 0; // To track how many times the ESP32 has woken up
RTC_DATA_ATTR bool morningActivation = false;
RTC_DATA_ATTR bool eveningActivation = false;

void setup() {
  setMainLed(LED_MODE_BLINK_FAST);
  Serial.begin(115200);
  delay(1000); //wait for Serial to initialize
  Serial.println("--- Setup Start ---");
  
  timeSet = false;

  ledInit();
  
  pinMode(RELAY_PIN, OUTPUT); // Set pin 8 (D8) as an output
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off initially

  Serial.println("waiting a little bit...");
  for (int i = 0; i < 5; i++)
  {
    setGreenLed(LED_MODE_ON);
    delay(1000);
    setGreenLed(LED_MODE_OFF);
  }
  Serial.println("waiting done...");

  // Increment boot count every time the ESP32 starts (after reset or deep sleep wake)
  bootCount++;
  Serial.printf("Boot number: %d\n", bootCount);

  // Initialize RTC time only if it's the very first boot or after a power cycle
  // (RTC_DATA_ATTR variables are reset on power cycle, but not deep sleep)
  if (bootCount == 1) {
    Serial.println("Setting initial RTC time...");
    // Use rtc.setTime(second, minute, hour, day, month, year)
    rtc.setTime(0, 0, 0, 13, 6, 2025);
    timeSet = syncTimeWithNTP();
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
  offAllLeds();
  Serial.println("--- Setup End ---");
}

void loop() {
  Serial.println("--- Loop Start ---");
  setMainLed(LED_MODE_BLINK_SLOW);

  // Get current time
  int currentHour = rtc.getHour();
  int currentMinute = rtc.getMinute();
  int currentSecond = rtc.getSecond();

  Serial.println("Time Set: " + String(timeSet));
  Serial.println("Current time: ");
  Serial.println(rtc.getTime());
  Serial.println("Current date: ");
  Serial.println(rtc.getDate());

  if(DEBUG){
    // always activate for short period
    activateRelay();
    delay(5000);
  }else{
    // Check for 5 AM activation window
    if (timeSet && currentHour == MORNING_H && currentMinute >= 0 && currentMinute < 30 && !morningActivation )
    {
      morningActivation = true;
      Serial.println("Within 5 AM activation window.");
      activateRelay();
    }
    // Check for 10 PM activation window
    else if (timeSet && currentHour == EVENING_H && currentMinute >= 0 && currentMinute < 30 && !eveningActivation) 
    {
      eveningActivation = true;
      Serial.println("Within 10 PM activation window.");
      activateRelay();
    }else if (timeSet && currentHour == EVENING_H && currentMinute > 35 && morningActivation && eveningActivation) 
    {
      eveningActivation = false;
      morningActivation = false;
      Serial.println("Resetting activation flags after evening window.");
      delay(1000);
    }else{
      Serial.println("Attempting to sync time for 5 min or less if done");
      setBlueLed(LED_MODE_OFF);
      if(!timeSet){
        for (int i = 0; i < 5; i++)
        {
          Serial.println("Attempt # " + String(i));  
          timeSet = syncTimeWithNTP();
          if(timeSet){break;}
          Serial.println("Waiting 1 minute for next attempt "); 
          setBlueLed(LED_MODE_BLINK_SLOW);
          delay(60*1000);
        }
        Serial.println("All attempts to update time done");
      }else{
        Serial.println("Time already correctly set");
      }
      
    }
  }
  
  Serial.println("--- Loop End ---");
  
  offAllLeds();
  // if(DEBUG){
    
  //   digitalWrite(LED_BUILTIN, LOW);
  //   goIntoSpleep(1L);
  // }else{
  //   digitalWrite(LED_BUILTIN, LOW);
  //   goIntoSpleep(SLEEP_MINUTES);
  // }
  setMainLed(LED_MODE_BLINK_SLOW);
  delay(60*1000);
}

void activateRelay()
{ 
  setRedLed(LED_MODE_ON);
  Serial.println("Activating relay for " + String(ACTIVATION_MINUTES) + " minutes.");
  digitalWrite(RELAY_PIN, HIGH); // Activate the relay

  if(DEBUG){
    delay(2000);
  }else{
    delay(ACTIVATION_MINUTES * 60 * 1000); // Stay awake for ACTIVATION_MINUTES minutes
  }
  
  digitalWrite(RELAY_PIN, LOW); // Deactivate the relay
  Serial.println("Relay deactivated after " + String(ACTIVATION_MINUTES) + " minutes.");
  setRedLed(LED_MODE_OFF);
}
