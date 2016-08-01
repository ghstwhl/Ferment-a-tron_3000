/*********************

Controller module for the Ferment-a-Tron 3000

Copyright 2016, Chris O'Halloran 

This software assumes a stack made of:
  - Arduino (ATMega328P or compatable)
  - Adafruit Datalogging Shield wth RTC AND stacking headers (https://www.adafruit.com/product/1141 && https://www.adafruit.com/products/85)
  - Adafruit RGB LCD Shield with buttons (https://www.adafruit.com/products/714 or https://www.adafruit.com/products/716)
  - 10k Thermistor
  - 4 Relay Module (http://amzn.to/1W7wsyP)

Temp sensor is wired to A0

Relays:
  IN1-IN4 are wired to D4-D7, respectively

Buttons:

  UP     - Thermostat temp up one degree
  DOWN   - Thermostat temp down one degree
  LEFT   - Drive +/- down one degree  
  RIGHT  - Drive +/- up one degree  
  SELECT - write settings to EEPROM
**********************/

//Uncomment the below line to enable serial debugging and memory testing.
#define DEBUG 1


// BEGIN - load EEPROM library so we have persistent storage
#include <EEPROM.h>
// END - load EEPROM library so we have persistent storage


// BEGIN - RGB Shield with buttons includes, objects, and stuff.
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7
// END - RGB Shield with buttons includes, objects, and stuff.



// BEGIN - load RTC library so we can write useful logs
#include "RTClib.h"
RTC_DS1307 RTC;
String returnDateTime(void);
// END - load RTC library so we can write useful logs




// BEGIN - defines for control of the relays
// These defines set the relay control pins.  They are here to make it easier if someone wires their stack differently.

#define RELAY1 4
#define RELAY2 5
#define RELAY3 6
#define RELAY4 7
// END - defines for control of the relays



// BEGIN - Let's get set up for SD data logging
#include <SPI.h>
#include <SD.h>
// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
const int chipSelect = 10;
// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;
int peltierStatus; 
// END - Let's get set up for SD data logging




//// These defines are used in the read_temp_f()  function.
// Defines related to the thermistor being used for temperature readings
// which analog pin to connect
#define THERMISTORPIN A0         
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000      
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25   
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10000    
// Let's pre-define our termperature reading subroutine
float read_temp_f(void);
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
uint8_t buttons;
void buttonCheck(void);
void relayControl(void);


void setup() {
#ifdef DEBUG
  Serial.begin(9600);  //Start the serial connection with the computer
                       //to view the result open the serial monitor 
  Serial.println(F("Setup..."));
#endif

  // BEGIN - initialize the LCD and wipe it clean
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.setBacklight(WHITE);
  // END - initialize the LCD and wipe it clean


// Let's make sure the RTC is there...
// If your RTC needs to be inialized, use the example code supplied in the library.  If you add the time initialization code to
// this sketch, you run the risk of re-setting the clock every time you reset the Arduino.
  if (! RTC.begin()) {
#ifdef DEBUG
    Serial.println(F("Couldn't find RTC"));
#endif
    lcd.print("ERROR: RTC");
    while (1);
  }


  if (! RTC.isrunning()) {
#ifdef DEBUG
    Serial.println(F("RTC is NOT running!"));
#endif
  }


  // BEGIN - setup A0 for temperature readings, and try to load any stored settings
  //         IF there are NO stored settings, we need A0 ready so that the readSettingsFromEEPROM()
  //         function can use the current temp as the default.
  pinMode(A0, INPUT);
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


  // BEGIN - Time to set up the SD card and make sure it is there.

#ifdef DEBUG
  Serial.print(F("\nInitializing SD card..."));
#endif

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
#ifdef DEBUG
    Serial.println(F("initialization failed. Things to check:"));
    Serial.println(F("* is a card inserted?"));
    Serial.println(F("* is your wiring correct?"));
    Serial.println(F("* did you change the chipSelect pin to match your shield or module?"));
    lcd.print("ERROR: SD");
    while (1);
#endif
  } else if (!SD.begin(chipSelect)) {
#ifdef DEBUG
    Serial.println("Card failed, or not present");
#endif
    lcd.print("ERROR: SD");
    while (1);
  }
else {
#ifdef DEBUG
    Serial.println(F("Wiring is correct and a card is present."));
    Serial.println(F("card initialized."));
#endif
  }

  // Check to see if the log file exists.  If not, we need to initialize the file and write the CSV header line.
  if (!SD.exists("brewtron.csv") ) {
    File dataFile = SD.open("brewtron.csv", FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(F("datetime,setting,drift allowed,temperature,status"));
      dataFile.close();
      // print to the serial port too:
#ifdef DEBUG
      Serial.println(F("Writing CSV header: datetime,setting,drift allowed,temperature,status"));
#endif
    }
    // if the file isn't open, pop up an error:
    else {
#ifdef DEBUG
      Serial.println(F("error opening datalog.txt"));
#endif
    }

  }
  else {
#ifdef DEBUG
    Serial.println(F("EXISTS:  brewtron.csv"));
#endif
  }

}

void loop() {
  String dateTime = returnDateTime();
  temp = read_temp_f();
  relayControl();
  statusDisplay();


  // Let's write the logging data
  File dataFile = SD.open("brewtron.csv", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.print(dateTime);
    dataFile.print(F(","));
    dataFile.print(setTemperature);
    dataFile.print(F(","));
    dataFile.print(driftAllowed);
    dataFile.print(F(","));
    dataFile.print(temp);
    dataFile.print(F(","));
    dataFile.println(peltierStatus);
    dataFile.close();
    // print to the serial port too:
#ifdef DEBUG
    Serial.print(dateTime);
    Serial.print(F(","));
    Serial.print(setTemperature);
    Serial.print(F(","));
    Serial.print(driftAllowed);
    Serial.print(F(","));
    Serial.print(temp);
    Serial.print(F(","));
    Serial.println(peltierStatus);
#endif
  }
  // if the file isn't open, pop up an error:
  else {
#ifdef DEBUG
    Serial.println("error opening datalog.txt");
#endif
  }


  
  // We 'wait' for a second, but the whole time we are looking for button presses.
  ticktock = millis();
  while ( (ticktock <= millis()) && ((ticktock + 10000 ) >= millis()) ) {
    buttonCheck();
  }
}





void statusDisplay() {
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Temp:");
  lcd.print(temp);
  lcd.print("F");

  lcd.setCursor(0, 1);
  lcd.print("Set: ");
  lcd.print(setTemperature);
  lcd.print(F("F +/-"));
  lcd.print(driftAllowed);
  
}




void readSettingsFromEEPROM() {

  SettingsObject tempVar; //Variable to store custom object read from EEPROM.
  EEPROM.get(0, tempVar);

  if ( ((tempVar.setTemperature + tempVar.driftAllowed + 69) * 42) != tempVar.checksum) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Setting defaults");
    lcd.setBacklight(WHITE);
#ifdef DEBUG
    Serial.println(F("Custom EEPROM object has not been initialized."));
#endif
    setTemperature = (int)read_temp_f();
    driftAllowed = 2;

    lcd.setCursor(0, 1);
    lcd.print(setTemperature);
    lcd.print(F("F +/-"));
    lcd.print(driftAllowed);
    delay(1000);    
  }
  else {
    setTemperature = tempVar.setTemperature;
    driftAllowed = tempVar.driftAllowed;
#ifdef DEBUG
    Serial.println(F("Read custom object from EEPROM: "));
    Serial.println(tempVar.setTemperature);
    Serial.println(tempVar.driftAllowed);
    Serial.println(tempVar.checksum);
#endif
  }

}




float read_temp_f(void) {
    int samples[NUMSAMPLES];
      uint8_t i;
      float average;
     
      // take N samples in a row, with a slight delay
      for (i=0; i< NUMSAMPLES; i++) {
       samples[i] = analogRead(THERMISTORPIN);
       delay(10);
      }
     
      // average all the samples out
      average = 0;
      for (i=0; i< NUMSAMPLES; i++) {
         average += samples[i];
      }
      average /= NUMSAMPLES;

#ifdef DEBUG
      Serial.print(F("Average analog reading ")); 
      Serial.println(average);
#endif

      // convert the value to resistance
      average = 1023 / average - 1;
      average = SERIESRESISTOR / average;
#ifdef DEBUG
      Serial.print(F("Thermistor resistance ")); 
      Serial.println(average);
#endif
     
      float steinhart;
      steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
      steinhart = log(steinhart);                  // ln(R/Ro)
      steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
      steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
      steinhart = 1.0 / steinhart;                 // Invert
      steinhart -= 273.15;                         // convert to C
     
      float fahrenheit;
      fahrenheit = steinhart * 9 / 5 + 32;

#ifdef DEBUG
      Serial.print(F("Temperature ")); 
      Serial.print(fahrenheit);
      Serial.println(F(" *F"));
#endif

  return(fahrenheit);

}




void buttonCheck() {
  buttons = lcd.readButtons();

  if (buttons) {
    if (buttons & BUTTON_UP) {
      while (buttons & BUTTON_UP) { buttons = lcd.readButtons(); }
      setTemperature++;
    }
    if (buttons & BUTTON_DOWN) {
      while (buttons & BUTTON_DOWN) { buttons = lcd.readButtons(); }
      setTemperature--;
    }
    if (buttons & BUTTON_LEFT) {
      while (buttons & BUTTON_LEFT) { buttons = lcd.readButtons();}
      if (driftAllowed > 0 ) {
        driftAllowed--;
      }
    }
    if (buttons & BUTTON_RIGHT) {
      while (buttons & BUTTON_RIGHT) { buttons = lcd.readButtons();}
      driftAllowed++;
    }
    if (buttons & BUTTON_SELECT) {
      while (buttons & BUTTON_SELECT) { buttons = lcd.readButtons();}
      writeSettingsToEEPROM();
    }
    statusDisplay();
    relayControl();
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
  lcd.setBacklight(WHITE);
  delay(1000);
  lcd.clear();

#ifdef DEBUG
  Serial.println(F("Written custom setings to EEPROM"));
#endif

}



void relayControl() {
  if ( (int)temp < (setTemperature - driftAllowed) ) {
    // Turn on the heat and external fan
    digitalWrite(RELAY1,LOW);
    digitalWrite(RELAY2,HIGH);
    digitalWrite(RELAY3,LOW);
    lcd.setBacklight(RED);
    peltierStatus = 1;
  }
  else if ( (int)temp > (setTemperature + driftAllowed) ) {
    // Turn on the chiller and external fan
    digitalWrite(RELAY1,HIGH);
    digitalWrite(RELAY2,LOW);
    digitalWrite(RELAY3,LOW);
    lcd.setBacklight(BLUE);
    peltierStatus = 2;
  }
  else {
    // Turn off the heat pumps and fans    
    digitalWrite(RELAY1,HIGH);
    digitalWrite(RELAY2,HIGH);
    digitalWrite(RELAY3,HIGH);
    lcd.setBacklight(WHITE);
    peltierStatus = 0;
  }
}


String returnDateTime() {
  DateTime now = RTC.now();
  String DateString = "";
  char buf[8];

  sprintf(buf, "%04d", now.year());
  DateString += buf;
  DateString += '/';
  sprintf(buf, "%02d", now.month());
  DateString += buf;
  DateString += '/';
  sprintf(buf, "%02d", now.day());
  DateString += buf;

  DateString += ' ';

  sprintf(buf, "%02d", now.hour());
  DateString += buf;
  DateString += ':';
  sprintf(buf, "%02d", now.minute());
  DateString += buf;
  DateString += ':';
  sprintf(buf, "%02d", now.second());
  DateString += buf;

#ifdef DEBUG
  Serial.println(DateString);
#endif

  return(DateString);

}
