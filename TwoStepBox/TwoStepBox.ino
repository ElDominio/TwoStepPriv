/* Memory Mapping
 *  0 - mode
 *  1 - timeToCut
 *  2,3 - high/lowByte of highRPM
 *  4,5 - high/lowByte of lowRPM
 */

#include "normanTools.h"

//Pin assignments
byte RPMinputPin = 3;
byte SparkOutput = 7;
byte ArmTriggerPin = 5;
byte bluePin = 9;
byte redPin = 10;

byte analogIn = A4;

byte pinOne = A0;
byte pinTwo = A1;
byte pinThree = A2;
byte pinFour = A3;

//Program variables
unsigned int cutRPM;

unsigned int timeToCut; 
//int armVoltage;
byte positionCheckcount = 0;
byte potPosition;
byte cylCount = 4;
byte RPMerror = 0;

byte bitField = B11000101; //field of working bits for states; first bit is 7, last is 0

#define BIT_POS_REP_FLAG  0 //Position Report enabled
#define BIT_DIAG_REQUEST  1 
#define BIT_CLUTCH_LOGIC  2
#define BIT_CUT_ACTIVE    3
#define BIT_ARM_TRIGGER   4
#define BIT_SPARK_STATE   5
#define BIT_TOYOTA_FAKE   6
#define BIT_RPM_FILTER    7


//Working Variables
unsigned int RPM;
unsigned int RPMold;
unsigned long times;
unsigned long timeOld;
byte oldPotPos = 0;
//bool correctionFlag = false;
byte trigCounter;
unsigned long positionReport = 0;

//toyota function
unsigned int toyotaDivision = 0;
uint16_t timeCount = 0;
unsigned long timeTick = 0;
byte fakePin = 2;
byte fakePinTwo = 4;
bool fakeOut = false;
byte toyoCyl = 6;

void setup() {
	//Begin Serial comms
	Serial.begin(115200);

	//Set I/O
	pinMode(SparkOutput, OUTPUT);
 pinMode(bluePin,OUTPUT);
 pinMode(redPin,OUTPUT);
 pinMode(fakePin, OUTPUT);
	pinMode(ArmTriggerPin, INPUT_PULLUP);
	pinMode(RPMinputPin, INPUT);
  pinMode(pinOne, INPUT_PULLUP);pinMode(pinTwo, INPUT_PULLUP);pinMode(pinThree, INPUT_PULLUP);pinMode(pinFour, INPUT_PULLUP);
	digitalWrite(SparkOutput, HIGH); //activate spark output

  digitalWrite(bluePin,HIGH);
   Serial.println("Box Ready");
//	 Serial.println(bitField);
   	//Initialize variables
	 RPM = 0;
	 RPMold = 0;
	 trigCounter = 0;
	 timeOld = 0;
	 times = 0;
  cutRPM = 4000;
	 /*armTriggerbool = false;
	 bitSet(bitField, BIT_SPARK_STATE);*/
  //bitClear(bitField, BIT_DIAG_REQUEST);
  //bitSet(bitField, BIT_RPM_FILTER);
  //Serial.println(bitField);
  positionCheckcount = 0;
  trigCounter = 0;
  timeToCut = 5;
  bitSet(bitField, BIT_SPARK_STATE);
	//cylCount = EEPROM.read(70);
  potPosition = (8*!digitalRead(pinFour)) + (4*!digitalRead(pinThree)) +(2*!digitalRead(pinTwo)) +(!digitalRead(pinOne)) ;
  if (EEPROM.read(255) != 23){
    writeDefaultSettings();
  }
 loadCalibration();
 
 
  if (cylCount > 8){
    cylCount = 4;
  }

	 //Attach RPM input interrupt
  attachInterrupt(digitalPinToInterrupt(3), triggerCounter, RISING);
}

void loop() {
  //Serial.println(bitRead(bitField, BIT_RPM_FILTER));
  if ((bitRead(bitField, BIT_RPM_FILTER)) && ((micros() - timeOld) > 4000)){
    attachInterrupt(digitalPinToInterrupt(3), triggerCounter, RISING);
    //Serial.println("Filter ok");
  }

  if (bitRead(bitField, BIT_TOYOTA_FAKE)){
    toyotaFunction();
   // Serial.println("Toyta");
  }
  
  if (bitRead(bitField, BIT_CLUTCH_LOGIC)){

    bitWrite(bitField,BIT_ARM_TRIGGER,digitalRead(ArmTriggerPin));

  }
  else{  bitWrite(bitField,BIT_ARM_TRIGGER,!digitalRead(ArmTriggerPin));}
  //Serial.println(digitalRead(ArmTriggerPin));


  if (trigCounter > 0){
    RPMcounter();
  }
  if ((micros()- timeOld) > 670180){
    RPM = 0;
  }

    if (!bitRead(bitField, BIT_ARM_TRIGGER)){
      bitSet(bitField, BIT_SPARK_STATE);
      digitalWrite(bluePin, HIGH);
      digitalWrite(redPin, LOW);
      bitClear(bitField, BIT_CUT_ACTIVE);
      //Serial.println("Coil is On");
    }
    else{
      ignControl();
    }
     //Write ignition control out
    digitalWrite(SparkOutput, bitRead(bitField, BIT_SPARK_STATE));
   // Serial.println(bitRead(bitField, BIT_SPARK_STATE));

    if (Serial.available()){
      SerialComms();
    }

    
    if (bitRead(bitField,BIT_DIAG_REQUEST)){
      SerialDiag();
    }
    if (millis() % 2){
      positionCheckcount++;
    }
    if (positionCheckcount > 250){
      oldPotPos = potPosition;
      potPosition = (8*!digitalRead(pinFour)) + (4*!digitalRead(pinThree)) +(2*!digitalRead(pinTwo)) +(!digitalRead(pinOne)) ;
     //potPosition = positionCheck();
     if (oldPotPos != potPosition){
      loadCalibration();
     }
      positionCheckcount = 0;
    }
}

void toyotaFunction(){
	if(micros() > timeTick){
		toyotaDivision = times/toyoCyl;
		timeTick = micros();
		timeCount++;
    //Serial.print(timeCount); Serial.print(" > "); Serial.println(toyotaDivision);
		if(timeCount > toyotaDivision){
			timeCount = 0;
			//bitSet(bitField, BIT_TOYOTA_FAKE);
         // Serial.println(fakeOut);
      if (!bitRead(bitField, BIT_SPARK_STATE)){
        if (toyoCyl == 4){
          digitalWrite(fakePin, HIGH);
        }
        else if (toyoCyl == 6){
          fakeOut = !fakeOut;
          digitalWrite(fakePin, fakeOut);
  			  digitalWrite(fakePinTwo, !fakeOut);
        }
      }
		}
		if(timeCount > 1000){
			//bitClear(bitField, BIT_TOYOTA_FAKE);
        //  Serial.println(bitRead(bitField, BIT_TOYOTA_FAKE));
			digitalWrite(fakePin, LOW);
      digitalWrite(fakePinTwo, LOW);
		}
	}
}

void SerialDiag(){
  unsigned int posRep = millis() - positionReport;
  
  if (posRep >= 3000){
    Serial.print("Pot POsition: "); Serial.println(potPosition);
    Serial.print("cutRPM: "); Serial.println(cutRPM);
    Serial.print("RPM hysterisis: "); Serial.print(timeToCut);Serial.println("ms");
    Serial.print("Clutch Armed: ");Serial.println(bitRead(bitField,BIT_ARM_TRIGGER));
    Serial.print("CylMode: ");Serial.println(cylCount);
    //Serial.print("AnalogIn: ");Serial.println(armVoltage);
     Serial.print("Ignition Output: ");Serial.println(bitRead(bitField, BIT_SPARK_STATE));
     posRep = 0;
     positionReport = millis();
  }
  if (((posRep % 250) == 0) && (bitRead(bitField, BIT_POS_REP_FLAG))){
    Serial.print("RPM: "); Serial.println(RPM);
    bitClear(bitField, BIT_POS_REP_FLAG);
  }
  else if((posRep % 250) != 0){
    bitSet(bitField, BIT_POS_REP_FLAG);
  }
}

void SerialComms(){
  char commandChar = Serial.read();
  if (commandChar == 'w'){ //write command
    /* Memory Mapping
 *  0 - potposition
 *  1 - RPM hibyte
 *  2 - RPM lobyte
 *  3 - cutTime hi
 *  4 - cutTime lo
 *  Example Message:
 *  w0;205;50;
 */
    int i = 0;
    bool eepromFlag = false;
   /* Serial.println(F("Write mode activated, please enter the pot position (followed by a semicolon ';'"));
    Serial.println(F("then the desired cut RPMs, followed by a semicolon,"));
    Serial.println(F("and the the desired cut time, in ms, followed by a semicolon."));
    Serial.println(F("Ex. w14;3500;50; saves a 3500 RPM and 50ms cut to position 14"));*/
    while (i < 4){
      byte serialByte;
      int serialInt;
        while (i == 0){
          if (Serial.available()){
            serialByte = Serial.read();
            if( serialByte == 59){
              i++;
              //Serial.println("Breaking");
              break;
            }
            serialByte = serialByte - 48;
            serialByte = constrain(serialByte, 0, 10);
            potPosition = serialByte;
            Serial.print("i = "); Serial.print(i);Serial.print(" ");Serial.println(potPosition);
          }
        }
        while (i == 1){
         byte readRPM[4];
         byte digits = 0;
         while (digits < 4){
          if(Serial.available()){
            readRPM[digits] = Serial.read()-48;
            if (readRPM[digits] == 11){ break;}
            //Serial.println(digits);
           // Serial.println(readRPM[digits]);
            digits++;
          }
         }
          i++;
          cutRPM = 0;
          for (int i = 0; i < digits; i++){
           // Serial.print(cutRPM);Serial.print(" + ");Serial.println(readRPM[i]*powint(10,digits-i-1));
            cutRPM = cutRPM + readRPM[i]*powint(10,digits-i-1);
            
          }           
          //cutRPM = readRPM[0]*1000 + readRPM[1]*100 + readRPM[2]*10 +readRPM[3];
          cutRPM = constrain(cutRPM, 0, 8000);
           Serial.print("i = "); Serial.print(i);Serial.print(" ");Serial.println(cutRPM);
           if (Serial.peek() == 59){ Serial.read();}
        }
        while ( i == 2 ){
          byte readDC[4];
         byte digits = 0;
         while (digits < 4){
          if(Serial.available()){
            readDC[digits] = Serial.read()-48;
            if (readDC[digits] == 11){ break;}
            digits++;
          }
         }
          i++;
          timeToCut = 0;
          for (int i = 0; i < digits; i++){
            timeToCut = timeToCut + readDC[i]*powint(10,digits-i-1);
          }
            Serial.print("i = "); Serial.print(i);Serial.print(" ");Serial.print(timeToCut);Serial.print("rpm");
            Serial.println("");
            if (Serial.peek() == 59){ Serial.read();}
         
          }
      if ( i == 3){
          Serial.print("i = "); Serial.print(i);Serial.print(" ");
          burnCalibration(potPosition, cutRPM, timeToCut);
          i++; // i == 4
      }
    }// end while
  } // End write command
  
  else if (commandChar == 'r'){
    bitWrite(bitField, BIT_DIAG_REQUEST, !bitRead(bitField, BIT_DIAG_REQUEST)); // activate diag

    Serial.print("bitField = ");Serial.println(bitField, BIN);
    Serial.print("diagRequest = "); Serial.println(bitRead(bitField, BIT_DIAG_REQUEST));
  }
  else if (commandChar == 'l'){
    loadCalibration();
  }

 else if (commandChar == 'i'){

    bitWrite(bitField, BIT_CLUTCH_LOGIC, !(bitRead(bitField, BIT_CLUTCH_LOGIC)));
    EEPROM.update(71,(bitRead(bitField, BIT_CLUTCH_LOGIC)));
    Serial.println(F("Clutch logic inverted!"));
  }// end I case

  else if (commandChar == 't'){
    bitWrite(bitField, BIT_TOYOTA_FAKE, !(bitRead(bitField, BIT_TOYOTA_FAKE)));
    EEPROM.update(72,(bitRead(bitField, BIT_TOYOTA_FAKE)));
    Serial.print(F("Toyota output inverted:"));Serial.println(bitRead(bitField, BIT_TOYOTA_FAKE));
    /*char tempCyl = 0;
    Serial.println("Enter 'f' for 4 cylinder Toyota, or 's' for six cylinder Toyota");
    while ( tempCyl == 0){
       if (Serial.available()){tempCyl = Serial.read();}
       if (tempCyl == 'f'){
        toyoCyl = 4;
        EEPROM.update(74, 4);
        Serial.println("Four cyl set!");
       }
       else if (tempCyl == 's'){
        toyoCyl = 6;
        EEPROM.update(74, 6);
        Serial.println("Six cyl set!");
       }
       else{
        Serial.println("Please enter a valid value!");
        toyoCyl = 0;
        tempCyl = 0;
       }
    }*/
  }
  else if (commandChar == 'f'){
    bitWrite(bitField, BIT_RPM_FILTER, !(bitRead(bitField, BIT_RPM_FILTER)));
    EEPROM.update(73,(bitRead(bitField, BIT_RPM_FILTER)));
    Serial.print(F("RPM Filter inverted: "));Serial.println(bitRead(bitField, BIT_RPM_FILTER));
  }
  else if (commandChar == 'c'){
    byte i = 0;
    Serial.println(F("Enter 'd' for distributor, 'w' for wasted spark, or 'c' for Coil On Plug"));
    while (i < 1){
      if (Serial.available()){
        char serialRead = Serial.read();
        if (serialRead == 'd'){
          cylCount = 1;
          EEPROM.update(70,cylCount);
          i++;
        }
        else if (serialRead == 'w'){
          cylCount = 2;
          EEPROM.update(70,cylCount);
          i++;
        }
        else if (serialRead == 'c'){
          cylCount = 4;
          EEPROM.update(70,cylCount);
          i++;
        }
      } // end serial avail
    }// end while
    Serial.print(F("Spark mode set! Current mode is: "));Serial.println(cylCount);
  }// end C case
}

void triggerCounter(){
  if(bitRead(bitField, BIT_RPM_FILTER)){
    detachInterrupt(digitalPinToInterrupt(3));
  }
  trigCounter++;
  //Serial.println("TRIGGER");
}

void RPMcounter(){
 // Serial.print("micros "); Serial.print(micros());Serial.print("timeOld ");Serial.println(timeOld);
  times = micros()-timeOld;        //finds the time 
  //Serial.print("Times: "); Serial.println(times);
  RPM = (30108000/times);         //calculates rpm
  //Serial.println(RPM);
  RPM = RPM*(cylCount);
  if ((RPM > 0) && (RPM < 10000)){
    RPMold = RPM;
  }
  if ((RPM - RPMold) > 10000){
    RPM = RPMold;
  }
  timeOld = micros();
  
  
  trigCounter = 0;
}

void ignControl(){
  //Do ignition control methods

  if ((RPM > cutRPM) && !(bitRead(bitField, BIT_CUT_ACTIVE))){
    bitClear(bitField, BIT_SPARK_STATE);
    bitSet(bitField,BIT_CUT_ACTIVE);
    digitalWrite(bluePin, LOW);
    digitalWrite(redPin, HIGH);
  }
  else if ((RPM < (cutRPM - timeToCut)) && (bitRead(bitField, BIT_CUT_ACTIVE))){
    digitalWrite(bluePin, HIGH);
    digitalWrite(redPin, LOW);
    bitClear(bitField,BIT_CUT_ACTIVE);
    bitSet(bitField, BIT_SPARK_STATE);
  }
}


void loadCalibration(){
  byte tempPotPos = (8*!digitalRead(pinFour)) + (4*!digitalRead(pinThree)) +(2*!digitalRead(pinTwo)) +(!digitalRead(pinOne));
    Serial.print(("tempPotPos = "));Serial.println(tempPotPos);
    
    cutRPM = word(EEPROM.read(tempPotPos+tempPotPos), EEPROM.read(tempPotPos+tempPotPos+1));
    Serial.print("cutRPM: ");Serial.println(cutRPM);
    timeToCut = word(EEPROM.read(tempPotPos+tempPotPos+32), EEPROM.read(tempPotPos+tempPotPos+33));
    Serial.print("RPM hysterisis: ");Serial.println(timeToCut);
    cylCount = EEPROM.read(70);
    bitWrite(bitField, BIT_CLUTCH_LOGIC, EEPROM.read(71));
    bitWrite(bitField, BIT_TOYOTA_FAKE, EEPROM.read(72));
    bitWrite(bitField, BIT_RPM_FILTER, EEPROM.read(73));
   // toyoCyl = EEPROM.read(74);

}
