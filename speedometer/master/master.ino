//mega speedometer
#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>

#include "bitmap.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SSD1327_MIDAS_128X128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// speedometer display
int speedDisplay;
long totalOdo = 0;
char currOdoBuffer[10];
char odo [14]; //odo on oled.

//time
char timeBuffer[10];

//speed
char speedBuffer[5];

//temperature
//temperature//
#define THERMISTORPIN A1 // which analog pin to connect
#define THERMISTORNOMINAL 11000 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25 // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 5 // how many samples to take and average, more takes longer but is more 'smooth'
#define BCOEFFICIENT 3950 // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 9970 // the value of the 'other' resistor
int samples[NUMSAMPLES];
float temperature;

//light
char lightBuffer[6];

//delays
unsigned long mainScrRefInterval = 1000;// = 500 millisec
unsigned long mainScrPrevMillis = 0;//

//serial
const byte numChars = 8;
char receivedChars[numChars];
boolean newData = false;

//commands -  Ex: mega ask for Speed , send: <1>  received <145>.
const char cmdSlaveReady = '2'; //when slave (uno) is ready , automatically sent. Send; n/a reply <2ok>
const char cmdLightStatus = '4'; // light status automatically sent from uno. <4o10%> (ON) - <4n>(off) - <4b>(blink)
const char cmdReqToDsp = '5'; //request all the require info to normal display. (Temp, speed, light status) from uno
const char cmdBtn1 = '6'; //  Options: odo reset, menu, menu select.
const char cmdBtn2 = '7'; // options: menu Up
const char cmdBtn3 = '8'; // options: menu down

void setup(void) {
  Serial.begin(9600); // to debug
  Serial1.begin(115200); //communication between mcu  250000bps

  pinMode(THERMISTORPIN, INPUT);

  u8g2.begin();
  u8g2.setContrast(255);

  Serial.print(F("ready!"));

  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont29_tr);
  u8g2.setCursor(5, 21);
  u8g2.print(F("Welcome!"));
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(5, 110);
  u8g2.print(F("By: Alexandre Boudreault"));

  u8g2.sendBuffer();
  mainScrPrevMillis = millis();

  EEPROM.get(0, totalOdo);

  Serial.print(F("Total odo info: "));
  Serial.println(totalOdo);
}
void loop(void) {

  readTemperatureTherm();

  recvWithStartEndMarkers();

  recvWithStartEndMarkersSerial(); //debugger

  handleSerialRead();

  displayMainScreen();

}
void displayMainScreen() {

  if (millis() - mainScrPrevMillis > mainScrRefInterval) {

    mainScrPrevMillis = millis();

    	requestToDisplay();		//debugger  temp removed
    timerCalculation();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    //u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

    u8g2.drawXBMP( 110, 0, 16, 16, battery_bitmap);
    u8g2.setCursor(111, 21);
    u8g2.print(F("12%"));

    u8g2.drawXBMP( 0, 0, 16, 16, light_bitmap);
    u8g2.setCursor(50, 10); //temperature
    u8g2.print(temperature); //temperature

    //      u8g2.setFont/(u8g2_font_6x10_tr);

    u8g2.setCursor(25, 110);
    u8g2.print(F("Odo"));
    u8g2.setCursor(0, 120);
    u8g2.print(odo);

    u8g2.setCursor(90, 110); //elapsed time
    u8g2.print(F("Time"));
    u8g2.setCursor(80, 120);
    u8g2.print(timeBuffer);

    u8g2.setCursor(105, 90);
    u8g2.print(F("KM/h"));

    //      u8g2.setFont(u8g2_font_logisoso46_tr );
    //  u8g2.setFont(u8g2_font_fub42_tr);
    u8g2.setFont(u8g2_font_7Segments_26x42_mn);

    if (strlen(speedBuffer) < 2) {
      u8g2.setCursor(50, 82);
    } else {
      u8g2.setCursor(30, 82);
    }
    u8g2.print(speedBuffer);
    u8g2.sendBuffer();
  }
}
void timerCalculation() {
  unsigned long over;
  int runHours = int(millis() / 3600000);
  over = (millis() % 3600000);
  int runMinutes = int(over / 60000);
  over = over % 60000;
  int runSeconds = int(over / 1000);

  sprintf(timeBuffer, "%02d:%02d:%02d", runHours, runMinutes, runSeconds); ///user a lot.. may if running out , replace
}

void requestToDisplay() {
  //get current information from uno to display on screen
  Serial1.print(F("<"));
  Serial1.print(cmdReqToDsp);
  Serial1.print(F(">"));
  Serial1.flush();

  unsigned long previousMill = millis();

  while (Serial1.available() == 0) { //wait until we get data.or 200milliseconds

    if (millis() - previousMill > 200) {
      break;
    }
  }
  //got data, get it.
  recvWithStartEndMarkers();

  handleSerialRead();
}

void handleDisplayVar() {
  // <  0         ,   1 ,2, 3  ,4,     5     ,6,      7    >
  // <cmdSendToDsp,speed,-,lightStatus,-,current odo>
  Serial.print(F("From UNO: "));
  Serial.println(receivedChars);
  uint8_t idx 	= 0;
  uint8_t charIdx = 0;
  for (uint8_t i = 1 ; i < strlen(receivedChars); i ++) {
    if (receivedChars[i] == '-') {
      idx++;
      charIdx = 0;
    } else {
      switch (idx) {
        case 0:
          speedBuffer[charIdx] = receivedChars[i];
          charIdx++;
          if (receivedChars[i + 1] == '-') {
            speedBuffer[charIdx] = '\0';
          }
          break;
        //		case 0:
        //			tmpBuffer[charidx] = receivedChars[i];
        //			charidx++;
        //			if(receivedChars[i+1] == '-') {
        //				tmpBuffer[charidx] = '\0';
        //			}
        //			break;
        case 1:
          lightBuffer[charIdx] = receivedChars[i];
          charIdx++;
          if (receivedChars[i + 1] == '-') {
            lightBuffer[charIdx] = '\0';
          }
          break;
        case 2:
          currOdoBuffer[charIdx] = receivedChars[i];
          charIdx++;
          break;
      }
    }
  }
  currOdoBuffer[charIdx] = '\0';

  Serial.println(F("Results: "));
  Serial.print(F("speedBuffer: "));
  Serial.println(speedBuffer);
  Serial.print(F("lightBuffer: "));
  Serial.println(lightBuffer);
  Serial.print(F("currOdoBuffer: "));
  Serial.println(currOdoBuffer);

  //add current odo to total odo
  float currOdo = atof(currOdoBuffer);
  totalOdo = totalOdo + currOdo;

  sprintf(odo, "%s/%d", currOdoBuffer, totalOdo);
}
void readTemperatureTherm() {
  uint8_t i;

  float average;

  // take N samples in a row, with a slight delay
  for (i = 0; i < NUMSAMPLES; i++) {
    samples[i] = analogRead(THERMISTORPIN);
    delay(5);
  }
  // average all the samples out
  average = 0;
  for (i = 0; i < NUMSAMPLES; i++) {
    average += samples[i];
  }
  average /= NUMSAMPLES;

  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

  temperature = average / THERMISTORNOMINAL;     // (R/Ro)
  temperature = log(temperature);                  // ln(R/Ro)
  temperature /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  temperature += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  temperature = 1.0 / temperature;                 // Invert
  temperature -= 273.15;                         // convert to C

  //  Serial.println(temperature);
}

void handleSerialRead() {
  if (newData == true) {
    Serial.print(F("new Data: "));
    Serial.println(receivedChars);

    newData = false;

    switch (receivedChars[0]) {
      case cmdSlaveReady:
        Serial.println(F("UNO ready"));
        break;
      case cmdReqToDsp:  //5
        handleDisplayVar();
        break;
      case '9':
        requestToDisplay();
        break;
    }
  }
}
//function to handle recieving data.
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;

  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial1.available() > 0 && newData == false) {

    rc = Serial1.read();
    Serial.print("->: ");
    Serial.println(rc);

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;

        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void recvWithStartEndMarkersSerial() {
  static boolean recvInProgress = false;

  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;

        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}
