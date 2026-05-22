# CapstoneClawMachine
Claw Machine code for a mini claw machine built around an Arduino, CNC Shield motion control, NEMA 17 steppers, and a dual-servo claw.

## Operating Instructions
When the machine/program first initializes, the current gantry position is treated as **"zero"**. This **zero** is the prize drop position that the machine returns to after every grab cycle.

### Gameplay Loop
1. Power on the machine.
2. Move the claw over the prize chute location.
3. Set zero:
   - Press the **joystick switch once** (first SW press behavior), or
   - Press the dedicated **zero button** inside the machine.
4. Move over a prize using the joystick.
5. Press the joystick switch again to run the claw cycle.
6. The machine lowers, grabs, raises, returns to zero, and releases.

> ⚠️ There are currently no limit switches enforced in software. Avoid driving the gantry into hard stops.

---

## Development Guide

### Repository Contents
- `Claw_Code.ino` — main firmware sketch.
- `README.md` — project overview and setup/usage/development notes.

### Software Architecture (Current Firmware)
The firmware is structured around a tight `loop()` with three recurring tasks:
1. `checkZeroButton()` — debounced polling for the dedicated zero button (`D12`).
2. `readJoystickAndMove()` — joystick read + single-axis movement + SW-trigger handling.
3. `maybePrintPosition()` — throttled serial telemetry for tracked position.

Key behavior modules:
- **Motion control**
  - `stepBothMotors()` handles synchronized X/Y stepping for H-bot style movement.
  - `stepMotor()` handles single-motor stepping (currently used by Z only).
- **State + coordinate tracking**
  - `posX`, `posY`, `posZ` track software step coordinates relative to zero.
  - `doZero()` resets all coordinates to `(0,0,0)`.
  - `returnToZero()` drives X then Y back to zero after a grab.
- **Claw actuation**
  - `clawOpen()` and `clawClose()` coordinate dual-servo positions.
  - `runClawSequence()` performs the full capture/deposit routine.
- **Input reliability**
  - Debounce logic is implemented for both the zero button and joystick switch.
  - Joystick movement uses hysteresis thresholds to reduce jitter around center.

### Control Mapping
#### Inputs
- `A0` (`pinVRx`) — joystick X analog input.
- `A1` (`pinVRy`) — joystick Y analog input.
- `A2` (`pinSW`) — joystick switch (first press = zero; later presses = claw sequence).
- `D12` (`pinButton`) — dedicated zero button.

#### Stepper Driver Pins (CNC Shield)
- X axis: `STEP D2`, `DIR D5`
- Y axis: `STEP D4`, `DIR D7`
- Z axis: `STEP D3`, `DIR D6`
- Enable: `D8` (set `LOW` in setup to enable drivers)

#### Servo Pins
- Left claw servo: `D13`
- Right claw servo: `A3`

### Motion + Sequence Behavior
- Joystick motion is intentionally **single-axis only** (no diagonal movement).
- Direction activation thresholds:
  - Active when analog value passes `>700` or `<300`.
  - Releases when returning inside `600/400` hysteresis bounds.
- Claw sequence on trigger:
  1. Lower Z by 180 steps.
  2. Close claw.
  3. Raise Z by 180 steps.
  4. Return X/Y to software zero.
  5. Open claw.


### Serial Output / Debugging
Startup banner confirms control mappings and initial position report.
Typical useful log markers:
- `>>> ZEROED: position reset to (0, 0, 0) <<<`
- `--- CLAW TRIGGERED ---`
- `--- CLAW DONE, RETURNING ---`
- `--- AT ZERO ---`
- `--- SEQUENCE COMPLETE ---`

Use these events with position prints (`POS X/Y/Z`) to validate:
- zeroing behavior,
- coordinate tracking drift,
- sequence ordering.

### Tuning Parameters
You can tune behavior directly in constants near the top of `Claw_Code.ino`:
- `thresholdHigh`, `thresholdLow`, `releaseHigh`, `releaseLow` — joystick feel/jitter tolerance.
- `returnDelay` — speed for return-to-zero moves.
- Z movement count (`180` steps down/up loops in `runClawSequence()`).
- Servo open/close angles (require a lot of tuning!):
  - `LEFT_OPEN`, `LEFT_CLOSED`
  - `RIGHT_OPEN`, `RIGHT_CLOSED`

### Known Software Limitations
- No homing or limit-switch handling.
- No acceleration profile (constant step timing only).
- No persistent storage of zero between power cycles.
- No fault detection for missed steps/stalls.
- Z axis depth is fixed step count per cycle (not sensor-verified).

## Physical Parts


### Core Electronics
- Microcontroller board: **Arduino Uno**
- CNC Shield (uses classic `STEP/DIR/EN` pin pattern mapped in firmware)
- Stepper drivers: **A4988**
- Joystick module with analog X/Y + momentary switch
- Additional momentary pushbutton (wired to `D12` with pull-up logic)
- 2x servos for claw actuation

### Motion System
- 2x NEMA 17 stepper motors for XY gantry
- 1x stepper motor for Z lift
- Gantry mechanism style: **H-bot style coordinated XY stepping (software behavior)**

### Wiring Map (from firmware)
- Joystick VRx -> `A0`
- Joystick VRy -> `A1`
- Joystick SW  -> `A2`
- Zero button  -> `D12`
- Left servo   -> `D13`
- Right servo  -> `A3`
- X STEP/DIR   -> `D2` / `D5`
- Y STEP/DIR   -> `D4` / `D7`
- Z STEP/DIR   -> `D3` / `D6`
- Driver EN    -> `D8`