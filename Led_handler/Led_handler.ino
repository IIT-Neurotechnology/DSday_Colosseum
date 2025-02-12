/*********************************************************
 * RECEIVER for a 5-byte packet:
 *   [0x55][decoupled][hiIndex][loIndex][0x11]
 *
 * If SCREENSAVER is defined, it will continuously
 * animate a "bouncing line" until the *first* valid
 * serial packet arrives, at which point normal operation
 * begins.
 *********************************************************/
#include <Arduino.h>
#include <FastLED.h>

/* ---------- SCREENSAVER Feature Flag ---------- */
// Comment this out if you *don't* want a screensaver
//#define SCREENSAVER

#ifdef SCREENSAVER
bool screensaverActive = true;  // start in screensaver mode
#endif

/* ---------- CONFIG & GEOMETRY ---------- */
#define NUM_PANELS 4
#define PANEL_WIDTH 16
#define PANEL_HEIGHT 16
#define W 2
#define NUM_BARS (NUM_PANELS * PANEL_WIDTH) // 64
#define NUM_LEDS (PANEL_WIDTH * PANEL_HEIGHT * NUM_PANELS)
#define BRIGHTNESS 200

#define DATA_PIN 18
#define DATA_PIN2 19

#define NUM_STRIPS 2
#define LEDS_PER_STRIP (NUM_LEDS / NUM_STRIPS)

/************* LED ARRAYS ****************/
CRGB ledsA[LEDS_PER_STRIP];
CRGB ledsB[LEDS_PER_STRIP];

/************* PACKET DEFINES ************/
static const uint8_t START_BYTE = 0x55;
static const uint8_t END_BYTE   = 0x11;

/************* PACKET STATE **************/
bool    decoupled    = true;
int16_t currentIndex = 0;  // note it's signed

/************* INTERNAL STATE ************/
int  prevIndex = 10000; 
int  cdecoup   = 0;

/**********************************************************************
 * setLED: Utility to set a single LED by global index
 **********************************************************************/
void setLED(int idx, const CRGB &color) {
  if (idx < 0 || idx >= NUM_LEDS) return;
  if (idx < LEDS_PER_STRIP) {
    ledsA[idx] = color;
  } else {
    ledsB[idx - LEDS_PER_STRIP] = color;
  }
}

/**********************************************************************
 * LEDLine Class
 **********************************************************************/
class LEDLine {
public:
  LEDLine(int startIndex, CRGB color = CRGB::White)
    : m_start(startIndex), m_color(color)
  {
    m_length = PANEL_HEIGHT * W; 
  }

  void setStart(int startIndex) {
    m_start = startIndex;
  }

  void setColor(const CRGB& color) {
    m_color = color;
  }

  void draw() {
    for (int i = 0; i < m_length; i++) {
      int ledIndex = (m_start * PANEL_HEIGHT) + i;
      // wrap safely
      ledIndex = (ledIndex + NUM_LEDS) % NUM_LEDS;
      setLED(ledIndex, m_color);
    }
  }

  void clear() {
    for (int i = 0; i < m_length; i++) {
      int ledIndex = (m_start * PANEL_HEIGHT) + i;
      ledIndex = (ledIndex + NUM_LEDS) % NUM_LEDS;
      setLED(ledIndex, CRGB::Black);
    }
  }

  void clearAll() {
    // Clears entire array
    for (int i = 0; i < LEDS_PER_STRIP; i++) {
      ledsA[i] = CRGB::Black;
      ledsB[i] = CRGB::Black;
    }
  }

private:
  int  m_start;   
  int  m_length;  
  CRGB m_color;
};

CRGB lineColor = CRGB::Green;
LEDLine line(0, lineColor);
LEDLine oldline(prevIndex, CRGB::Black);

/**********************************************************************
 * screensaverAnimationStep():
 *  A single "step" of a bouncing line, so that we can
 *  check for new serial data on each iteration.
 **********************************************************************/
#ifdef SCREENSAVER
void screensaverAnimationStep() {
  static int bar       = 0;
  static int direction = 1;  // +1 forward, -1 backward

  // Clear everything
  FastLED.clear();

  // Draw a line at "bar"
  line.setStart(bar);
  line.draw();

  FastLED.show();

  // Move "bar" for next time
  bar += direction;
  // Bounce if we reach the extremes
  if (bar >= NUM_BARS) {
    bar = NUM_BARS - 1;
    direction = -1;
  } else if (bar < 0) {
    bar = 0;
    direction = 1;
  }
}
#endif

/**********************************************************************
 * setup()
 **********************************************************************/
void setup() {
  Serial.begin(115200);
  Serial.println("Receiver: Setup start");

  // Listen on Serial5
  Serial5.begin(115200);

  // FastLED setup
  FastLED.addLeds<NEOPIXEL, DATA_PIN >(ledsA, LEDS_PER_STRIP);
  FastLED.addLeds<NEOPIXEL, DATA_PIN2>(ledsB, LEDS_PER_STRIP);
  FastLED.setBrightness(BRIGHTNESS);

  FastLED.clear();
  FastLED.show();

#ifdef SCREENSAVER
  Serial.println("Screensaver Mode: will continue until first valid packet...");
#else
  Serial.println("No Screensaver Mode: normal operation");
#endif

  Serial.println("Receiver: Setup complete");
}

/**********************************************************************
 * loop()
 **********************************************************************/
void loop() {

#ifdef SCREENSAVER
  // If we're still in screensaver mode, check if a packet arrives
  if (screensaverActive) {
    // Attempt to read the first valid packet
    while (Serial5.available() >= 5) {
      if (Serial5.peek() == START_BYTE) {
        // If we read a valid packet, disable screensaver
        if (readPacket()) {
          screensaverActive = false;
          Serial.println("Screensaver disabled: Received first valid packet!");
          // Act on that packet
          FastLED.clear();
          updateLEDs();
          return; // done for this loop
        } else {
          // readPacket() failed, discard 1 byte to resync
          Serial5.read();
        }
      } else {
        // Not START_BYTE, discard 1 byte
        Serial5.read();
      }
    }

    // If we get here, no valid packet arrived this loop iteration
    // => keep running the screensaver
    screensaverAnimationStep();
    return; // end loop() here so we don't do normal logic below
  }
#endif

  // ----- Normal, Non-Screensaver Operation -----

  bool gotNewPacket = false;  // track if we got at least one valid packet

  // Keep trying to read as long as there's enough data for a 5-byte packet
  while (Serial5.available() >= 5) {
    // If the next byte is our START_BYTE, attempt to read the packet
    if (Serial5.peek() == START_BYTE) {
      if (readPacket()) {
        // We successfully read a 5-byte packet
        gotNewPacket = true;
      } else {
        // If readPacket() failed, discard one byte to resync
        Serial5.read();
      }
    } else {
      // If it's not START_BYTE, discard one byte
      Serial5.read();
    }
  }

  // Only update LEDs after we've read *all* pending packets
  if (gotNewPacket) {
    updateLEDs();
  }
}

/**********************************************************************
 * readPacket(): Parse the 5-byte message
 **********************************************************************/
bool readPacket() {
  if (Serial5.available() < 5) return false;

  // 1) start
  uint8_t start = Serial5.read();
  if (start != START_BYTE) return false;

  // 2) decoupled
  decoupled = (Serial5.read() == 1);

  // 3-4) hi & lo for currentIndex
  uint8_t hi = Serial5.read();
  uint8_t lo = Serial5.read();
  currentIndex = (int16_t)((hi << 8) | lo);

  // 5) end
  uint8_t end = Serial5.read();
  if (end != END_BYTE) return false;

  Serial.print("Receiver: Packet -> decoupled=");
  Serial.print(decoupled);
  Serial.print(", index=");
  Serial.println(currentIndex);

  return true;
}

/**********************************************************************
 * updateLEDs(): replicate your old logic
 **********************************************************************/
void updateLEDs() {
  // decoupled?
  if (decoupled) {
    Serial.print("Receiver: normal update => index=");
    Serial.println(currentIndex);
    line.setStart(currentIndex);
    line.draw();
    FastLED.show();
    prevIndex = currentIndex;
    if (cdecoup == 0) {
      Serial.println("Receiver: Decoupled => clearing all LEDs");
      line.clearAll();
      FastLED.show();
      cdecoup++;
      prevIndex = 10000;
    }
    return;
  }

  // normal mode
  if (currentIndex == prevIndex) {
    // no new index
    return;
  } 
  else if (currentIndex < -(W - 1) || currentIndex > (NUM_BARS - W+1)) {
    Serial.print("Receiver: line wraps or out of range => index=");
    Serial.println(currentIndex);

    line.setStart(NUM_BARS - 1);
    line.draw();
    line.setStart(-1);
    line.draw();
    FastLED.show();
  } 
  else {
    Serial.print("Receiver: normal update => index=");
    Serial.println(currentIndex);

    oldline.setStart(NUM_BARS - 1);
    oldline.draw();
    oldline.setStart(0);
    oldline.draw();

// (2) Erase old line from prevIndex (draw black)
    oldline.setStart(prevIndex);
    oldline.draw();

// (3) Draw new line
    line.setStart(currentIndex);
    line.draw();

    FastLED.show();
    prevIndex = currentIndex;
    cdecoup   = 0;
  }
}
