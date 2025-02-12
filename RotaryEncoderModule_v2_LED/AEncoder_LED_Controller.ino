/************************************************************
 * Minimal SENDER TEENSY CODE
 * 
 * Responsibilities:
 *   1) Calculates the bar index from EncoderPos.
 *   2) Sends it (plus decoupling flag) via Serial5.
 *   3) No LED logic, no virtual logic, minimal updates.
 * 
 * Wiring (Serial5):
 *   - Sender (REM) TX5 (pin 33) -> Receiver (EXT_Teensy) RX5 (pin 31) 
 *   - Sender (REM) RX5 (pin 34) -> Receiver (EXT_Teensy) TX5 (pin 32)
 *   - Shared GND
 ************************************************************/

/* ---------- PROTOCOL (Packet) DEFINITION ---------- */
static const uint8_t START_BYTE = 0x55;
static const uint8_t END_BYTE   = 0x11;

/* ---------- GEOMETRY CONSTANTS ---------- */
#define PPR               1024
#define NUM_PANELS        4
#define NUM_VIRTUAL_PANELS 1
#define PANEL_WIDTH       16
#define PANEL_HEIGHT      16
#define NUM_BARS          (NUM_PANELS * PANEL_WIDTH) // 64
#define NUM_VIRTUAL_BARS  (NUM_VIRTUAL_PANELS * PANEL_WIDTH)
#define W                 1

//#define DEBUG

/* ----------
   EXTERNAL variables (assumed updated elsewhere):
      EncoderPos       (range ~ -wrapPoint..+wrapPoint)
      wrapPoint        (e.g., 2048)
      wrapPointInverse (e.g., -2048)
      decouplingflag   (0 or 1)
---------- */
extern int16_t EncoderPos;
extern int16_t wrapPoint;
extern int16_t wrapPointInverse;
extern int     decouplingflag;

/* ---------- PROTOTYPES ---------- */
static void sendIndexOverSerial5(int16_t ledIndex, bool decoupled);

/* 
   Track the last index we sent so we only update
   the receiver when something changes.
*/
static int16_t lastIndex = 9999; // an impossible sentinel

/* -----------------------------------------------------
   setupLED: Initialize Serial5 (no LED logic here)
------------------------------------------------------*/
void setupLED() {
  #ifdef DEBUG
    Serial.begin(115200);
    Serial.println("Sender: setupLED()");
  #endif

  // Initialize Serial5 for communication
  Serial5.begin(115200);

  // Initialize lastIndex if desired
  lastIndex = 9999;
}

/* -----------------------------------------------------
   loopLED: 
   1) Checks decoupling.
   2) Maps EncoderPos => ledIndex in normal mode.
   3) Sends only if there's a change.
------------------------------------------------------*/
void loopLED() {
  // If decoupled, send a special index = -1, but only once
  if (decouplingflag == 1) {
    if (lastIndex != -1) {
      // We just switched to decoupled
      #ifdef DEBUG
        Serial.println("Sender: decoupled -> sending -1");
      #endif
      sendIndexOverSerial5(-1, true);
      lastIndex = -1;  // So we won't resend until decouplingflag goes 0 again
    }
    // No further normal index calculations in decoupled mode
    return;
  }

  // Normal operation: map the EncoderPos to a LED bar index
  int16_t ledIndex = map(
    EncoderPos,
    -wrapPointInverse,  // typically +2048
    -wrapPoint,         // typically -2048
     0, 
    (NUM_BARS + NUM_VIRTUAL_BARS - 1)
  );

  // Optional offset if you want to center the line visually
  ledIndex -= (PANEL_WIDTH + W) / 2;

  // If the new index is different, send it
  if (ledIndex != lastIndex) {
    #ifdef DEBUG
      Serial.print("Sender: ledIndex changed -> ");
      Serial.println(ledIndex);
    #endif
    sendIndexOverSerial5(ledIndex, false);
    lastIndex = ledIndex;
  }
}

/* -----------------------------------------------------
   sendIndexOverSerial5: 
   Sends 1 start byte, 1 decoupled byte, 2 index bytes,
   1 end byte.
------------------------------------------------------*/
static void sendIndexOverSerial5(int16_t ledIndex, bool decoupled) {
  Serial5.write(START_BYTE);              // 0x55
  Serial5.write(decoupled ? 1 : 0);       // decoupled flag (1 or 0)

  // ledIndex as 16-bit signed
  uint8_t highByte = (uint8_t)((ledIndex >> 8) & 0xFF);
  uint8_t lowByte  = (uint8_t)(ledIndex & 0xFF);

  Serial5.write(highByte);
  Serial5.write(lowByte);
  Serial5.write(END_BYTE);                // 0x11

  #ifdef DEBUG
    Serial.print("Sender: Packet sent -> decoupled=");
    Serial.print(decoupled ? 1 : 0);
    Serial.print(", index=");
    Serial.println(ledIndex);
  #endif
}
