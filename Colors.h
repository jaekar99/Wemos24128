/*
   ESP8266 NeoPixel NTP Clock Program

    Colors.h - Color Functions

    Concept, Design and Implementation by: Craig A. Lindley
    Last update: 06/10/2016
*/

#ifndef COLORS_H
#define COLORS_H

// Create a 24 bit color from its RGB components
uint32_t createColor(int red, int green, int blue) {
  uint32_t color = red;
  color <<= 8;
  color |= green;
  color <<= 8;
  color |= blue;
  return color;
}

void setLargeRingColor(uint32_t color) {
  for (int i = 0; i < 24; i++) {
    setPixelColor(LARGE_RING_OFFSET + i, color);
  }
  showPixels();
}

void setSmallRingColor(uint32_t color) {
  for (int i = 0; i < 12; i++) {
    setPixelColor(SMALL_RING_OFFSET + i, color);
  }
  showPixels();
}

void setStripColor(uint32_t color) {
  for (int i = 0; i < 8; i++) {
    setPixelColor(STRIP_OFFSET + i, color);
  }
  showPixels();
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t wheel(byte wheelPos) {
  if (wheelPos < 85) {
    return createColor(wheelPos * 3, 255 - wheelPos * 3, 0);
  } else if (wheelPos < 170) {
    wheelPos -= 85;
    return createColor(255 - wheelPos * 3, 0, wheelPos * 3);
  } else {
    wheelPos -= 170;
    return createColor(0, wheelPos * 3, 255 - wheelPos * 3);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 3; j++) { // Three cycles of all colors on wheel
    for (i = 0; i < PIXEL_COUNT; i++) {
      setPixelColor(i, wheel(((i * 256 / PIXEL_COUNT) + j) & 255));
    }
    showPixels();
    delay(wait);
  }
}

#endif


