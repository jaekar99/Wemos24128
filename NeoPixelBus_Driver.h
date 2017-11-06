/*
   ESP8266 NeoPixel NTP Clock Program

    NeoPixelBus.h - NeoPixel Bus Library Interface Functions

    Concept, Design and Implementation by: Craig A. Lindley
    Last Update: 06/10/2016
*/

#ifndef NEOPIXELBUS_DRIVER
#define NEOPIXELBUS_DRIVER

#include <NeoPixelBus.h>

static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PIXEL_COUNT, 0);
static NeoGamma<NeoGammaTableMethod> colorGamma;

static float brightness;

// Set the overall brightness of the NeoPixel LEDs
// as they are extremely bright
void setBrightness(float value) {
  value = (value < 0.01) ? 0.01 : value;
  value = (value > 0.8)  ? 0.8 : value;
  brightness = value;
}

void initPixels(void) {
  // Resets all the NeoPixels to an off state
  strip.Begin();
  strip.Show();
}

void setPixelColor(int num, uint32_t color) {
  RgbColor rgbColor;

  rgbColor.B = ((color & 0xFF) * brightness);
  color >>= 8;
  rgbColor.G = ((color & 0xFF) * brightness);
  color >>= 8;
  rgbColor.R = ((color & 0xFF) * brightness);

  // Gamma correct the specified color
  rgbColor = colorGamma.Correct(rgbColor);
  strip.SetPixelColor(num, rgbColor);
}

// Clear all of the pixels
void clearAllPixels(void) {
  for (int i = 0; i < PIXEL_COUNT; i++) {
    setPixelColor(i, 0);
  }
  strip.Show();
}

// Set all of the pixels to specified color
void setAllPixels(uint32_t color) {
  for (int i = 0; i < PIXEL_COUNT; i++) {
    setPixelColor(i, color);
  }
  strip.Show();
}

// Make pixel changes visible
void showPixels(void) {
  strip.Show();
}

#endif

