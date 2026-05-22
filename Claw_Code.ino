// NEMA 17 + CNC Shield + Joystick Control + Servo
// Corrected: Added Servo control on Pin 13 when joystick is clicked
// Update: Restricted movement to one axis at a time (No Diagonals)
// Update: Added position tracking + joystick SW zeroing (A2)
// Update: Dual servo claw — left on D13, right on A3
// Fix: stepBothMotors() for synchronized H-bot stepping
// Fix: Hysteresis on joystick thresholds to eliminate jitter
// Update: Pushbutton (D12) zeroes, joystick SW (A2) triggers claw
// Update: Return to zero after raise, then open claw
// Update: First joystick SW press zeroes, all subsequent trigger claw

#include <Servo.h>

// --- Pin Definitions ---
const int pinVRx    = A0;
const int pinVRy    = A1;
const int pinSW     = A2; // Joystick click — first press zeroes, then claw trigger
const int pinButton = 12; // Pushbutton — ZEROING

// Servo Setup
Servo leftServo;
Servo rightServo;
const int pinServoLeft  = 13;
const int pinServoRight = A3;

// Claw positions
const int LEFT_OPEN    = 0;
const int LEFT_CLOSED  = 30;
const int RIGHT_OPEN   = 30;
const int RIGHT_CLOSED = 0;

// CNC Shield standard pins
const int stepX = 2;
const int dirX  = 5;

const int stepY = 4;
const int dirY  = 7;

const int stepZ = 3;
const int dirZ  = 6;

const int enPin = 8;

// --- Joystick Thresholds ---
const int thresholdHigh = 700;
const int thresholdLow  = 300;
const int releaseHigh   = 600;
const int releaseLow    = 400;

// Active direction tracker — 0=none, 1=up, 2=down, 3=left, 4=right
int activeDir = 0;

// Return-to-zero speed
// 3000 = slow/safe, 1000 = normal, 500 = fast
const int returnDelay = 3000;

// --- Position Tracking ---
long posX = 0;
long posY = 0;
long posZ = 0;

// First SW press zeroes, all subsequent presses trigger claw
bool hasBeenZeroed = false;

// Pushbutton zero debounce
bool          zeroPending      = false;
bool          zeroLastState    = HIGH;
unsigned long zeroDebounceTime = 0;
const unsigned long ZERO_DEBOUNCE_MS = 50;

// Joystick SW debounce
bool          swPending      = false;
bool          swLastState    = HIGH;
unsigned long swDebounceTime = 0;
const unsigned long SW_DEBOUNCE_MS = 50;

// Serial print throttle
long lastPrintedX = 1;
long lastPrintedY = 1;
long lastPrintedZ = 1;
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL_MS = 100;

// ---------------------------------------------------------------
void setup() {
    Serial.begin(9600);

    pinMode(pinVRx,    INPUT);
    pinMode(pinVRy,    INPUT);
    pinMode(pinButton, INPUT_PULLUP);
    pinMode(pinSW,     INPUT_PULLUP);

    pinMode(stepX, OUTPUT);
    pinMode(dirX,  OUTPUT);
    pinMode(stepY, OUTPUT);
    pinMode(dirY,  OUTPUT);
    pinMode(stepZ, OUTPUT);
    pinMode(dirZ,  OUTPUT);
    pinMode(enPin, OUTPUT);

    digitalWrite(enPin, LOW);

    leftServo.attach(pinServoLeft);
    rightServo.attach(pinServoRight);
    clawOpen();

    Serial.println("=== Claw Machine Ready ===");
    Serial.println("Pushbutton (D12)  = zero position");
    Serial.println("Joystick SW (A2)  = FIRST press zeroes, then triggers claw");
    Serial.println("X+ RIGHT  X- LEFT  Y+ UP  Y- DOWN  Z+ LOWER  Z- RAISE");
    printPosition(true);
}

// ---------------------------------------------------------------
void loop() {
    checkZeroButton();
    readJoystickAndMove();
    maybePrintPosition();
}

// ---------------------------------------------------------------
//  Synchronized dual-motor step — both STEP pins fire together
// ---------------------------------------------------------------
void stepBothMotors(int dir1, int dir2, int delayUs) {
    digitalWrite(dirX, dir1);
    digitalWrite(dirY, dir2);
    delayMicroseconds(5);

    digitalWrite(stepX, HIGH);
    digitalWrite(stepY, HIGH);
    delayMicroseconds(10);
    digitalWrite(stepX, LOW);
    digitalWrite(stepY, LOW);

    delayMicroseconds(delayUs);
}

// ---------------------------------------------------------------
//  Claw helpers
// ---------------------------------------------------------------
void clawOpen() {
    leftServo.write(LEFT_OPEN);
    rightServo.write(RIGHT_OPEN);
}

void clawClose() {
    leftServo.write(LEFT_CLOSED);
    rightServo.write(RIGHT_CLOSED);
}

// ---------------------------------------------------------------
//  Zeroing helper — shared by both buttons
// ---------------------------------------------------------------
void doZero() {
    posX = 0; posY = 0; posZ = 0;
    Serial.println(">>> ZEROED: position reset to (0, 0, 0) <<<");
    printPosition(true);
}

// ---------------------------------------------------------------
//  Zeroing: pushbutton on D12
//  Also resets hasBeenZeroed so SW first-press logic stays consistent
// ---------------------------------------------------------------
void checkZeroButton() {
    bool btnState = digitalRead(pinButton);

    if (btnState == LOW && zeroLastState == HIGH) {
        zeroDebounceTime = millis();
        zeroPending = true;
    }

    if (zeroPending && (millis() - zeroDebounceTime) >= ZERO_DEBOUNCE_MS) {
        if (digitalRead(pinButton) == LOW) {
            doZero();
            hasBeenZeroed = true; // pushbutton zero also counts as zeroed
        }
        zeroPending = false;
    }

    zeroLastState = btnState;
}

// ---------------------------------------------------------------
//  Return gantry to X=0, Y=0 — X axis first, then Y
// ---------------------------------------------------------------
void returnToZero() {
    Serial.println("--- RETURNING TO ZERO ---");

    if (posX > 0) {
        Serial.print("X: "); Serial.print(posX); Serial.println(" steps LEFT");
        for (long i = posX; i > 0; i--) {
            stepBothMotors(HIGH, HIGH, returnDelay);
            posX--;
        }
    } else if (posX < 0) {
        Serial.print("X: "); Serial.print(-posX); Serial.println(" steps RIGHT");
        for (long i = posX; i < 0; i++) {
            stepBothMotors(LOW, LOW, returnDelay);
            posX++;
        }
    }

    if (posY > 0) {
        Serial.print("Y: "); Serial.print(posY); Serial.println(" steps DOWN");
        for (long i = posY; i > 0; i--) {
            stepBothMotors(HIGH, LOW, returnDelay);
            posY--;
        }
    } else if (posY < 0) {
        Serial.print("Y: "); Serial.print(-posY); Serial.println(" steps UP");
        for (long i = posY; i < 0; i++) {
            stepBothMotors(LOW, HIGH, returnDelay);
            posY++;
        }
    }

    printPosition(true);
    Serial.println("--- AT ZERO ---");
}

// ---------------------------------------------------------------
//  Position printing
// ---------------------------------------------------------------
void maybePrintPosition() {
    bool changed  = (posX != lastPrintedX || posY != lastPrintedY || posZ != lastPrintedZ);
    bool timedOut = (millis() - lastPrintTime) >= PRINT_INTERVAL_MS;
    if (changed && timedOut) printPosition(false);
}

void printPosition(bool force) {
    if (force || (posX != lastPrintedX || posY != lastPrintedY || posZ != lastPrintedZ)) {
        Serial.print("POS  X: ");
        Serial.print(posX);
        Serial.print(" steps  |  Y: ");
        Serial.print(posY);
        Serial.print(" steps  |  Z: ");
        Serial.print(posZ);
        Serial.println(" steps");

        lastPrintedX  = posX;
        lastPrintedY  = posY;
        lastPrintedZ  = posZ;
        lastPrintTime = millis();
    }
}

// ---------------------------------------------------------------
//  Joystick + motor logic
// ---------------------------------------------------------------
void readJoystickAndMove() {

    int xVal = analogRead(pinVRx);
    int yVal = analogRead(pinVRy);

    // --- Hysteresis direction logic ---
    if (activeDir == 1 && xVal < releaseHigh) activeDir = 0;
    if (activeDir == 2 && xVal > releaseLow)  activeDir = 0;
    if (activeDir == 3 && yVal > releaseLow)  activeDir = 0;
    if (activeDir == 4 && yVal < releaseHigh) activeDir = 0;

    if (activeDir == 0) {
        if      (xVal > thresholdHigh) activeDir = 1;
        else if (xVal < thresholdLow)  activeDir = 2;
        else if (yVal < thresholdLow)  activeDir = 3;
        else if (yVal > thresholdHigh) activeDir = 4;
    }

    switch (activeDir) {
        case 1: stepBothMotors(LOW,  HIGH, 4000); posY++; break; // UP
        case 2: stepBothMotors(HIGH, LOW,  4000); posY--; break; // DOWN
        case 3: stepBothMotors(HIGH, HIGH, 4000); posX--; break; // LEFT
        case 4: stepBothMotors(LOW,  LOW,  4000); posX++; break; // RIGHT
    }

    // --- Joystick SW: first press zeroes, all others trigger claw ---
    bool swState = digitalRead(pinSW);

    if (swState == LOW && swLastState == HIGH) {
        swDebounceTime = millis();
        swPending = true;
    }

    if (swPending && (millis() - swDebounceTime) >= SW_DEBOUNCE_MS) {
        if (digitalRead(pinSW) == LOW) {
            swPending = false;
            if (!hasBeenZeroed) {
                // First press — set zero
                doZero();
                hasBeenZeroed = true;
                Serial.println(">>> First SW press: zero set. Subsequent presses will trigger claw. <<<");
            } else {
                // All subsequent presses — trigger claw
                runClawSequence();
            }
        } else {
            swPending = false; // released before debounce confirmed, ignore
        }
    }

    swLastState = swState;
}

// ---------------------------------------------------------------
//  Full claw sequence
//  Lower → close → raise → return to zero → open
// ---------------------------------------------------------------
void runClawSequence() {
    Serial.println("--- CLAW TRIGGERED ---");

    // 1. Lower Z
    for (int i = 0; i < 180; i++) {
        stepMotor(stepZ, dirZ, LOW, 3700);
        posZ++;
    }

    // 2. Close claw at bottom
    delay(500);
    clawClose();
    delay(1000);

    // 3. Raise Z
    for (int i = 0; i < 180; i++) {
        stepMotor(stepZ, dirZ, HIGH, 3700);
        posZ--;
    }
    delay(500);

    printPosition(true);
    Serial.println("--- CLAW DONE, RETURNING ---");

    // 4. Return to zero holding claw closed
    returnToZero();

    // 5. Open claw at zero
    clawOpen();
    delay(500);

    Serial.println("--- SEQUENCE COMPLETE ---");
}

// ---------------------------------------------------------------
//  Stepper function — Z axis only, X/Y use stepBothMotors()
// ---------------------------------------------------------------
void stepMotor(int pinStep, int pinDir, int dir, int stepDelay) {
    digitalWrite(pinDir, dir);
    delayMicroseconds(5);
    digitalWrite(pinStep, HIGH);
    delayMicroseconds(10);
    digitalWrite(pinStep, LOW);
    delayMicroseconds(stepDelay);
}