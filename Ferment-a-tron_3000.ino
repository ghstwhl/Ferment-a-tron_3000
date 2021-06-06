/*********************

Controller module for the Ferment-a-Tron 3000

This software assumes a stack made of:
  - Seeeduino v3 - https://wiki.seeedstudio.com/Seeeduino_v3.0/
  - Generic LCD 1602 LCD shield https://github.com/ghstwhl/ePartners/tree/main/LC7001
    This uses A0 for buttons, in a janky value range sort of way.
  - 10k Thermistor - https://www.adafruit.com/product/372
    Thermistor is wired to A1
  - 4 Relay Module (http://amzn.to/1W7wsyP)

Had to chuck the datalogging becasue the SD card and the cheap LCD both use D10. Ugh.
#CheapPartsParadox

This version is in Celcius, cuz New Zealand!

Relays:
  IN1-IN4 are wired to D1-D4, respectively

Buttons:

  UP     - Thermostat temp up one degree
  DOWN   - Thermostat temp down one degree
  LEFT   - Drive +/- down one degree  
  RIGHT  - Drive +/- up one degree  
  SELECT - write settings to EEPROM



Controller code Copyright 2016, Chris O'Halloran

*********************************************************************************
Code related to thermistor readings is copyright Limor Fried, Adafruit Industries
and distributed under the MIT License.
Please keep attribution and consider buying parts from Adafruit.
https://learn.adafruit.com/thermistor/using-a-thermistor
*********************************************************************************


**********************/

#include <LiquidCrystal.h>
//LCD pin to Arduino https://github.com/ghstwhl/ePartners/tree/main/LC7001
LiquidCrystal lcd( 8,  9,  4,  5,  6,  7);

// BEGIN - load EEPROM library so we have persistent storage
#include <EEPROM.h>
// END - load EEPROM library so we have persistent storage


// BEGIN - defines for control of the relays
// These defines set the relay control pins.  They are here to make it easier if someone wires their stack differently.
#define RELAY1 0
#define RELAY2 1
#define RELAY3 2
#define RELAY4 3
// END - defines for control of the relays

// https://learn.adafruit.com/thermistor/using-a-thermistor
//// These defines are used in the read_temp_f()  function.
// Defines related to the thermistor being used for temperature readings
// which analog pin to connect
#define THERMISTORPIN A1       
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000      
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25   
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 100
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10000    
// Let's pre-define our termperature reading subroutine
float read_temp_c(void);
float temp;
void statusDisplay(void);


// This structure is used to read adn write values from EEPROM
struct SettingsObject {
  int setTemperature;
  int driftAllowed;
  int checksum;
};
// This prototype is for the function that reads the above structure from the EEPROM
void readSettingsFromEEPROM(void);
// This prototype is for the function that writes the above structure from the EEPROM
void writeSettingsToEEPROM(void);
// Global storage for the temp and drift settings
int setTemperature;
int driftAllowed;
unsigned long ticktock;

//Making the variable for button status global, rather than create and destroy it every time we call buttonCheck()
int buttons;
void buttonCheck(void);
void relayControl(void);


void setup() {

  // BEGIN - initialize the LCD and wipe it clean
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0,0);
  // END - initialize the LCD and wipe it clean


  // BEGIN - setup A1 for temperature readings, and try to load any stored settings
  //         IF there are NO stored settings, we need A0 ready so that the readSettingsFromEEPROM()
  //         function can use the current temp as the default.
  pinMode(A1, INPUT);
  readSettingsFromEEPROM();
  // END - setup A0 for temperature readings, and try to load any stored settings
  
  // BEGIN - Let's get our relays prepped for action.
  pinMode(RELAY1, OUTPUT);
  digitalWrite(RELAY1,HIGH);

  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY2,HIGH);
    
  pinMode(RELAY3, OUTPUT);
  digitalWrite(RELAY3,HIGH);

  pinMode(RELAY4, OUTPUT);
  digitalWrite(RELAY4,HIGH);
  // END - Let's get our relays prepped for action.

  // THIS REQUIRES CONNECTING 3.3v to AREF
  analogReference(EXTERNAL);


}

void loop() {
  temp = read_temp_c();
  statusDisplay();
  for (int i=0; i < 100; i+=10) {
    buttonCheck();
    delay(10);
  }
  relayControl();
}





void statusDisplay() {
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Temp:");
  lcd.print(temp);
  lcd.print("C   ");

  lcd.setCursor(0, 1);
  lcd.print("Set: ");
  lcd.print(setTemperature);
  lcd.print(F("C +/-"));
  lcd.print(driftAllowed);
  lcd.print("   ");
  
}




void readSettingsFromEEPROM() {

  SettingsObject tempVar; //Variable to store custom object read from EEPROM.
  EEPROM.get(0, tempVar);

  if ( ((tempVar.setTemperature + tempVar.driftAllowed + 69) * 42) != tempVar.checksum) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Setting defaults");
    setTemperature = (int)read_temp_c();
    driftAllowed = 1;

    lcd.setCursor(0, 1);
    lcd.print(setTemperature);
    lcd.print(F("C +/-"));
    lcd.print(driftAllowed);
    delay(1000);    
  }
  else {
    setTemperature = tempVar.setTemperature;
    driftAllowed = tempVar.driftAllowed;
  }

}




float read_temp_c(void) {
  // https://learn.adafruit.com/thermistor/using-a-thermistor
  float average = 0;
     
  // take N samples in a row, with a slight delay
  for (uint8_t i=0; i< NUMSAMPLES; i++) {
    average += analogRead(THERMISTORPIN);
    delay(10);
  }
  // average all the samples out
  average /= NUMSAMPLES;


      // convert the value to resistance
      average = 1023 / average - 1;
      average = SERIESRESISTOR / average;
     
      float steinhart;
      steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
      steinhart = log(steinhart);                  // ln(R/Ro)
      steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
      steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
      steinhart = 1.0 / steinhart;                 // Invert
      steinhart -= 273.15;                         // convert to C


  return(steinhart);

}




void buttonCheck() {
  buttons = analogRead(0);
  
  if ( buttons < 800) {

    if (buttons < 60) {
      driftAllowed++;
    }
    else if (buttons < 250) {
      setTemperature++;
    }
    else if (buttons < 500){
      setTemperature--;
    }
    else if (buttons < 825){
      if ( 0 < driftAllowed ) {
        driftAllowed--;
      }
    }
//  When using 3.3v AREF, the Select button is no longer detectable.
//  This forces us to do write to EEPROM after every button press.
    writeSettingsToEEPROM();
    while( analogRead(0) < 800 ) {
    }

  }
}

void writeSettingsToEEPROM() {
  //Data to store.
  SettingsObject tempVar = {
    setTemperature,
    driftAllowed,
    (( setTemperature + driftAllowed + 69) * 42)
  };


  EEPROM.put(0, tempVar);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Settings saved");
  delay(1000);
  lcd.clear();
}



void relayControl() {
  if ( (int)temp < (setTemperature - driftAllowed) ) {
    // Turn on the heat and external fan
    digitalWrite(RELAY1,LOW);
    digitalWrite(RELAY2,HIGH);
    digitalWrite(RELAY3,LOW);
    lcd.setCursor(15,1);
    lcd.print(">");
  }
  else if ( (int)temp > (setTemperature + driftAllowed) ) {
    // Turn on the chiller and external fan
    digitalWrite(RELAY1,HIGH);
    digitalWrite(RELAY2,LOW);
    digitalWrite(RELAY3,LOW);
    lcd.setCursor(15,1);
    lcd.print("<");
  }
  else {
    // Turn off the heat pumps and fans    
    digitalWrite(RELAY1,HIGH);
    digitalWrite(RELAY2,HIGH);
    digitalWrite(RELAY3,HIGH);
    lcd.setCursor(15,1);
    lcd.print("-");
  }
}
