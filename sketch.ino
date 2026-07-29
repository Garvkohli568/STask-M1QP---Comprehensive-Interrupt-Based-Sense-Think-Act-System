#include <Arduino.h>
#include <avr/interrupt.h>

// ---------------- PIN DEFINITIONS ----------------

const byte DOOR_PIN = 2;        // External interrupt button
const byte WINDOW_PIN = 8;      // Pin change interrupt button
const byte EMERGENCY_PIN = 9;   // Pin change interrupt button
const byte ALARM_LED_PIN = 5;   // Red alarm LED

// ---------------- INTERRUPT FLAGS ----------------

volatile bool doorInterruptFlag = false;
volatile bool pinChangeInterruptFlag = false;
volatile bool timerInterruptFlag = false;

volatile byte latestPortBState = 0;
volatile byte previousPortBState = 0;

// ---------------- SYSTEM STATES ----------------

bool doorActive = false;
bool windowActive = false;
bool emergencyActive = false;
bool alarmActive = false;

// ---------------- DEBOUNCE VARIABLES ----------------

unsigned long lastDoorEventTime = 0;
unsigned long lastWindowEventTime = 0;
unsigned long lastEmergencyEventTime = 0;

const unsigned long DEBOUNCE_TIME = 50;

// ---------------- EXTERNAL INTERRUPT ----------------

void doorISR()
{
  // Keep the ISR short.
  // Only set a flag here.
  doorInterruptFlag = true;
}

// ---------------- PIN CHANGE INTERRUPT ----------------

ISR(PCINT0_vect)
{
  byte currentPortBState = PINB;

  byte changedPins =
    currentPortBState ^ previousPortBState;

  // Check D8 and D9.
  if (changedPins & ((1 << PB0) | (1 << PB1)))
  {
    latestPortBState = currentPortBState;
    pinChangeInterruptFlag = true;
  }

  previousPortBState = currentPortBState;
}

// ---------------- TIMER INTERRUPT ----------------

ISR(TIMER1_COMPA_vect)
{
  // Only set a flag.
  timerInterruptFlag = true;
}

// ---------------- TIMER SETUP ----------------

void setupTimer1()
{
  noInterrupts();

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // Arduino Uno clock = 16 MHz
  // Prescaler = 1024
  // This creates approximately a one-second event.
  OCR1A = 15624;

  // CTC mode.
  TCCR1B |= (1 << WGM12);

  // Prescaler 1024.
  TCCR1B |= (1 << CS12);
  TCCR1B |= (1 << CS10);

  // Enable Timer1 compare interrupt.
  TIMSK1 |= (1 << OCIE1A);

  interrupts();
}

// ---------------- PIN CHANGE SETUP ----------------

void setupPinChangeInterrupts()
{
  /*
    Arduino Uno:

    D8 = PB0 = PCINT0
    D9 = PB1 = PCINT1

    Both pins use the same interrupt group.
  */

  previousPortBState = PINB;

  // Enable Port B pin change interrupts.
  PCICR |= (1 << PCIE0);

  // Enable D8.
  PCMSK0 |= (1 << PCINT0);

  // Enable D9.
  PCMSK0 |= (1 << PCINT1);
}

// ---------------- PRINT CURRENT STATE ----------------

void printSystemState()
{
  Serial.println("Current System State:");

  Serial.print("Door: ");
  Serial.println(
    doorActive ? "ACTIVE" : "INACTIVE"
  );

  Serial.print("Window: ");
  Serial.println(
    windowActive ? "ACTIVE" : "INACTIVE"
  );

  Serial.print("Emergency: ");
  Serial.println(
    emergencyActive ? "ACTIVE" : "INACTIVE"
  );

  Serial.print("Alarm LED: ");
  Serial.println(
    alarmActive ? "ON" : "OFF"
  );

  Serial.println("--------------------------------");
}

// ---------------- THINK AND ACT LOGIC ----------------

void processSystemLogic()
{
  /*
    Decision rules:

    1. Emergency button pressed:
       Alarm LED ON.

    2. Door and window active together:
       Alarm LED ON.

    3. Only one normal sensor active:
       Warning only, alarm LED OFF.

    4. No sensors active:
       Safe condition.
  */

  if (emergencyActive)
  {
    alarmActive = true;

    Serial.println(
      "Decision: Emergency condition detected."
    );
  }
  else if (doorActive && windowActive)
  {
    alarmActive = true;

    Serial.println(
      "Decision: Door and window are active together."
    );
  }
  else if (doorActive || windowActive)
  {
    alarmActive = false;

    Serial.println(
      "Decision: Warning - one entry sensor is active."
    );
  }
  else
  {
    alarmActive = false;

    Serial.println(
      "Decision: System is safe."
    );
  }

  // Act stage.
  digitalWrite(
    ALARM_LED_PIN,
    alarmActive ? HIGH : LOW
  );

  if (alarmActive)
  {
    Serial.println(
      "Action: Alarm LED turned ON."
    );
  }
  else
  {
    Serial.println(
      "Action: Alarm LED turned OFF."
    );
  }

  printSystemState();
}

// ---------------- PROCESS DOOR EVENT ----------------

void processDoorEvent()
{
  unsigned long currentTime = millis();

  if (
    currentTime - lastDoorEventTime
    < DEBOUNCE_TIME
  )
  {
    return;
  }

  lastDoorEventTime = currentTime;

  doorActive =
    digitalRead(DOOR_PIN) == LOW;

  Serial.println();
  Serial.println(
    "[External Interrupt Event - D2]"
  );

  if (doorActive)
  {
    Serial.println(
      "Door button pressed."
    );
  }
  else
  {
    Serial.println(
      "Door button released."
    );
  }

  processSystemLogic();
}

// ---------------- PROCESS PIN CHANGE EVENTS ----------------

void processPinChangeEvents()
{
  unsigned long currentTime = millis();

  byte capturedPortState;

  noInterrupts();
  capturedPortState = latestPortBState;
  interrupts();

  bool newWindowState =
    !(capturedPortState & (1 << PB0));

  bool newEmergencyState =
    !(capturedPortState & (1 << PB1));

  bool stateChanged = false;

  if (
    newWindowState != windowActive &&
    currentTime - lastWindowEventTime
    >= DEBOUNCE_TIME
  )
  {
    lastWindowEventTime = currentTime;
    windowActive = newWindowState;
    stateChanged = true;

    Serial.println();
    Serial.println(
      "[Pin Change Interrupt Event - D8]"
    );

    if (windowActive)
    {
      Serial.println(
        "Window button pressed."
      );
    }
    else
    {
      Serial.println(
        "Window button released."
      );
    }
  }

  if (
    newEmergencyState != emergencyActive &&
    currentTime - lastEmergencyEventTime
    >= DEBOUNCE_TIME
  )
  {
    lastEmergencyEventTime = currentTime;
    emergencyActive = newEmergencyState;
    stateChanged = true;

    Serial.println();
    Serial.println(
      "[Pin Change Interrupt Event - D9]"
    );

    if (emergencyActive)
    {
      Serial.println(
        "Emergency button pressed."
      );
    }
    else
    {
      Serial.println(
        "Emergency button released."
      );
    }
  }

  if (stateChanged)
  {
    processSystemLogic();
  }
}

// ---------------- PROCESS TIMER EVENT ----------------

void processTimerEvent()
{
  Serial.println(
    "[Timer1 Event] System heartbeat - one second."
  );
}

// ---------------- SETUP ----------------

void setup()
{
  Serial.begin(9600);

  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(WINDOW_PIN, INPUT_PULLUP);
  pinMode(EMERGENCY_PIN, INPUT_PULLUP);

  pinMode(ALARM_LED_PIN, OUTPUT);

  digitalWrite(ALARM_LED_PIN, LOW);

  // External interrupt on D2.
  attachInterrupt(
    digitalPinToInterrupt(DOOR_PIN),
    doorISR,
    CHANGE
  );

  // Pin change interrupts on D8 and D9.
  setupPinChangeInterrupts();

  // Timer1 periodic interrupt.
  setupTimer1();

  // Read initial states.
  doorActive =
    digitalRead(DOOR_PIN) == LOW;

  windowActive =
    digitalRead(WINDOW_PIN) == LOW;

  emergencyActive =
    digitalRead(EMERGENCY_PIN) == LOW;

  Serial.println(
    "========================================"
  );

  Serial.println(
    "SIT315 Task M1"
  );

  Serial.println(
    "Interrupt-Based Sense-Think-Act System"
  );

  Serial.println(
    "========================================"
  );

  Serial.println(
    "D2: Door sensor using attachInterrupt()"
  );

  Serial.println(
    "D8: Window sensor using Pin Change Interrupt"
  );

  Serial.println(
    "D9: Emergency sensor using Pin Change Interrupt"
  );

  Serial.println(
    "Timer1: One-second periodic event"
  );

  Serial.println(
    "D5: Alarm LED"
  );

  Serial.println(
    "========================================"
  );

  processSystemLogic();
}

// ---------------- MAIN LOOP ----------------

void loop()
{
  if (doorInterruptFlag)
  {
    noInterrupts();
    doorInterruptFlag = false;
    interrupts();

    processDoorEvent();
  }

  if (pinChangeInterruptFlag)
  {
    noInterrupts();
    pinChangeInterruptFlag = false;
    interrupts();

    processPinChangeEvents();
  }

  if (timerInterruptFlag)
  {
    noInterrupts();
    timerInterruptFlag = false;
    interrupts();

    processTimerEvent();
  }
}
