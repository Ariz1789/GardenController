#include <ESP32Time.h>
#include <HardwareSerial.h> // For Serial.println, ensure it's available after deep sleep
#include <WiFi.h>           // For WiFi connection
#include <time.h>           // For NTP functions (time.h standard C library)
#include "Arduino.h"

// Functions prototypes

void goIntoSpleep();
void activateRelay();
void print_wakeup_reason();
void syncTimeWithNTP();
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

// Set web server port number to 80
WiFiServer server(80);
// Variable to store the HTTP request
String header;
// TaskHandle_t Task1;


void setup() {

  Serial.begin(9600);
  delay(100); //wait for Serial to initialize
  Serial.println("--- Setup Start ---");

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

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT); // Set pin 8 (D8) as an output
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off initially

  // start webservice
  // xTaskCreatePinnedToCore(
  //     setWebServer, /* Function to implement the task */
  //     "webservice", /* Name of the task */
  //     10000,  /* Stack size in words */
  //     NULL,  /* Task input parameter */
  //     0,  /* Priority of the task */
  //     &Task1,  /* Task handle. */
  //     0); /* Core where the task should run */

  // activation bool reset
  eveningActivation = false;
  morningActivation = false;

  // --- Configure button for deep sleep wakeup ---
  // Enable EXT0 wakeup on GPIO0 (BOOT button) to trigger on a LOW level
  // This means pressing the button will wake the ESP32.
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  Serial.println("Button wakeup enabled on GPIO0 (BOOT button).");
  Serial.println("--- Setup End ---");
}

void loop() {
  Serial.println("--- Loop Start ---");
  digitalWrite(LED_BUILTIN, HIGH); // Indicate board is awake

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
    // wait for a minute to keep board on for some times
    delay(1 * 60 * 1000);
    for (int i = 0; i < 6; i++)
    {
      syncTimeWithNTP();
      delay(50);
    }
    Serial.println("All attempts to update time done");
  }

  Serial.println("--- Loop End ---");
  
  // vTaskDelete(Task1); // Delete the web server task to free resources

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
  Serial.println("Activating relay for " + String(ACTIVATION_MINUTES) + " minutes.");
  digitalWrite(RELAY_PIN, HIGH); // Activate the relay

  bool ledON = true;
  delay(ACTIVATION_MINUTES * 60 * 1000); // Stay awake for ACTIVATION_MINUTES minutes
  for (int i = 0; i < 60 * ACTIVATION_MINUTES; i++)
  {
    digitalWrite(RELAY_PIN, ledON ? LOW : HIGH);
    ledON = !ledON;
    delay(999); // Stay awake for ACTIVATION_MINUTES minutes
  }
  digitalWrite(RELAY_PIN, LOW); // Deactivate the relay
  Serial.println("Relay deactivated after " + String(ACTIVATION_MINUTES) + " minutes.");
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
void syncTimeWithNTP() {
  Serial.println("\nConnecting to WiFi for time sync...");
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
    } else {
      Serial.println("NTP time synchronization failed or timed out.");
    }
    WiFi.disconnect(true); // Disconnect from WiFi to save power
    Serial.println("WiFi disconnected.");
  } else {
    Serial.println("Failed to connect to WiFi hotspot.");
  }
  WiFi.mode(WIFI_OFF); // Turn off WiFi radio completely
}

void setWebServer(void *parameter)
{
  WiFiClient client = server.available();   // Listen for incoming clients

  long timeoutTime = 30000;
  if (client) {                             // If a new client connects,
    long currentTime = millis();
    long previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
                        
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>Orto Ariazzi - Dotti Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 26  
            client.println("<p>La Scheda si è attivata " + String(bootCount) + " volte</p>");
            client.println("<p>L'irrigazione della mattina è stata fatta: " + String(morningActivation?"si":"no") + "</p>");
            client.println("<p>L'irrigazione della sera è stata fatta: " + String(eveningActivation?"si":"no") + "</p>");
            
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}