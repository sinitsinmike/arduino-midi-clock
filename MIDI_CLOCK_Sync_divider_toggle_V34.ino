/* ======================= MIDI CLOCK — V3.3 (Beat on left-G segment opt.) ======================= *
 * File: MIDI_CLOCK_V3_3.ino
 * FW_VERSION: "V3.3"   Build: __DATE__ __TIME__
 *
 * Compile-time switches (по умолчанию ОТКЛЮЧЕНО):
 *   // #define ENABLE_SYNC 1               // SYNC-выход + меню делителя/полярности
 *   // #define ENABLE_A1_DOUBLECLICK 1     // A1 double-click = Restart from bar 1 (SPP=0+Start)
 *   // #define ENABLE_BEAT_SEG 1           // мигание сегмента G у левой цифры раз в такт
 *   // #define BEAT_SEG_ONLY_WHEN_PLAY 1   // если включить — мигает ТОЛЬКО в PLAY
 * ================================================================================================= */

#define FW_VERSION     "V3.3"
#define FW_BUILDSTAMP  __DATE__ " " __TIME__

#include <TimerOne.h>
#include <EEPROM.h>

/* ========= MASTER SWITCHES ========= */
// #define ENABLE_SYNC 1
// #define ENABLE_A1_DOUBLECLICK 1
// #define ENABLE_BEAT_SEG 1
// #define BEAT_SEG_ONLY_WHEN_PLAY 1

/* ===== MIDI on D1 (TX0). Do NOT open Serial Monitor. ===== */
#define MIDI_SERIAL Serial
#define HAVE_HW_MIDI 1

/* === Pins / behavior === */
#define TAP_PIN 2
#define TAP_PIN_POLARITY FALLING
#define START_STOP_INPUT_PIN A1        // active-LOW
#define DIMMER_CHANGE_PIN A0           // nudge
#define BLINK_OUTPUT_PIN 5
#define BLINK_PIN_POLARITY 0           // 0=positive

#ifdef ENABLE_SYNC
  #define SYNC_OUTPUT_PIN 8
  #define SYNC_PIN_POLARITY 0
  #define SYNC_PULSE_WIDTH_US 15000
#endif

/* === Button timings (ms) === */
#define SHORT_PRESS_MIN_MS 30
#define SHORT_PRESS_COOLDOWN_MS 200
#define CAL_LONG_MS 3000
#define NUDGE_TOGGLE_HOLD_MS 4000
#define TAP_TOGGLE_HOLD_MS 5000
#ifdef ENABLE_A1_DOUBLECLICK
  #define DOUBLE_CLICK_MAX_MS 400
#endif
#ifdef ENABLE_SYNC
  #define SYNC_MENU_HOLD_MS 1000
  #define SYNC_SUBPAGE_TAP_HOLD_MS 800
  #define SYNC_MENU_IDLE_EXIT_MS 10000UL
#endif

/* === TAP tempo === */
#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150

/* === Nudge === */
#define DEAD_ZONE 80
#define NUDGE_MAX_SPEED_TENTHS 200
#define CENTER_INIT_DEFAULT 512

/* === Display (TM1637) === */
#define TM1637_DISPLAY
#ifdef TM1637_DISPLAY
  #include <TM1637Display.h>
  #define TM1637_CLK_PIN 3
  #define TM1637_DIO_PIN 4
  #define TM1637_BRIGHTNESS 0x0f
  TM1637Display display(TM1637_CLK_PIN, TM1637_DIO_PIN);
  uint8_t segs[4] = {0,0,0,0};

  // segments
  #define SEG_A  0x01
  #define SEG_B  0x02
  #define SEG_C  0x04
  #define SEG_D  0x08
  #define SEG_E  0x10
  #define SEG_F  0x20
  #define SEG_G  0x40
  #define SEG_DP 0x80

  // letters (approx)
  #define SEG_L_  (SEG_D|SEG_E|SEG_F)
  #define SEG_O_  (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F)
  #define SEG_C_  (SEG_A|SEG_D|SEG_E|SEG_F)
  #define SEG_N_  (SEG_C|SEG_E|SEG_G)
  #define SEG_U_  (SEG_B|SEG_C|SEG_D|SEG_E|SEG_F)
  #define SEG_K_  (SEG_B|SEG_C|SEG_E|SEG_F|SEG_G)
  #define SEG_P   (SEG_A|SEG_B|SEG_E|SEG_F|SEG_G)
  #define SEG_AA  (SEG_A|SEG_B|SEG_C|SEG_E|SEG_F|SEG_G)
  #define SEG_Y   (SEG_B|SEG_C|SEG_D|SEG_F|SEG_G)
  #define SEG_S_  (SEG_A|SEG_C|SEG_D|SEG_F|SEG_G)
  #define SEG_T_  (SEG_D|SEG_E|SEG_F|SEG_G)
  #define SEG_F_  (SEG_A|SEG_E|SEG_F|SEG_G)

  // words
  const uint8_t WORD_PLAY[4] = { SEG_P,  SEG_L_, SEG_AA, SEG_Y  };
  const uint8_t WORD_STOP[4] = { SEG_S_, SEG_T_, SEG_O_, SEG_P  };
  const uint8_t WORD_CAL [4] = { SEG_C_, SEG_AA, SEG_L_, 0x00  };
  const uint8_t WORD_LOCK[4] = { SEG_L_, SEG_O_, SEG_C_, SEG_K_ };
  const uint8_t WORD_UNLK[4] = { SEG_U_, SEG_N_, SEG_L_, SEG_K_ };
  const uint8_t WORD_CPLY[4] = { SEG_C_, SEG_P,  SEG_L_, SEG_Y  };
  const uint8_t WORD_CALL[4] = { SEG_C_, SEG_AA, SEG_L_, SEG_L_ };

  #ifdef ENABLE_BEAT_SEG
    #define BEAT_PULSE_MS 60
    volatile bool beatPulseRequest = false; // ISR → loop
    bool beatSegOn = false;                 // состояние SEG_G у 1-й цифры
    unsigned long beatOffAtMs = 0;
  #endif

  #ifdef ENABLE_SYNC
    const uint8_t WORD_SYNC[4] = { SEG_S_, SEG_Y,  SEG_N_, SEG_C_ };
    const uint8_t WORD_SPON[4] = { SEG_S_, SEG_P,  SEG_O_, SEG_N_ };
    const uint8_t WORD_SPOF[4] = { SEG_S_, SEG_P,  SEG_O_, SEG_F_ };
  #endif
#endif

/* === EEPROM layout === */
#define EEPROM_ADDR_BPM_MSB     0
#define EEPROM_ADDR_BPM_LSB     1
#define EEPROM_ADDR_CENTER_MSB  2
#define EEPROM_ADDR_CENTER_LSB  3
#define EEPROM_ADDR_FLAGS       4
#define FLAG_NUDGE_ENABLED           0x01
#define FLAG_CLOCK_ONLY_WHEN_PLAY    0x02
#define FLAG_SYNC_DIV_MASK           0x0C  // bits 2..3
#define FLAG_SYNC_POL_INV            0x10

/* === MIDI/Clock === */
#define MIDI_TIMING_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC
#define CLOCKS_PER_BEAT 24
#define MINIMUM_BPM 400
#define MAXIMUM_BPM 3000
#define BLINK_TIME 4

/* === Globals === */
int bpm;  // tenths
long minimumTapInterval = 60L * 1000 * 1000 * 10 / MAXIMUM_BPM;
long maximumTapInterval = 60L * 1000 * 1000 * 10 / MINIMUM_BPM;

volatile long firstTapTime = 0, lastTapTime = 0;
volatile long timesTapped = 0;

volatile int  blinkCount = 0;
volatile bool playing = false;

int  nudge_center = CENTER_INIT_DEFAULT;
bool nudgeEnabled = true;
bool clockOnlyWhenPlay = false;

bool  bpmDirty = false;
unsigned long bpmLastChangeMs = 0;

const uint8_t BLINK_ON  = (BLINK_PIN_POLARITY==0) ? HIGH : LOW;
const uint8_t BLINK_OFF = (BLINK_PIN_POLARITY==0) ? LOW  : HIGH;

#ifdef ENABLE_SYNC
  bool     syncPolarityInv = false;
  uint8_t  SYNC_ON_L = HIGH, SYNC_OFF_L = LOW;
  void refreshSyncLevels() {
    bool activeHigh = ((SYNC_PIN_POLARITY==0) ^ syncPolarityInv);
    SYNC_ON_L  = activeHigh ? HIGH : LOW;
    SYNC_OFF_L = activeHigh ? LOW  : HIGH;
  }
  bool inSyncMenu = false;
  bool syncMenuPolarityMode = false;
  uint8_t syncDivIndex = 1;                 // 1/4
  const uint8_t TICKS_PER_PULSE_LUT[4] = {48,24,12,6}; // S2,S4,S8,S16
  volatile uint8_t ticksPerPulse = 24;
  unsigned long syncMenuLastActivityMs = 0;
  static bool tapPressedMenu=false; static unsigned long tapDownMs=0;
  static bool a1PressedMenu=false;  static unsigned long a1DownMs=0;
  static bool tapHeldMenu=false;    static unsigned long tapHeldStartMs=0;
  static bool bothPressedMenu=false; static unsigned long bothT0Menu=0;
  volatile bool syncPulseOn = false;
  volatile unsigned long syncOffAtUs = 0;
  volatile uint16_t syncTickCounter = 0;
#endif

/* === Combo/state === */
volatile bool suppressTap = false;
bool bothPressed = false, toggledThisHold = false;
unsigned long bothT0 = 0;

bool a1Pressed = false, calFired = false, a1BlockUntilRelease = false;
unsigned long a1T0 = 0, lastShortActionMs = 0;

/* TAP-hold state */
static bool tapHoldActive=false, tapToggledThisHold=false;
static unsigned long tapHoldT0=0;

#ifdef ENABLE_A1_DOUBLECLICK
  bool a1SinglePending = false;
  unsigned long a1FirstClickMs = 0;
#endif

/* ==== EEPROM helpers ==== */
inline void eepromWriteIfChanged(int addr, uint8_t val) {
  if (EEPROM.read(addr) != val) EEPROM.write(addr, val);
}
inline void eepromWriteWordIfChanged(int addrMsb, int addrLsb, uint16_t word) {
  uint8_t msb = (uint8_t)(word >> 8);
  uint8_t lsb = (uint8_t)(word & 0xFF);
  if (EEPROM.read(addrMsb) != msb) EEPROM.write(addrMsb, msb);
  if (EEPROM.read(addrLsb) != lsb) EEPROM.write(addrLsb, lsb);
}

/* ==== Protos ==== */
void tapInput();
void startOrStop();
#ifdef ENABLE_A1_DOUBLECLICK
void startFromBar1();
#endif
void sendClockPulse();
void updateBpm();
long calculateIntervalMicroSecs(int bpm_tenths);
#ifdef TM1637_DISPLAY
void showSegs(const uint8_t w[4], unsigned d_ms);
void showBpm(int bpm_tenths);
  #ifdef ENABLE_BEAT_SEG
  void setBeatSeg(bool on);
  #endif
  #ifdef ENABLE_SYNC
    void showSyncDivIdx(uint8_t idx);
    void showSyncPol();
  #endif
#endif
#ifdef ENABLE_SYNC
void applyDiv(uint8_t idx);
void applyPol(bool inv);
#endif
void loadCenter();
void saveCenter(int c);
void runCenterCalibration();
void loadFlags();
void saveFlags();
void ensureSyncBitsClearedIfDisabled();

/* === SPP0 + phase resync === */
void sendSPPZero(){ MIDI_SERIAL.write((uint8_t)0xF2); MIDI_SERIAL.write((uint8_t)0x00); MIDI_SERIAL.write((uint8_t)0x00); }
void hardResyncPhase(){
  blinkCount = 0;
#ifdef ENABLE_SYNC
  syncTickCounter = 0; syncPulseOn = false; digitalWrite(SYNC_OUTPUT_PIN, SYNC_OFF_L);
#endif
}

/* ===== setup ===== */
void setup() {
  MIDI_SERIAL.begin(31250);

  pinMode(BLINK_OUTPUT_PIN, OUTPUT);
  digitalWrite(BLINK_OUTPUT_PIN, BLINK_OFF);

#ifdef ENABLE_SYNC
  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
  refreshSyncLevels();
  digitalWrite(SYNC_OUTPUT_PIN, SYNC_OFF_L);
#endif

  pinMode(DIMMER_CHANGE_PIN, INPUT);
  pinMode(START_STOP_INPUT_PIN, INPUT_PULLUP);
  pinMode(TAP_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TAP_PIN), tapInput, TAP_PIN_POLARITY);

  int msb = EEPROM.read(EEPROM_ADDR_BPM_MSB);
  int lsb = EEPROM.read(EEPROM_ADDR_BPM_LSB);
  bpm = (msb<<8) | lsb;
  if (bpm < MINIMUM_BPM || bpm > MAXIMUM_BPM) bpm = 1200;

  loadCenter();
  loadFlags();
#ifndef ENABLE_SYNC
  ensureSyncBitsClearedIfDisabled();
#endif

  Timer1.initialize(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);

#ifdef TM1637_DISPLAY
  display.setBrightness(TM1637_BRIGHTNESS);
  showSegs(clockOnlyWhenPlay ? WORD_CPLY : WORD_CALL, 350);
  showBpm(bpm);
#endif
}

/* ===== loop ===== */
void loop() {
  unsigned long nowMs = millis();
  bool tapHeld = (digitalRead(TAP_PIN) == LOW);
  bool a1Held  = (digitalRead(START_STOP_INPUT_PIN) == LOW);

#ifdef TM1637_DISPLAY
  #ifdef ENABLE_BEAT_SEG
  if (beatPulseRequest) {
    beatPulseRequest = false;
    setBeatSeg(true);
    beatOffAtMs = nowMs + BEAT_PULSE_MS;
  }
  if (beatSegOn && (long)(nowMs - beatOffAtMs) >= 0) {
    setBeatSeg(false);
  }
  #endif
#endif

#ifdef ENABLE_SYNC
  if (syncPulseOn && (long)((unsigned long)micros() - syncOffAtUs) >= 0) {
    digitalWrite(SYNC_OUTPUT_PIN, SYNC_OFF_L);
    syncPulseOn = false;
  }
#endif

#ifdef ENABLE_SYNC
  if (!inSyncMenu) {
#endif
    if (tapHeld && a1Held) {
      if (!bothPressed) {
        bothPressed = true; bothT0 = nowMs; toggledThisHold = false;
        a1Pressed=false; calFired=false; a1BlockUntilRelease=true;
        #ifdef ENABLE_A1_DOUBLECLICK
          a1SinglePending = false;
        #endif
      }
      suppressTap = true;
#ifdef ENABLE_SYNC
      unsigned long held = nowMs - bothT0;
      if (!toggledThisHold && held >= SYNC_MENU_HOLD_MS && held < NUDGE_TOGGLE_HOLD_MS) {
        inSyncMenu = true;
        syncMenuPolarityMode = false;
        syncMenuLastActivityMs = nowMs;
        bothPressedMenu=false; tapPressedMenu=false; a1PressedMenu=false; tapHeldMenu=false;
        #ifdef TM1637_DISPLAY
          showSegs(WORD_SYNC, 350);
          showSyncDivIdx(syncDivIndex);
        #endif
        toggledThisHold = true;
      } else
#endif
      if (!toggledThisHold && (nowMs - bothT0) >= NUDGE_TOGGLE_HOLD_MS) {
        nudgeEnabled = !nudgeEnabled; saveFlags();
        #ifdef TM1637_DISPLAY
          showSegs(nudgeEnabled ? WORD_UNLK : WORD_LOCK, 700);
          showBpm(bpm);
        #endif
        toggledThisHold = true;
      }
    } else {
      if (bothPressed) { bothPressed=false; suppressTap=false; }

      // TAP ≥5s → toggle ClockOnlyWhenPlay
      if (tapHeld && !a1Held) {
        if (!tapHoldActive) { tapHoldActive=true; tapHoldT0=nowMs; tapToggledThisHold=false; }
        if (!tapToggledThisHold && (nowMs - tapHoldT0) >= TAP_TOGGLE_HOLD_MS) {
          clockOnlyWhenPlay = !clockOnlyWhenPlay; saveFlags();
          if (clockOnlyWhenPlay && !playing) digitalWrite(BLINK_OUTPUT_PIN, BLINK_OFF);
          #ifdef TM1637_DISPLAY
            showSegs(clockOnlyWhenPlay ? WORD_CPLY : WORD_CALL, 700);
            showBpm(bpm);
          #endif
          tapToggledThisHold = true;
          timesTapped = 0;
        }
      } else {
        tapHoldActive = false;
        tapToggledThisHold = false;
      }

      // TAP tempo
      long nowUs = micros();
      if (timesTapped > 0 && timesTapped < MINIMUM_TAPS && (nowUs - lastTapTime) > maximumTapInterval) {
        timesTapped = 0;
      } else if (timesTapped >= MINIMUM_TAPS) {
        long avgTapInterval = (lastTapTime - firstTapTime) / (timesTapped - 1);
        if ((nowUs - lastTapTime) > (avgTapInterval * EXIT_MARGIN / 100L)) {
          bpm = 60L * 1000 * 1000 * 10 / avgTapInterval;
          updateBpm();
          blinkCount = ((nowUs - lastTapTime) * 24 / avgTapInterval) % CLOCKS_PER_BEAT;
          timesTapped = 0;
        }
      }

      // Nudge
      if (nudgeEnabled) {
        static unsigned long lastMsN=0; static long accum01=0;
        if (lastMsN==0) lastMsN=nowMs;
        unsigned long dt = nowMs - lastMsN; lastMsN=nowMs;

        int v = analogRead(DIMMER_CHANGE_PIN);
        int d = v - nudge_center; int ad = d<0?-d:d;
        if (ad > DEAD_ZONE && dt>0) {
          long rangeL = nudge_center - DEAD_ZONE;
          long rangeR = (1023 - nudge_center) - DEAD_ZONE;
          long range  = (rangeL < rangeR ? rangeL : rangeR);
          if (range < 1) range = 1;
          long eff = ad - DEAD_ZONE; if (eff>range) eff=range;
          long speed_tenths = (long)eff * eff * (long)NUDGE_MAX_SPEED_TENTHS / (range*range);
          long signedSpeed = (d<0) ? -speed_tenths : speed_tenths;
          accum01 += signedSpeed * (long)dt;
          long steps = accum01 / 1000; accum01 -= steps*1000;
          if (steps) {
            bpm += (int)steps;
            if (bpm<MINIMUM_BPM) bpm=MINIMUM_BPM;
            if (bpm>MAXIMUM_BPM) bpm=MAXIMUM_BPM;
            updateBpm();
          }
        }
      }

      // A1: short/long (+ optional double-click)
      if (a1BlockUntilRelease) { if (!a1Held) a1BlockUntilRelease=false; }
      else {
        if (a1Held && !a1Pressed) { a1Pressed=true; a1T0=nowMs; calFired=false; }
        if (a1Held && a1Pressed && !calFired && (nowMs - a1T0) >= CAL_LONG_MS) {
          runCenterCalibration(); calFired=true;
          #ifdef ENABLE_A1_DOUBLECLICK
            a1SinglePending=false;
          #endif
        }
        if (!a1Held && a1Pressed) {
          unsigned long dur = nowMs - a1T0; a1Pressed=false;
          if (!calFired && dur>=SHORT_PRESS_MIN_MS) {
#ifdef ENABLE_A1_DOUBLECLICK
            if (!a1SinglePending) {
              a1SinglePending = true; a1FirstClickMs = nowMs;
            } else {
              if ((nowMs - a1FirstClickMs) <= DOUBLE_CLICK_MAX_MS) {
                a1SinglePending = false;
                startFromBar1();
                lastShortActionMs = nowMs;
              } else {
                a1FirstClickMs = nowMs; // restart window
              }
            }
#else
            if ((nowMs-lastShortActionMs)>=SHORT_PRESS_COOLDOWN_MS) {
              startOrStop(); lastShortActionMs=nowMs;
            }
#endif
          }
        }
      }
    }
#ifdef ENABLE_SYNC
  } else {
    // SYNC menu (вырезается, если ENABLE_SYNC закомментирован)
    if (tapHeld && a1Held) {
      if (!bothPressedMenu) { bothPressedMenu=true; bothT0Menu=nowMs; }
      if ((nowMs - bothT0Menu) >= SYNC_MENU_HOLD_MS) {
        inSyncMenu=false; bothPressedMenu=false; tapPressedMenu=false; a1PressedMenu=false; tapHeldMenu=false;
        #ifdef TM1637_DISPLAY
          showBpm(bpm);
        #endif
      }
    } else {
      if (bothPressedMenu) { bothPressedMenu=false; }
      if (tapHeld && !tapHeldMenu) { tapHeldMenu=true; tapHeldStartMs=nowMs; }
      if (!tapHeld && tapHeldMenu)  { tapHeldMenu=false; }
      if (tapHeldMenu && (nowMs - tapHeldStartMs) >= SYNC_SUBPAGE_TAP_HOLD_MS) {
        tapHeldMenu=false; syncMenuPolarityMode = !syncMenuPolarityMode; syncMenuLastActivityMs = nowMs;
        #ifdef TM1637_DISPLAY
          if (syncMenuPolarityMode) showSyncPol();
          else                      showSyncDivIdx(syncDivIndex);
        #endif
      }
      if (tapHeld && !tapPressedMenu) { tapPressedMenu=true; tapDownMs=nowMs; }
      if (!tapHeld && tapPressedMenu) {
        unsigned long dur = nowMs - tapDownMs; tapPressedMenu=false;
        if (dur >= SHORT_PRESS_MIN_MS && !a1Held) {
          syncMenuLastActivityMs = nowMs;
          if (!syncMenuPolarityMode) {
            uint8_t idx = (syncDivIndex + 1) & 0x03; applyDiv(idx);
          } else {
            applyPol(!syncPolarityInv);
          }
        }
      }
      if (a1Held && !a1PressedMenu) { a1PressedMenu=true; a1DownMs=nowMs; }
      if (!a1Held && a1PressedMenu) {
        unsigned long dur = nowMs - a1DownMs; a1PressedMenu=false;
        if (dur >= SHORT_PRESS_MIN_MS && !tapHeld) {
          syncMenuLastActivityMs = nowMs;
          if (!syncMenuPolarityMode) {
            uint8_t idx = (syncDivIndex + 3) & 0x03; applyDiv(idx);
          } else {
            applyPol(!syncPolarityInv);
          }
        }
      }
      if ((nowMs - syncMenuLastActivityMs) >= SYNC_MENU_IDLE_EXIT_MS) {
        inSyncMenu=false;
        #ifdef TM1637_DISPLAY
          showBpm(bpm);
        #endif
      }
    }
  }
#endif

#ifdef ENABLE_A1_DOUBLECLICK
  if (a1SinglePending && (nowMs - a1FirstClickMs) > DOUBLE_CLICK_MAX_MS &&
      (nowMs - lastShortActionMs) >= SHORT_PRESS_COOLDOWN_MS) {
    startOrStop();
    lastShortActionMs = nowMs;
    a1SinglePending = false;
  }
#endif

  if (bpmDirty && (millis() - bpmLastChangeMs) >= 1000) {
    eepromWriteWordIfChanged(EEPROM_ADDR_BPM_MSB, EEPROM_ADDR_BPM_LSB, (uint16_t)bpm);
    bpmDirty=false;
  }
}

/* ===== ISR/services ===== */
void tapInput() {
#ifdef ENABLE_SYNC
  if (suppressTap || inSyncMenu) return;
#else
  if (suppressTap) return;
#endif
  long now = micros();
  if (now - lastTapTime < minimumTapInterval) return;
  if (timesTapped == 0) firstTapTime = now;
  timesTapped++; lastTapTime = now;
}

void startOrStop() {
#ifdef TM1637_DISPLAY
  if (!playing) {
    hardResyncPhase();
    sendSPPZero();
    MIDI_SERIAL.write(MIDI_START);
    showSegs(WORD_PLAY, 300);
    showBpm(bpm);
  } else {
    MIDI_SERIAL.write(MIDI_STOP);
    digitalWrite(BLINK_OUTPUT_PIN, BLINK_OFF);
    blinkCount = 0;
    #ifdef ENABLE_BEAT_SEG
      setBeatSeg(false);                // почему: исключить «зависание» сегмента при STOP
    #endif
    #ifdef ENABLE_SYNC
      digitalWrite(SYNC_OUTPUT_PIN, SYNC_OFF_L);
      syncPulseOn = false;
      syncTickCounter = 0;
    #endif
    showSegs(WORD_STOP, 300);
    showBpm(bpm);
  }
#else
  if (!playing) { hardResyncPhase(); sendSPPZero(); MIDI_SERIAL.write(MIDI_START); }
  else          { MIDI_SERIAL.write(MIDI_STOP); digitalWrite(BLINK_OUTPUT_PIN, BLINK_OFF); blinkCount = 0;
    #ifdef ENABLE_BEAT_SEG
      setBeatSeg(false);
    #endif
  }
#endif
  playing = !playing;
}

#ifdef ENABLE_A1_DOUBLECLICK
void startFromBar1() {
  hardResyncPhase();
  sendSPPZero();
  MIDI_SERIAL.write(MIDI_START);
  playing = true;
#ifdef TM1637_DISPLAY
  showSegs(WORD_PLAY, 250);
  showBpm(bpm);
#endif
}
#endif

void sendClockPulse() {
#if HAVE_HW_MIDI
  if (!clockOnlyWhenPlay || playing) {
    MIDI_SERIAL.write(MIDI_TIMING_CLOCK);

    blinkCount = (blinkCount + 1) % CLOCKS_PER_BEAT;
    if (blinkCount == 0) {
      digitalWrite(BLINK_OUTPUT_PIN, BLINK_ON);
      #if defined(TM1637_DISPLAY) && defined(ENABLE_BEAT_SEG)
        #ifdef BEAT_SEG_ONLY_WHEN_PLAY
          if (playing) beatPulseRequest = true;   // только при PLAY
        #else
          beatPulseRequest = true;                // всегда, когда есть клок
        #endif
      #endif
    } else if (blinkCount == BLINK_TIME) {
      digitalWrite(BLINK_OUTPUT_PIN, BLINK_OFF);
    }

  #ifdef ENABLE_SYNC
    if (++syncTickCounter >= ticksPerPulse) {
      syncTickCounter = 0;
      digitalWrite(SYNC_OUTPUT_PIN, SYNC_ON_L);
      syncPulseOn = true;
      syncOffAtUs = (unsigned long)micros() + SYNC_PULSE_WIDTH_US;
    }
  #endif

  } else {
    blinkCount = 0;
    if (digitalRead(BLINK_OUTPUT_PIN) != BLINK_OFF) digitalWrite(BLINK_OUTPUT_PIN, BLINK_OFF);
  #ifdef ENABLE_SYNC
    syncTickCounter = 0;
    if (syncPulseOn) { digitalWrite(SYNC_OUTPUT_PIN, SYNC_OFF_L); syncPulseOn = false; }
  #endif
  }
#endif
}

void updateBpm() {
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
#ifdef TM1637_DISPLAY
  showBpm(bpm);
#endif
  bpmDirty = true; bpmLastChangeMs = millis();
}

long calculateIntervalMicroSecs(int bpm_tenths) {
  return 60L * 1000 * 1000 * 10 / bpm_tenths / CLOCKS_PER_BEAT;
}

/* ==== Nudge center / EEPROM ==== */
void loadCenter() {
  int msb = EEPROM.read(EEPROM_ADDR_CENTER_MSB);
  int lsb = EEPROM.read(EEPROM_ADDR_CENTER_LSB);
  int c = (msb<<8) | lsb;
  nudge_center = (c>=0 && c<=1023) ? c : CENTER_INIT_DEFAULT;
}
void saveCenter(int c) {
  eepromWriteWordIfChanged(EEPROM_ADDR_CENTER_MSB, EEPROM_ADDR_CENTER_LSB, (uint16_t)c);
}
void runCenterCalibration() {
  long sum=0; for (int i=0;i<64;i++){ sum += analogRead(DIMMER_CHANGE_PIN); delay(2); }
  int c = (int)(sum/64);
  if (c<50) c=50; if (c>973) c=973;
  nudge_center = c; saveCenter(c);
#ifdef TM1637_DISPLAY
  showSegs(WORD_CAL, 500);
  showBpm(bpm);
#endif
}

/* ==== Flags ==== */
void loadFlags() {
  uint8_t f = EEPROM.read(EEPROM_ADDR_FLAGS);
  if (f==0xFF) {
    nudgeEnabled=true; clockOnlyWhenPlay=false;
  #ifdef ENABLE_SYNC
    syncDivIndex=1; syncPolarityInv=false; refreshSyncLevels();
  #endif
    saveFlags();
  } else {
    nudgeEnabled       = (f & FLAG_NUDGE_ENABLED) != 0;
    clockOnlyWhenPlay  = (f & FLAG_CLOCK_ONLY_WHEN_PLAY) != 0;
  #ifdef ENABLE_SYNC
    syncDivIndex       = (uint8_t)((f & FLAG_SYNC_DIV_MASK) >> 2) & 0x03;
    if (syncDivIndex>3) syncDivIndex=1;
    syncPolarityInv    = (f & FLAG_SYNC_POL_INV) != 0;
    refreshSyncLevels();
  #endif
  }
}
void saveFlags() {
  uint8_t f = EEPROM.read(EEPROM_ADDR_FLAGS);
  f &= ~(FLAG_NUDGE_ENABLED | FLAG_CLOCK_ONLY_WHEN_PLAY | FLAG_SYNC_DIV_MASK | FLAG_SYNC_POL_INV);
  if (nudgeEnabled)      f |= FLAG_NUDGE_ENABLED;
  if (clockOnlyWhenPlay) f |= FLAG_CLOCK_ONLY_WHEN_PLAY;
#ifdef ENABLE_SYNC
  f |= (uint8_t)((syncDivIndex & 0x03) << 2);
  if (syncPolarityInv)   f |= FLAG_SYNC_POL_INV;
#endif
  eepromWriteIfChanged(EEPROM_ADDR_FLAGS, f);
}

/* clear SYNC bits if SYNC stripped */
void ensureSyncBitsClearedIfDisabled() {
#ifndef ENABLE_SYNC
  uint8_t f = EEPROM.read(EEPROM_ADDR_FLAGS);
  uint8_t cleared = f & ~(FLAG_SYNC_DIV_MASK | FLAG_SYNC_POL_INV);
  eepromWriteIfChanged(EEPROM_ADDR_FLAGS, cleared);
#endif
}

/* ==== Display helpers ==== */
#ifdef TM1637_DISPLAY
void showSegs(const uint8_t w[4], unsigned d_ms){
  segs[0]=w[0]; segs[1]=w[1]; segs[2]=w[2]; segs[3]=w[3];
  display.setSegments(segs);
  delay(d_ms);
}
void showBpm(int bpm_tenths){
  int whole = bpm_tenths/10;
  segs[0] = (whole>=1000)? display.encodeDigit((whole/1000)%10):0x00;
  segs[1] = (whole>= 100)? display.encodeDigit((whole/100)%10):0x00;
  segs[2] = (whole>=  10)? display.encodeDigit((whole/10)%10):0x00;
  segs[3] = display.encodeDigit(whole%10) & 0x7F;   // без DP
  #ifdef ENABLE_BEAT_SEG
    if (beatSegOn) segs[0] |= SEG_G;               // средний сегмент у левой цифры
  #endif
  display.setSegments(segs);
}
  #ifdef ENABLE_BEAT_SEG
void setBeatSeg(bool on){
  beatSegOn = on;
  if (on)  segs[0] |= SEG_G;
  else     segs[0] &= (uint8_t)~SEG_G;
  display.setSegments(segs);
}
  #endif

#ifdef ENABLE_SYNC
void showSyncDivIdx(uint8_t idx){
  segs[0] = SEG_S_; segs[1] = 0x00;
  if (idx==3) { segs[2]=display.encodeDigit(1); segs[3]=display.encodeDigit(6); }
  else { segs[2]=0x00; uint8_t num = (idx==0)?2 : (idx==1)?4 : 8; segs[3]=display.encodeDigit(num); }
  display.setSegments(segs);
}
void showSyncPol(){
  if (syncPolarityInv) { segs[0]=SEG_S_; segs[1]=SEG_P; segs[2]=SEG_O_; segs[3]=SEG_N_; } // SPON
  else                 { segs[0]=SEG_S_; segs[1]=SEG_P; segs[2]=SEG_O_; segs[3]=SEG_F_; } // SPOF
  display.setSegments(segs);
}
#endif
#endif