
#include "src/AlcDisplay/AlcDisplay.h"
AlcDisplay display(PIN_PD4 /*Bit0*/, PIN_PD5 /*Bit1*/, PIN_PD6 /*Bit2*/, PIN_PD7 /*Bit3*/, PIN_PF0 /*strobeA*/, PIN_PF2 /*strobeB*/, PIN_PF1 /*cmd*/, PIN_PF3 /*reset*/);



// ---------- Pins ----------
#define MUX_A0   PIN_PA0
#define MUX_A1   PIN_PA1
#define MUX_A2   PIN_PA2
#define MUX_EN1  PIN_PA3   // INH IC1: LOW = aktiv
#define MUX_EN2  PIN_PA4   // INH IC2: LOW = aktiv
#define I_LADE   PIN_PG1
#define RES      PIN_PE2
#define ADC_IN   PIN_PD3

#define LADEN1   PIN_PC0
#define ENTL1   PIN_PC4
#define PWM1   PIN_PE3
#define LADEN2   PIN_PC1
#define ENTL2   PIN_PC5
#define PWM2   PIN_PE4
#define LADEN3   PIN_PC2
#define ENTL3   PIN_PC6
#define PWM3   PIN_PE5
#define LADEN4   PIN_PC3
#define ENTL4   PIN_PC7
#define PWM4   PIN_PB5

// ---------- Konfiguration ----------
bool     RES_AKTIV    = HIGH;   // Pegel, bei dem C1 kurzgeschlossen ist
bool     INVERT_PEGEL = HIGH;   // Pegel an I_LADE, bei dem der Inverter IC5B benutzt wird
uint8_t  refMux       = 1;      // Referenz -2,5 V an IC1 ...
uint8_t  refKanal     = 6;      // ... Kanal 6
const float U_REF = 2.5;

// Timer1 läuft mit F_CPU/8
const uint32_t TICKS_PRO_S = F_CPU / 8;
const uint16_t T1_TICKS    = TICKS_PRO_S / 50;   // 20 ms = eine Netzperiode
const uint16_t T2_TIMEOUT  = 65000;              // max. T2 (Ticks)

// Ergebnis-Codes
const int32_t ERR_POLARITAET = -1;   // Komparator schon in Phase 1 gekippt
const int32_t ERR_TIMEOUT    = -2;   // Phase 2 kippt nie

// ---------- Port-Zugriff ----------
static volatile uint8_t *resReg, *iladeReg, *adcPinReg;
static uint8_t resMask, iladeMask, adcMask;

static inline void setPin(volatile uint8_t *reg, uint8_t mask, bool high) {
  if (high) *reg |= mask; else *reg &= ~mask;
}
static inline bool readAdc() { return (*adcPinReg & adcMask) != 0; }

// PA0..PA2 = Kanal, PA3 = EN1, PA4 = EN2 (HIGH = gesperrt)
static inline uint8_t muxBits(uint8_t mux, uint8_t kanal) {
  return (kanal & 7) | (mux == 1 ? _BV(4) : _BV(3));
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("AlcDisplay"));
  display.reset();
  
  pinMode(MUX_A0, OUTPUT);
  pinMode(MUX_A1, OUTPUT);
  pinMode(MUX_A2, OUTPUT);
  pinMode(MUX_EN1, OUTPUT);
  pinMode(MUX_EN2, OUTPUT);
  pinMode(I_LADE, OUTPUT);
  pinMode(RES, OUTPUT);
  pinMode(ADC_IN, INPUT_PULLUP);

  pinMode(LADEN1,OUTPUT);
  pinMode(ENTL1,OUTPUT);
  pinMode(PWM1,OUTPUT);
  digitalWrite(LADEN1, HIGH);
  digitalWrite(ENTL1, LOW);
  analogWrite(PWM1, 0);

  delay(100);

  digitalWrite(ENTL1, HIGH);
  analogWrite(PWM1, 50);

  resReg    = portOutputRegister(digitalPinToPort(RES));
  resMask   = digitalPinToBitMask(RES);
  iladeReg  = portOutputRegister(digitalPinToPort(I_LADE));
  iladeMask = digitalPinToBitMask(I_LADE);
  adcPinReg = portInputRegister(digitalPinToPort(ADC_IN));
  adcMask   = digitalPinToBitMask(ADC_IN);

  setPin(resReg, resMask, RES_AKTIV);
  setPin(iladeReg, iladeMask, !INVERT_PEGEL);
  PORTA |= _BV(3) | _BV(4);          // beide Muxe gesperrt

  TCCR1A = 0;
  TCCR1B = _BV(CS11);                // Prescaler 8
}

// ---------- Eine Dual-Slope-Messung ----------
// Rückgabe: T2 in Ticks (>0) oder ERR_POLARITAET / ERR_TIMEOUT
int32_t dualSlope(uint8_t mux, uint8_t kanal, bool invert, uint16_t *t1ist) {
  const uint8_t hi   = PORTA & 0xE0;
  const uint8_t mess = hi | muxBits(mux, kanal);
  const uint8_t ref  = hi | muxBits(refMux, refKanal);

  // 1) Reset, Kanal und Polarität einstellen, einschwingen lassen
  setPin(resReg, resMask, RES_AKTIV);
  setPin(iladeReg, iladeMask, invert ? INVERT_PEGEL : !INVERT_PEGEL);
  PORTA = mess;
  delay(5);
  const bool idle = readAdc();

  // 2) Phase 1: Messspannung für feste Zeit T1 integrieren
  noInterrupts();
  const uint16_t t0 = TCNT1;
  setPin(resReg, resMask, !RES_AKTIV);
  while ((uint16_t)(TCNT1 - t0) < T1_TICKS) { }
  const bool gekippt = (readAdc() != idle);

  // 3) Phase 2: Referenz aufschalten (nie invertiert)
  setPin(iladeReg, iladeMask, !INVERT_PEGEL);
  PORTA = ref;
  const uint16_t t1 = TCNT1;
  *t1ist = (uint16_t)(t1 - t0);

  int32_t ergebnis = ERR_TIMEOUT;
  if (gekippt) {
    ergebnis = ERR_POLARITAET;
  } else {
    while ((uint16_t)(TCNT1 - t1) < T2_TIMEOUT) {
      if (readAdc() != idle) {                 // Nulldurchgang
        ergebnis = (uint16_t)(TCNT1 - t1);
        break;
      }
    }
  }
  interrupts();

  // 4) Aufräumen
  setPin(resReg, resMask, RES_AKTIV);
  setPin(iladeReg, iladeMask, !INVERT_PEGEL);
  PORTA |= _BV(3) | _BV(4);
  return ergebnis;
}

// ---------- Spannung in Volt, vorzeichenrichtig; NAN bei Fehler ----------
float messeSpannung(uint8_t mux, uint8_t kanal) {
  uint16_t t1;
  bool negativ = false;
  const float GAIN = 1.08;   // ≈ 1/0,925, vorläufig

  int32_t t2 = dualSlope(mux, kanal, false, &t1);
  if (t2 == ERR_POLARITAET) {                  // negativ -> mit Inverter wiederholen
    negativ = true;
    t2 = dualSlope(mux, kanal, true, &t1);
    if (t2 == ERR_POLARITAET) return 0.0;      // in beiden Richtungen "falsch" -> ~0 V
  }
  if (t2 < 0) return NAN;                      // Timeout

  float d = U_REF * (float)t2 / (float)t1;                 // Rohbetrag
  float u = negativ ? (d + 0.081) / 0.944 : (d + 0.080) / 0.962;
  return negativ ? -u : u;
}
void loop() {

  display.setVoltage(messeSpannung(2, 1)*13);
  display.setCurrent(messeSpannung(2, 0)*1.62);
  
  //display.setCapacity(messeSpannung(2, 5)*13);
  delay(500);
}
