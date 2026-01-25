// file: MIDI-Clock_nudge_like_original_speed_var_v0_9.ino
// FW tag: quadratic nudge on A0, ~4 BPM/s max, FALLING tap, TM1637 shows integer BPM
// Built: __DATE__ __TIME__

#include <TimerOne.h>

/* ===== Firmware version tag ===== */
#define FW_NAME     "MIDI-Clock nudge"
#define FW_VERSION  "0.9 (quadratic nudge, ~4 BPM/s, DEAD_ZONE=50)"

/* ===== MIDI port abstraction ===== */
#if defined(UBRR1H) || defined(SERIAL_PORT_HARDWARE1)
  #define HAVE_HW_MIDI 1
  #define MIDI_SERIAL Serial1
#else
  #include <SoftwareSerial.h>
  #define HAVE_HW_MIDI 0
  const uint8_t MIDI_RX_UNUSED = 7;
  const uint8_t MIDI_TX_PIN    = 6;   // → DIN-5 pin 5 via ~220 Ω
  SoftwareSerial MIDI_SERIAL(MIDI_RX_UNUSED, MIDI_TX_PIN);
#endif

/* ===== TAP ===== */
#define TAP_PIN 2
#define TAP_PIN_POLARITY FALLING
#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150

/* ===== ABSOLUTE BPM INPUT — OFF (avoid conflict with nudge) ===== */
// #define DIMMER_INPUT_PIN A0
#define DIMMER_CHANGE_MARGIN 20

/* ===== NUDGE on A0 (left↓, right↑, center=stop) ===== */
#define DIMMER_CHANGE_PIN A0
#define DEAD_ZONE 50
#define NUDGE_MAX_SPEED_TENTHS 200   // max speed at edge: 20.0 tenths BPM/s = ~4 BPM/s

/* ===== Indicators ===== */
#define BLINK_OUTPUT_PIN 5
#define BLINK_PIN_POLARITY 0
#define BLINK_TIME 4

#define SYNC_OUTPUT_PIN 9
#define SYNC_PIN_POLARITY 0

/* ===== Start/Stop ===== */
#define START_STOP_INPUT_PIN A1
#define START_STOP_PIN_POLARITY 0
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC
#define DEBOUNCE_INTERVAL 500L // ms

/* ===== EEPROM ===== */
#define EEPROM_ADDRESS 0
#ifdef EEPROM_ADDRESS
  #include <EEPROM.h>
#endif

/* ===== MIDI forwarding ===== */
#define MIDI_FORWARD

/* ===== TM1637 ===== */
#define TM1637_DISPLAY
#ifdef TM1637_DISPLAY
  #include <TM1637Display.h>
  #define TM1637_CLK_PIN 3
  #define TM1637_DIO_PIN 4
  #define TM1637_BRIGHTNESS 0x0f
#endif

/* ===== General ===== */
#define MIDI_TIMING_CLOCK 0xF8
#define CLOCKS_PER_BEAT 24
#define MINIMUM_BPM 400     // 40.0 BPM (tenths)
#define MAXIMUM_BPM 3000    // 300.0 BPM (tenths)

/* ===== Compile-time guard against pin conflicts ===== */
#ifdef DIMMER_INPUT_PIN
  #if (DIMMER_INPUT_PIN == DIMMER_CHANGE_PIN)
    #error "DIMMER_INPUT_PIN conflicts with DIMMER_CHANGE_PIN. Disable one or change pins."
  #endif
#endif

long intervalMicroSeconds;
int bpm;  // tenths BPM (e.g., 1200 = 120.0)

long minimumTapInterval = 60L * 1000 * 1000 * 10 / MAXIMUM_BPM;
long maximumTapInterval = 60L * 1000 * 1000 * 10 / MINIMUM_BPM;

volatile long firstTapTime = 0;
volatile long lastTapTime  = 0;
volatile long timesTapped  = 0;

volatile int blinkCount = 0;

bool playing = false;
long lastStartStopTime = 0;

/* SoftSerial-safe MIDI clock buffer */
volatile uint8_t pendingClocks = 0;

#ifdef TM1637_DISPLAY
TM1637Display display(TM1637_CLK_PIN, TM1637_DIO_PIN);
uint8_t tm1637_data[4] = {0,0,0,0};
#endif

/* Prototypes */
void tapInput();
void startOrStop();
void sendClockPulse();
void updateBpm(long now);
long calculateIntervalMicroSecs(int bpm);
#ifdef TM1637_DISPLAY
void setDisplayValue(int bpm_tenths);
#endif

void setup() {
  Serial.begin(38400);
  MIDI_SERIAL.begin(31250);

  Serial.println();
  Serial.print(FW_NAME); Serial.print(" ");
  Serial.print(FW_VERSION); Serial.print(" | Built: ");
  Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);

  pinMode(BLINK_OUTPUT_PIN, OUTPUT);
  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
  pinMode(START_STOP_INPUT_PIN, INPUT);
  pinMode(DIMMER_CHANGE_PIN, INPUT);   // explicit A0

  pinMode(TAP_PIN, INPUT_PULLUP);      // FALLING + button to GND
  attachInterrupt(digitalPinToInterrupt(TAP_PIN), tapInput, TAP_PIN_POLARITY);

#ifdef EEPROM_ADDRESS
  bpm = (EEPROM.read(EEPROM_ADDRESS) << 8) + EEPROM.read(EEPROM_ADDRESS + 1);
  if (bpm < MINIMUM_BPM || bpm > MAXIMUM_BPM) bpm = 1200;
#else
  bpm = 1200;
#endif

  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);

#ifdef TM1637_DISPLAY
  display.setBrightness(TM1637_BRIGHTNESS);
  setDisplayValue(bpm); // integer BPM
#endif
}

void loop() {
  long now = micros();

/* TAP */
#ifdef TAP_PIN
  if (timesTapped > 0 && timesTapped < MINIMUM_TAPS && (now - lastTapTime) > maximumTapInterval) {
    timesTapped = 0;
  } else if (timesTapped >= MINIMUM_TAPS) {
    long avgTapInterval = (lastTapTime - firstTapTime) / (timesTapped - 1);
    if ((now - lastTapTime) > (avgTapInterval * EXIT_MARGIN / 100)) {
      bpm = 60L * 1000 * 1000 * 10 / avgTapInterval;
      updateBpm(now);
      blinkCount = ((now - lastTapTime) * 24 / avgTapInterval) % CLOCKS_PER_BEAT;
      timesTapped = 0;
    }
  }
#endif

/* NUDGE on A0: quadratic speed + fractional accumulator */
#ifdef DIMMER_CHANGE_PIN
  static bool init = false;
  static unsigned long lastMs = 0;
  static long accum01 = 0;                 // "0.1 BPM × ms"

  unsigned long ms = millis();
  if (!init) { init = true; lastMs = ms; }
  unsigned long dt = ms - lastMs;          // ms since last update

  int v = analogRead(DIMMER_CHANGE_PIN);   // 0..1023
  int delta  = v - 512;
  int adelta = delta < 0 ? -delta : delta;

  if (adelta > DEAD_ZONE) {
    if (dt > 0) {
      long range = 512 - DEAD_ZONE;               // >0
      long eff   = adelta - DEAD_ZONE;            // 1..range
      long speed_tenths = (long)NUDGE_MAX_SPEED_TENTHS * eff * eff / (range * range); // 0.1 BPM/s

      accum01 += speed_tenths * (long)dt;         // integrate over dt
      int steps = (int)(accum01 / 1000);          // tenths BPM
      accum01 %= 1000;

      if (steps) {
        if (delta < 0) steps = -steps;
        bpm += steps;
        if (bpm < MINIMUM_BPM) bpm = MINIMUM_BPM;
        if (bpm > MAXIMUM_BPM) bpm = MAXIMUM_BPM;
        updateBpm(now);
      }
      lastMs = ms;
    }
  } else {
    accum01 = 0;                                   // stop in dead zone
    lastMs  = ms;
  }
#endif

/* Start/Stop */
  bool startStopPressed = (START_STOP_PIN_POLARITY - analogRead(START_STOP_INPUT_PIN)) > 1024 / 2;
  if (startStopPressed && (lastStartStopTime + (DEBOUNCE_INTERVAL * 1000)) < now) {
    startOrStop();
    lastStartStopTime = now;
  }

/* MIDI forwarding */
#ifdef MIDI_FORWARD
  while (MIDI_SERIAL.available()) {
    int b = MIDI_SERIAL.read();
    MIDI_SERIAL.write(b);
  }
#endif

/* SoftSerial: flush ticks outside ISR */
#if !HAVE_HW_MIDI
  noInterrupts();
  uint8_t ticks = pendingClocks;
  pendingClocks = 0;
  interrupts();
  while (ticks--) MIDI_SERIAL.write(MIDI_TIMING_CLOCK);
#endif
}

void tapInput() {
  long now = micros();
  if (now - lastTapTime < minimumTapInterval) return; // debounce by time
  if (timesTapped == 0) firstTapTime = now;
  timesTapped++;
  lastTapTime = now;
}

void startOrStop() {
  if (!playing) MIDI_SERIAL.write(MIDI_START);
  else          MIDI_SERIAL.write(MIDI_STOP);
  playing = !playing;
}

void sendClockPulse() {
#if HAVE_HW_MIDI
  MIDI_SERIAL.write(MIDI_TIMING_CLOCK);    // HW UART: safe in ISR
#else
  pendingClocks++;                         // SoftSerial: accumulate only
#endif

  blinkCount = (blinkCount + 1) % CLOCKS_PER_BEAT;
  if (blinkCount == 0) {
    analogWrite(BLINK_OUTPUT_PIN, 255 - BLINK_PIN_POLARITY);
    analogWrite(SYNC_OUTPUT_PIN, 255 - SYNC_PIN_POLARITY);
  } else {
    if (blinkCount == 1)    analogWrite(SYNC_OUTPUT_PIN, 0 + SYNC_PIN_POLARITY);
    if (blinkCount == BLINK_TIME) analogWrite(BLINK_OUTPUT_PIN, 0 + BLINK_PIN_POLARITY);
  }
}

void updateBpm(long /*now*/) {
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
#ifdef EEPROM_ADDRESS
  EEPROM.write(EEPROM_ADDRESS, bpm / 256);
  EEPROM.write(EEPROM_ADDRESS + 1, bpm % 256);
#endif
#ifdef TM1637_DISPLAY
  setDisplayValue(bpm);  // integer BPM
#endif
}

long calculateIntervalMicroSecs(int bpm_tenths) {
  return 60L * 1000 * 1000 * 10 / bpm_tenths / CLOCKS_PER_BEAT;
}

#ifdef TM1637_DISPLAY
void setDisplayValue(int bpm_tenths) {
  int whole = bpm_tenths / 10;
  tm1637_data[0] = (whole >= 1000) ? display.encodeDigit((whole / 1000) % 10) : 0x00;
  tm1637_data[1] = (whole >= 100)  ? display.encodeDigit((whole / 100)  % 10) : 0x00;
  tm1637_data[2] = (whole >= 10)   ? display.encodeDigit((whole / 10)   % 10) : 0x00;
  tm1637_data[3] = display.encodeDigit(whole % 10) & 0x7F;  // DP off
  display.setSegments(tm1637_data);
}
#endif
