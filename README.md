# BPOD-based Circular LED COLOSSEUM with Rotary Encoder Control

Render            |  Real
:-------------------------:|:-------------------------:
<img width="759" alt="ed130eb8-e9cf-46bc-b179-3c29f74139bb" src="https://github.com/user-attachments/assets/8c2bfa3c-4272-426c-a526-0a7a45d237e5" /> | ![IMG_E566E174E43D-1](https://github.com/user-attachments/assets/663ee327-6ebf-47c2-a723-3dfb6b113a29)

This system integrates a **BPOD** behavioral platform with a **circular LED display** and a **high-resolution rotary encoder**, creating a three-dimensional "colosseum-like" environment for a mouse in a neuroscience experiment. The mouse sits at the center of a 3D-printed, circular arena, surrounded by flexible LED panels. By turning a rotary encoder, the mouse effectively rotates a luminous vertical bar around the "walls" of this arena. Over time, the mouse can learn to anticipate the position of this bar, providing insights into spatial navigation and cognitive mapping.

---

## Hardware Components

- **BPOD Module**  
  Controls experimental states, rewards, and logs events. It synchronizes with the Teensy-based rotary encoder module and the LED display.

- **Rotary Encoder (1024 PPR)**  
  Translates the mouse's rotational input into LED position updates. The ratio `N` defines how many full rotations of the encoder correspond to a complete LED revolution. For example, `N = 2` means two wheel rotations produce one full LED ring rotation.

- **Flexible Circular LED Panels (WS2812B)**  
  - Four 16×16 LED matrices (each containing WS2812B LEDs) arranged in a 2×2 layout, forming a 64×16 (1024 LED) array.  
  - These are curved into a circular shape inside a 3D-printed enclosure.  
  - Each WS2812B LED needs ~1.25 µs per bit, **24 bits** (3 bytes for RGB), plus a ~50 µs reset at the end of the data stream.

- **BPOD Rotary Encoder Module (Teensy-based)**  
  A **Teensy** microcontroller reads the rotary encoder, calculates LED positions, and **sends** data over serial if required. A second Teensy can then handle the actual LED updates, preventing timing conflicts.

---

## Additional Teensy 3.6 for LED Control

To handle the large number of LEDs (1024 WS2812Bs) efficiently, a **second Teensy 3.6** is used **exclusively** for LED updates:

- **Faster LED Updates with FastLED**  
  - The second Teensy runs the **FastLED** library, which manages precise timing for WS2812B signals (~1.25 µs/bit).  
  - This Teensy listens on **Serial5** for packets containing LED bar indices and color commands.  
  - By splitting tasks across two microcontrollers, the rotary encoder readings remain accurate, and LED updates remain flicker-free.

- **Offloading CPU Load**  
  - The first Teensy (rotary encoder module) only sends position data to the second Teensy.  
  - This separation ensures smoother operation, as the CPU-intensive LED data updates occur on a dedicated microcontroller.

- **Two Data Pins for Higher Throughput**  
  - The second Teensy 3.6 drives the LEDs on **pins 18 and 19**, effectively splitting the 1024 LEDs into two parallel strips.  
  - Each LED requires 24 bits of data plus a ~50 µs reset at the end of the data frame. Driving two strips simultaneously nearly halves the total transmission time.

### Approximate Update Frequency Calculation

- **Single-Strip Transmission Time**  
  - Each LED needs 24 bits × 1.25 µs = 30 µs of transmission time per LED.  
  - For 1024 LEDs, total data time = 1024 × 30 µs = 30,720 µs, plus ~50 µs reset ≈ **30.77 ms** per frame.  
  - That’s about **32.5 fps** (frames per second) if you only had a single strip.

- **Two Strips in Parallel**  
  - If you split the 1024 LEDs into two equal strips, each strip updates 512 LEDs in parallel.  
  - Transmission time per strip is roughly 512 × 30 µs = 15,360 µs, plus ~50 µs ≈ **15.41 ms**.  
  - This yields around **65 fps** as a theoretical maximum, assuming continuous updates.

---
![397879740-6fc6f9c6-3a99-4fa2-a4de-f5ea3d144b3a](https://github.com/user-attachments/assets/cb4c680b-034b-4676-8e4e-ab40f9b38491)


## Software Components

- **BPOD State Machine**  
  Defines trial structures, handles reward timing, and synchronizes LED updates with behavioral events. The LED state can be triggered at certain times or after specific encoder changes, all logged by BPOD.

- **Teensy Firmware (C++ with FastLED)**  
  - **First Teensy** (Encoder Module)  
    Reads the rotary encoder and sends serial messages (via **Serial5**) containing updated LED positions or commands.  
  - **Second Teensy** (Dedicated LED Driver)  
    Receives data on **Serial5**, updates the WS2812B strips on pins 18 and 19, and ensures precise timing.

---

## Functionality

1. **Encoder-to-LED Mapping**  
   The encoder’s angular range maps onto the LED indices forming a circular ring. Each bar corresponds to a specific angular segment. Changing `N` modifies how many full turns of the encoder produce one full 360° LED sweep.

2. **Cylindrical Illusion**  
   The curved LED panels create a wraparound environment, giving the mouse the impression of a cylindrical “colosseum.” Rotating the encoder moves the illuminated bar around the circumference.

3. **Cognitive Task**  
   Mice can learn to anticipate the bar’s position, forming an internal “cognitive map” of the environment. Researchers can adjust difficulty by changing `N`, the **bar width** (`W`), or brightness.

4. **Minimal Updates**  
   Instead of resetting all 1024 LEDs every loop, the firmware switches off only the old bar and lights the new one. This approach minimizes computation, reduces flicker, and helps maintain brightness consistency.

---

## Setup & Calibration

1. **Hardware Assembly**  
   - 3D print the circular enclosure (e.g., on a Prusa) to hold four flexible 16×16 LED panels.  
   - Mount the LED panels around the inner circumference to form a 64×16 (1024 LED) array.  
   - Attach the rotary encoder, ensuring stable mechanical coupling for the mouse’s rotation.  
   - Use one Teensy for the rotary encoder and another Teensy 3.6 (with FastLED) for LED data.  
   - Provide sufficient power and stable wiring (WS2812B can draw significant current).

2. **Firmware & Parameters**  
   - Load the Teensy firmware for the encoder module (handles reading and serial output).  
   - Load the Teensy firmware for the LED driver (handles incoming serial commands and drives LEDs).  
   - Confirm `NUM_LEDS = 1024` (or your actual total), and that two strips of 512 LEDs are mapped to pins 18 & 19.  
   - Set `N` for the desired encoder rotations per 360° sweep. Adjust `W` (the bar width in LEDs) and LED brightness to tune performance and difficulty.

3. **BPOD Integration**  
   - Configure BPOD’s state machine to detect encoder triggers and LED transitions.  
   - Optionally deliver rewards when certain encoder conditions are met.  
   - Use BPOD’s data logging to correlate behavior, encoder events, and LED states.

---

## Maintenance & Troubleshooting

- **LED/Encoder Alignment**  
  If LED positions don’t match the encoder reading, verify the index mapping and `N` factor.  
- **Power Supply**  
  Flickering or dim LEDs may indicate insufficient power or significant voltage drop. Use robust wiring and/or inject power at multiple points.  
- **Frame Rate Optimization**  
  If you need faster updates, switch to 4 pins to achieve a faster parallel communication with the LEDs
- **Serial Communication**  
  Ensure the second Teensy is receiving data correctly on **Serial5** (check baud rate, TX/RX pins).

---

## Conclusion

This integrated system provides a precise, stable, and configurable visual environment for neuroscience experiments. By combining a **BPOD** state machine, a **high-resolution rotary encoder**, and **two Teensy 3.6** boards (one exclusively handling **FastLED** updates), researchers can study spatial cognition in rodents. Efficient partial‐update LED control ensures smooth visuals, aiding in the formation of robust internal cognitive maps in the mouse’s brain.
