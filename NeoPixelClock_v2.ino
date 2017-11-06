/*
   ESP8266 NeoPixel NTP Clock Program

    This hardware/software combination implements a digital clock that
    never needs setting as it gets the current time and date by polling
    Network Time Protocol (NTP) servers on the Internet. The clock's time
    is synchronized to NTP time every 5 minutes. Use of the TimeZone
    library means that Daylight Savings Time (DST) is automatically
    taken into consideration so no time change button is necessary.
    Clock always runs in 12 hour mode.

    in version 2.X a high level connection FSM is implemented to make
    sure clock recovers successfully from a power/wireless/Internet outage.

    The hardware consists of the following parts:
      NodeMCU Amica R2 from Electrodragon
      100 ohm 1/4w 5% resistor
      Adafruit 12 LED Neopixel Ring
      Adafruit 24 LED Neopixel Ring
      Adafruit  8 LED Neopixel Strip

    The software is built using the following:
      Uses Arduino IDE version 1.6.8
      Uses ESP8266-Arduino 2.2.0 addon
      Uses the NeoPixelBus library - https://github.com/Makuna/NeoPixelBus
      Uses the Time library - https://github.com/PaulStoffregen/Time
      Uses the TimeZone library - https://github.com/JChristensen/Timezone

    Concept, Design and Implementation by: Craig A. Lindley
    Version: 2.0
    Last update: 06/10/2016
*/

#include <ESP8266WiFi.h>
#include <Time.h>
#include <Timezone.h>
#include "NTP.h"

// ***************************************************************
// Start of user configuration items
// ***************************************************************

// Set your WiFi login credentials
#define WIFI_SSID "Moxie Java 2"
#define WIFI_PASS ""

// This clock is in the Mountain Time Zone
// Change this for your timezone
#define DST_TIMEZONE_OFFSET -7    // Day Light Saving Time offset (-6 is mountain time)
#define  ST_TIMEZONE_OFFSET -8    // Standard Time offset (-7 is mountain time)

// ***************************************************************
// TimeZone and Daylight Savings Time Rules
// ***************************************************************

// Define daylight savings time rules for the USA
TimeChangeRule myDST = {"MDT", Second, Sun, Mar, 2, DST_TIMEZONE_OFFSET * 60};
TimeChangeRule mySTD = {"MST", First,  Sun, Nov, 2,  ST_TIMEZONE_OFFSET * 60};
Timezone myTZ(myDST, mySTD);

// ***************************************************************
//  NeoPixelBus DMA Driver Instance
// ***************************************************************

// These are the offsets for the LEDs in the total PIXEL_COUNT
#define LARGE_RING_OFFSET  0
#define SMALL_RING_OFFSET 24
#define STRIP_OFFSET      36

// Total LED count: 12 (small ring) + 24 (large ring) + 8 (strip)
#define PIXEL_COUNT 44

#include "NeoPixelBus.h"
#include "NeoPixelBus_Driver.h"
#include "Colors.h"

// ***************************************************************
// Update Display Functions
// ***************************************************************

// Setup display colors
uint32_t BG_COLOR      = createColor(  0, 130,  80);
uint32_t SECONDS_COLOR = createColor(255, 215,   0);
uint32_t HOURS_COLOR   = createColor(255,   0,   0);
uint32_t MINUTES_COLOR = createColor(  0, 255,   0);
uint32_t DOW_COLOR     = createColor(255,   0,   0);
uint32_t MONTH_COLOR   = createColor(255, 255,   0);
uint32_t DAY_COLOR_1   = createColor(  0, 255,   0);
uint32_t DAY_COLOR_2   = createColor(  0, 255, 255);
uint32_t YEAR_COLOR    = createColor(  0,   0, 255);

// Display FSM operational states
enum DISPLAY_STATE {
  STATE_DISPATCH, STATE_RUNNING, STATE_EVENT10,
  STATE_EVENT15, STATE_EVENT30
};
// Current display state
enum DISPLAY_STATE displayState;

// Connection FSM operational states
enum CONNECTION_STATE {
  STATE_CHECK_WIFI_CONNECTION, STATE_WIFI_CONNECT,
  STATE_CHECK_INTERNET_CONNECTION, STATE_INTERNET_CONNECTED
};
// Current connection state
enum CONNECTION_STATE connectionState;

// Misc variables
time_t previousMinute = 0;
time_t minutes = 0;
int stripIndex = 0;
bool up = true;

// This function is called once a second
void updateDisplay(void) {

  const int STEPS = 64;
  const int PAUSE = 10;
  uint32_t color;

  TimeChangeRule *tcr;        // Pointer to the time change rule

  // Read the current UTC time from the NTP provider
  time_t utc = now();

  // Convert to local time taking DST into consideration
  time_t localTime = myTZ.toLocal(utc, &tcr);

  // Run finite state machine
  switch (displayState) {

    case STATE_DISPATCH:
      {
        minutes = minute(localTime);

        // Dispatch events in priority order
        // 30 minute event ?
        if ((minutes != previousMinute) && ((minutes % 30) == 0)) {
          displayState = STATE_EVENT30;
        }
        // 15 minute event ?
        else if ((minutes != previousMinute) && (minutes != 0) && ((minutes % 15) == 0)) {
          displayState = STATE_EVENT15;
        }
        // 10 minute event ?
        else if ((minutes != previousMinute) && (minutes != 0) && ((minutes % 10) == 0)) {
          displayState = STATE_EVENT10;
        }
        // Normal time display state
        else  {
          displayState = STATE_RUNNING;
        }
      }
      break;

    // Normal time display state
    case STATE_RUNNING:
      {
        // Set the clock's background color
        setAllPixels(BG_COLOR);

        // Map time to pixel positions
        int secondsPixel = LARGE_RING_OFFSET + map(second(localTime), 0, 59, 0, 23);
        int minutesPixel = LARGE_RING_OFFSET + map(minute(localTime), 0, 59, 0, 23);
        int hoursPixel   = SMALL_RING_OFFSET + ((hourFormat12(localTime) + 1) % 12);

        setPixelColor(secondsPixel, SECONDS_COLOR);
        setPixelColor(minutesPixel, MINUTES_COLOR);
        setPixelColor(hoursPixel,   HOURS_COLOR);

        // Clear strip
        for (int i = 0; i < 8; i++) {
          setPixelColor(STRIP_OFFSET + i, BG_COLOR);
        }
        // Draw moving seconds indicator. Color depends on time of day.
        if (isPM(localTime)) {
          setPixelColor(STRIP_OFFSET + stripIndex, SECONDS_COLOR);
        } else  {
          setPixelColor(STRIP_OFFSET + stripIndex, MINUTES_COLOR);
        }

        // Pixel moves left to right then right to left
        if (up) {
          stripIndex++;
          if (stripIndex == 8) {
            stripIndex = 6;
            up = false;
          }
        } else  {
          stripIndex--;
          if (stripIndex < 0) {
            stripIndex = 1;
            up = true;
          }
        }
        showPixels();

        displayState = STATE_DISPATCH;
      }
      break;

    // 10 minute event is rotating rainbow thru circular neopixel rings
    case STATE_EVENT10:
      {
        // Save current minute value
        previousMinute = minutes;

        rainbowCycle(300);

        displayState = STATE_DISPATCH;
      }
      break;

    // 15 minute event is setting led devices to different colors
    case STATE_EVENT15:
      {
        // Save current minute value
        previousMinute = minutes;

        for (int loop = 0; loop < 6; loop++) {
          clearAllPixels();

          for (int i = 0; i < STEPS; i++) {
            switch (loop % 3) {
              case 0:
                color = createColor(i * 4, 0, 0);
                break;
              case 1:
                color = createColor(0, i * 4, 0);
                break;
              case 2:
                color = createColor(0, 0, i * 4);
                break;
            }
            setSmallRingColor(color);
            delay(PAUSE);
          }

          for (int i = 63; i > 0; i--) {
            switch (loop % 3) {
              case 0:
                color = createColor(i * 4, 0, 0);
                break;
              case 1:
                color = createColor(0, i * 4, 0);
                break;
              case 2:
                color = createColor(0, 0, i * 4);
                break;
            }
            setSmallRingColor(color);
            delay(PAUSE);
          }

          for (int i = 0; i < STEPS; i++) {
            switch (loop % 3) {
              case 0:
                color = createColor(0, i * 4, 0);
                break;
              case 1:
                color = createColor(0, 0, i * 4);
                break;
              case 2:
                color = createColor(i * 4, 0, 0);
                break;
            }
            setLargeRingColor(color);
            delay(PAUSE);
          }

          for (int i = 63; i > 0; i--) {
            switch (loop % 3) {
              case 0:
                color = createColor(0, i * 4, 0);
                break;
              case 1:
                color = createColor(0, 0, i * 4);
                break;
              case 2:
                color = createColor(i * 4, 0, 0);
                break;
            }
            setLargeRingColor(color);
            delay(PAUSE);
          }

          for (int i = 0; i < STEPS; i++) {
            switch (loop % 3) {
              case 0:
                color = createColor(0, 0, i * 4);
                break;
              case 1:
                color = createColor(i * 4, 0, 0);
                break;
              case 2:
                color = createColor(0, i * 4, 0);
                break;
            }
            setStripColor(color);
            delay(PAUSE);
          }

          for (int i = 63; i > 0; i--) {
            switch (loop % 3) {
              case 0:
                color = createColor(0, 0, i * 4);
                break;
              case 1:
                color = createColor(i * 4, 0, 0);
                break;
              case 2:
                color = createColor(0, i * 4, 0);
                break;
            }
            setStripColor(color);
            delay(PAUSE);
          }
          displayState = STATE_DISPATCH;
        }
      }
      break;

    // Event fires every thirty minutes
    // Displays date
    case STATE_EVENT30:
      {
        // Save current minute value
        previousMinute = minutes;

        // Index of left most LED in large NeoPixel ring
        const int FIRST_LED_NUM = 18;
        int ledNum;

        clearAllPixels();

        // Display day of the week 1 .. 7 - Sunday is day 1
        // Set the DOW indicator
        setPixelColor(STRIP_OFFSET,     DOW_COLOR);
        setPixelColor(STRIP_OFFSET + 1, DOW_COLOR);
        // Set inner ring to same color
        for (int i = 0; i < 12; i++) {
          setPixelColor(SMALL_RING_OFFSET + i, DOW_COLOR);
        }
        showPixels();

        // Get the day of the week
        int dow = weekday(localTime);
        ledNum = FIRST_LED_NUM;
        for (int i = 0; i < dow; i++) {
          setPixelColor(ledNum, DOW_COLOR);
          showPixels();
          delay(400);
          ledNum += 2;
          ledNum %= 24;
        }
        delay(2000);

        clearAllPixels();

        // Display month of year 1 .. 12 - January is month 1
        // Set the MONTH indicator
        setPixelColor(STRIP_OFFSET + 2, MONTH_COLOR);
        setPixelColor(STRIP_OFFSET + 3, MONTH_COLOR);
        // Set inner ring to same color
        for (int i = 0; i < 12; i++) {
          setPixelColor(SMALL_RING_OFFSET + i, MONTH_COLOR);
        }
        showPixels();

        // Get the month of the year
        int mon = month(localTime);
        ledNum = FIRST_LED_NUM;
        for (int i = 0; i < mon; i++) {
          setPixelColor(ledNum, MONTH_COLOR);
          showPixels();
          delay(400);
          ledNum++;
          ledNum %= 24;
        }
        delay(2000);

        clearAllPixels();

        // Display day of month 1 .. 31
        // Set the DAY indicator
        setPixelColor(STRIP_OFFSET + 4, DAY_COLOR_1);
        setPixelColor(STRIP_OFFSET + 5, DAY_COLOR_1);
        // Set inner ring to same color
        for (int i = 0; i < 12; i++) {
          setPixelColor(SMALL_RING_OFFSET + i, DAY_COLOR_1);
        }
        showPixels();

        // Get the month of the year
        int d, d1;
        d = d1 = day(localTime);
        if (d > 24) {
          d = 24;
        }
        ledNum = FIRST_LED_NUM;
        for (int i = 0; i < d; i++) {
          setPixelColor(ledNum, DAY_COLOR_1);
          showPixels();
          delay(400);
          ledNum++;
          ledNum %= 24;
        }
        if (d1 > 24) {
          d = d1 - 24;
          ledNum = FIRST_LED_NUM;
          for (int i = 0; i < d; i++) {
            setPixelColor(ledNum, DAY_COLOR_2);
            showPixels();
            delay(400);
            ledNum++;
            ledNum %= 24;
          }
        }
        delay(2000);

        clearAllPixels();

        // Display year - 2000 e.g. 2016 = 16
        // Set the YEAR indicator
        setPixelColor(STRIP_OFFSET + 6, YEAR_COLOR);
        setPixelColor(STRIP_OFFSET + 7, YEAR_COLOR);
        // Set inner ring to same color
        for (int i = 0; i < 12; i++) {
          setPixelColor(SMALL_RING_OFFSET + i, YEAR_COLOR);
        }
        showPixels();

        // Get the month of the year
        int y = year(localTime) - 2000;
        ledNum = FIRST_LED_NUM;
        for (int i = 0; i < y; i++) {
          setPixelColor(ledNum, YEAR_COLOR);
          showPixels();
          delay(400);
          ledNum++;
          ledNum %= 24;
        }
        delay(2000);

        displayState = STATE_DISPATCH;
      }
      break;
  }
}

// ***************************************************************
// Program setup
// ***************************************************************

void setup() {

  Serial.begin(115200);
  delay(1000);

  // Set overall LED brightness
  setBrightness(0.2);

  // Initialize the led pixel driver
  initPixels();

  // Set the initial connection state
  connectionState = STATE_CHECK_WIFI_CONNECTION;

  // Set the initial display state
  displayState = STATE_DISPATCH;
}

// ***************************************************************
// Program loop
// ***************************************************************

// Previous seconds value
time_t previousSecond = 0;
boolean ntpInitialized = false;

void loop() {

  // Run the connection FSM to make sure clock reconnects to
  // Wifi network after power failures.
  switch (connectionState) {

    // Check the WiFi connection
    case STATE_CHECK_WIFI_CONNECTION:
      //      Serial.printf("\nState: check WiFi connection\n");

      // Are we connected ?
      if (WiFi.status() != WL_CONNECTED) {
        // Wifi is NOT connected
        connectionState = STATE_WIFI_CONNECT;
      } else  {
        // Wifi is connected so check Internet
        connectionState = STATE_CHECK_INTERNET_CONNECTION;
      }
      break;

    // No Wifi so attempt WiFi connection
    case STATE_WIFI_CONNECT:
      {
        //        Serial.printf("\nState: Wifi connect\n");

        // Clear all Neo Pixels
        clearAllPixels();

        // Indicate NTP no yet initialized
        ntpInitialized = false;

        // Set station mode
        WiFi.mode(WIFI_STA);

        // Start connection process
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        Serial.printf("\nAttempting WiFi connection to: %s\n", WIFI_SSID);

        // Initialize iteration counter
        int i = 0;

        while ((WiFi.status() != WL_CONNECTED) && (i++ < 30)) {
          delay(400);
          Serial.printf(".");
        }
        if (i == 31) {
          Serial.printf("\nCould not connect to WiFi\n");
          delay(100);
          break;
        }
        Serial.printf("\nConnected to WiFi\n\n");
        connectionState = STATE_CHECK_INTERNET_CONNECTION;
      }
      break;

    case STATE_CHECK_INTERNET_CONNECTION:
      //      Serial.printf("\nState: check Internet connection\n");

      // Do we have a connection to the Internet ?
      if (checkInternetConnection()) {
        // We have an Internet connection
        if (! ntpInitialized) {
          // We are connected to the Internet for the first time so set NTP provider
          initNTP();

          ntpInitialized = true;

          Serial.printf("\nConnected to the Internet\n");
        }
        connectionState = STATE_INTERNET_CONNECTED;
      } else  {
        connectionState = STATE_CHECK_WIFI_CONNECTION;
        delay(100);
      }
      break;

    // Connected so run clock
    case STATE_INTERNET_CONNECTED:
      //      Serial.printf("\nState: internet connected\n");

      // Update the display only if time has changed
      if (timeStatus() != timeNotSet) {
        if (second() != previousSecond) {
          previousSecond = second();

          // Update the display
          updateDisplay();
        }
      }
      delay(100);

      // Set next connection state
      connectionState = STATE_CHECK_WIFI_CONNECTION;
      break;
  }
}


