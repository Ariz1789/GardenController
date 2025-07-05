#include <ESP32Time.h>


ESP32Time rtc(3600); // Initial offset. If you use configTzFile, this might become less critical for direct rtc.adjust


bool syncTimeWithNTP();

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