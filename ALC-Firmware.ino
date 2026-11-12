
#include "src/AlcDisplay/AlcDisplay.h"
AlcDisplay display(PIN_PD4 /*Bit0*/, PIN_PD5 /*Bit1*/, PIN_PD6 /*Bit2*/, PIN_PD7 /*Bit3*/, PIN_PF0 /*strobeA*/, PIN_PF2 /*strobeB*/, PIN_PF1 /*cmd*/, PIN_PF3 /*reset*/);

#include "AlcAdc.h"
AlcAdc adc( &PORTA,
            0b00000111, //PIN_PA0, PIN_PA1, PIN_PA2
            0b00011000, //PIN_PA3, PIN_PA4
            PIN_PG1,
            PIN_PE2,
            PIN_PD3 );


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



void setup() {
  //Serial.begin(115200);
  //Serial.println(F("AlcDisplay"));
  display.reset();
  adc.init();
  

  pinMode(LADEN1,OUTPUT);
  pinMode(ENTL1,OUTPUT);
  pinMode(PWM1,OUTPUT);
  digitalWrite(LADEN1, HIGH);
  digitalWrite(ENTL1, LOW);
  analogWrite(PWM1, 0);

  delay(100);

  digitalWrite(ENTL1, HIGH);
  analogWrite(PWM1, 20);


}


void loop() {

  display.setVoltage(adc.messeSpannung(1, 1, false)*13);
  display.setCurrent(adc.messeSpannung(1, 0, true)*1.62);
  
  display.setCapacity(adc.messeSpannung(0, 6, true));
  delay(500);
}
