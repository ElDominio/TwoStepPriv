uint32_t times = 0;
uint32_t timeOld = 0;

uint16_t cutRPM = 5000;
uint16_t cutRPMhyst = 100;
uint16_t RPM = 0;

uint8_t cylCount = 4;
uint8_t bitField = B00001001;
uint8_t triggerCounter = 0;

void RPMcounter();

#define BIT_CLUTCH_LOGIC  0
#define BIT_CUT_ACTIVE    1
#define BIT_ARM_TRIGGER   2
#define BIT_SPARK_STATE   3

#include "pins.h"
#include <LiquidCrystal.h>

/*
#define MAP_INPUT   PD2
#define TPS_INPUT   PC4
#define SPARK_OUT    PD4
#define ARM_TRG_PIN PD3
#define RED_LED_PIN PB5
#define BLU_LED_PIN PB4
#define RPM_IN_PIN PC3

*/

  LiquidCrystal (lcd, PC7,PC6, PC5, PC4, PA2, PA1, PA3);
  
void setup() {
  // put your setup code here, to run once:
  pinMode(SPARK_OUT, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(ARM_TRG_PIN, INPUT_PULLUP);
  GPIO_Init(GPIOA, GPIO_PIN_3, GPIO_MODE_IN_PU_IT);
  Serial_begin(115200);

  lcd_begin(16, 2);
 // lcd_clear();
  lcd_print_s("START HYST  END");
  lcd_setCursor(0,1);
  lcd_print_u(cutRPM);
  lcd_setCursor(6,1);
  lcd_print_u(cutRPMhyst);
  lcd_setCursor(12,1);
  lcd_print_u(cutRPM - cutRPMhyst);
  
  
  EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOA, EXTI_SENSITIVITY_FALL_ONLY); 
  EXTI_SetTLISensitivity(EXTI_TLISENSITIVITY_FALL_ONLY);
  
  enableInterrupts();
  
}

void loop() {
  // put your main code here, to run repeatedly:
   if (bitRead(bitField, BIT_CLUTCH_LOGIC)){

    bitWrite(bitField,BIT_ARM_TRIGGER,digitalRead(ARM_TRG_PIN));

  }
  else{  bitWrite(bitField,BIT_ARM_TRIGGER,!digitalRead(ARM_TRG_PIN));}
  
  if (triggerCounter > 0){
    RPMcounter();
  }
  if (!bitRead(bitField, BIT_ARM_TRIGGER)){
      bitSet(bitField, BIT_SPARK_STATE);
      digitalWrite(BLU_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      bitClear(bitField, BIT_CUT_ACTIVE);
      //Serial.println("Coil is On");
    }
    else{
      ignControl();
    }
    
  //Write ignition control out
    digitalWrite(SPARK_OUT, bitRead(bitField, BIT_SPARK_STATE));
}


void ignControl(){
    if (RPM > cutRPM){
    bitClear(bitField, BIT_SPARK_STATE);
    bitSet(bitField,BIT_CUT_ACTIVE);
    digitalWrite(BLU_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
  }
  else if (RPM < (cutRPM - cutRPMhyst)){
    digitalWrite(BLU_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    bitClear(bitField,BIT_CUT_ACTIVE);
    bitSet(bitField, BIT_SPARK_STATE);
  }
}
void RPMcounter(){
  
  times = micros()-timeOld;        //finds the time 
  RPM = (30108000/times);         //calculates rpm
  //Serial.println(RPM);
  RPM = RPM*(cylCount);
  timeOld = micros();
  
  triggerCounter = 0;
}
