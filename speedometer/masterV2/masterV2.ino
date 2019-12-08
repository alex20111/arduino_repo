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
//EEPROM IDX
uint8_t eepromIdx[] = {0, 4, 8, 9}; //saves in EEPROM. 0=totalOdo(4byte), 4=currOdo(4b),8=currWheelCirc(1b), 9=lightAuto(1b), 10 --> to be next

// speedometer display
long totalOdo = 0;   //Saved in EEPROM eepromIdx[0]
float currOdo = 0.0; //saved in EEPROM eepromIdx[1]
int prevOdo = 0;
char currOdoBuffer[10];
char odoDspFormatted [14]; //odo on oled.
//time
long startTime = 0;
char timeBuffer[10];
//speed
char speedBuffer[5];
//light
char lightBuffer[6];

//temperature//
#define THERMISTORPIN A1 // which analog pin to connect
#define THERMISTORNOMINAL 11000 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25 // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 5 // how many samples to take and average, more takes longer but is more 'smooth'
#define BCOEFFICIENT 3950 // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 9970 // the value of the 'other' resistor
int samples[NUMSAMPLES];
float temperature;

//delays
unsigned long mainScrPrevMillis = 0;//

//serial
const byte numChars = 20;
char receivedChars[numChars];
boolean newData = false;
boolean slaveReady = false;

//commands -  Ex: mega ask for Speed , send: <1>  received <145>.
const char CMD_SLAVE_READY   = '2'; //when slave (uno) is ready , automatically sent. Send; n/a reply <2ok>
const char CMD_LIGHT_STATUS = '4'; // light status automatically sent from uno. <4o10%> (ON) - <4n>(off) - <4b>(blink)
const char CMD_REQ_TO_DSP   = '5'; //request all the require info to normal display. (Temp, speed, light status) from uno
const char CMD_BTN1_MENU = '6'; //  Options: menu, menu select. command from slave
const char CMD_BTN2_SHORT  = '7'; // options: menu Up command from slave
const char CMD_BTN3_SHORT  = '8'; // options: menu down command from slave
const char CMD_BTN1_RESET_ODO   = '9'; //  Options: odo reset command from slave
const char CMD_MENU_END     = '1'; // options: exit menu
const char CMD_SAVE_ODO     = 'A'; // options: command from slave to save ODO
const char CMD_WHEEL_DATA   = 'B'; // options: Send to slave on startup for wheel circ data
const char CMD_LIGHT_DATA   = 'C'; // options: Send to slave on startup for light function

//menu options
boolean inMenuMode = false;
const uint8_t WHEEL_CIR_MIN = 191; //in cm
const uint8_t WHEEL_CIR_MAX = 233; //in cm
uint8_t menuOption = 0; //menu all options = 1, wheel circumference = 2, menu Lights = 3, menu exit = 4;
uint8_t currWheelCirc = 209; //in CM, Saved in EEPROM[2]
uint8_t lightAuto = 0; //0 = ON , 1 = off;
const char *main_menu_list =
  "Wheel Size\n" 
  "Light\n" 
  "Exit";

char menu_wheel_size[32];
const char *menu_wheel_size_txt = 
  "UP / DOWN to change\n"
  "BTN 1 for exit";


void setup(void) {
  Serial.begin(9600); // to debug
  Serial1.begin(115200); //communication between mcu  250000bps
  Serial.print(EEPROM.get(eepromIdx[2], currWheelCirc));
  //
  //just once, verity fi we have data for wheel circ and lignt auto
//  if (EEPROM.read(eepromIdx[2]) == 255) {
//    Serial.println(F("No wheel circ, adding 209"));
//    uint8_t circ = 209;
//    EEPROM.put(eepromIdx[2], circ);
//  }
//  if (EEPROM.read(eepromIdx[3]) == 255) {
//    Serial.println(F("No light status adding 0"));
//    uint8_t lgt = 0;
//    EEPROM.put(eepromIdx[3], lgt);
//  }
  ////////////////////

  pinMode(THERMISTORPIN, INPUT);

  u8g2.begin();
  u8g2.setContrast(255);

  welcomeScreen();

  EEPROM.get(eepromIdx[0], totalOdo);
  EEPROM.get(eepromIdx[1], currOdo);

  unsigned long waitMillis = millis();
  //wait from answer from uno and then send the saved data. 5 sec
  while (!newData && (millis() - waitMillis) < 15000 ) {
    recvWithStartEndMarkers() ;
  }
  handleSerialRead();

  if (!slaveReady) {
    problemConnectingScreen();
    while (true) {} //do not start
  }
  //send  saved info
  sendSlaveStartingData();

  Serial.print(F("Curr odo: ")); //debug
  Serial.print(currOdo);
  Serial.print(F(" Total odo: ")); //debug
               Serial.println(totalOdo);
    mainScrPrevMillis = millis();
  startTime         = millis();
}
//MAIN LOOP//
void loop(void) {

  readTemperatureTherm();

  recvWithStartEndMarkers();

  recvWithStartEndMarkersSerial(); //debugger

  handleSerialRead();

  displayMainScreen();
}

void displayMainScreen() {

  if (millis() - mainScrPrevMillis > 1000 && !inMenuMode) {
    mainScrPrevMillis = millis();

    requestToDisplay();
//    if(
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
    u8g2.print(odoDspFormatted);

    u8g2.setCursor(90, 110); //elapsed time
    u8g2.print(F("Time"));
    u8g2.setCursor(80, 120);
    u8g2.print(timeBuffer);

    u8g2.setCursor(105, 90);
    u8g2.print(F("KM/h"));
    //      u8g2.setFont(u8g2_font_logisoso46_tr );

    //  u8g2.setFont(u8g2_font_fub42_tr);
    u8g2.setFont(u8g2_font_7Segments_26x42_mn);
    Serial.print(F("len:"));
    Serial.print(strlen_P(speedBuffer)); //DEBUG
    if (strlen_P(speedBuffer) < 2) {
      u8g2.setCursor(50, 82);
    } else {
      u8g2.setCursor(30, 82);
    }
    u8g2.print(speedBuffer);
    u8g2.sendBuffer();
  }
}
void timerCalculation() {
  //calculate the time stince started.
  unsigned long over;
  int timeInMillis = millis() - startTime; // just reset the startTime to millis to start over  the timer
  int runHours = int(timeInMillis / 3600000);
  over = (timeInMillis % 3600000);
  int runMinutes = int(over / 60000);
  over = over % 60000;
  int runSeconds = int(over / 1000);

  sprintf(timeBuffer, "%02d:%02d:%02d", runHours, runMinutes, runSeconds); ///user a lot.. may if running out , replace
}

void requestToDisplay() {

  //get current information from uno to display on screen
  Serial1.print(F("<"));
  Serial1.print(CMD_REQ_TO_DSP);
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
  //Serial.print(F("From UNO: "));
  //Serial.println(receivedChars);
  uint8_t idx   = 0;
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
//
//  Serial.println(F("Results: "));
//  Serial.print(F("speedBuffer: "));
//  Serial.println(speedBuffer);
//  Serial.print(F("lightBuffer: "));
//  Serial.println(lightBuffer);
//  Serial.print(F("currOdoBuffer: "));
//  Serial.println(currOdoBuffer);

  prevOdo = (int)currOdo; //save previous current odo before incrementing
  //add current odo to total odo
  currOdo = atof(currOdoBuffer);

  if ( (int)currOdo > prevOdo) { //Ex if
    totalOdo++;
  }
  sprintf(odoDspFormatted, "%s/%l", currOdoBuffer, totalOdo);
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
}
void saveOdo() {
  Serial.println(F("Received command to save ODO"));
  EEPROM.put(eepromIdx[0], totalOdo);
  EEPROM.put(eepromIdx[1], currOdo);
}
void resetCurrentOdo() {
  Serial.println(F("Received command to RESET ODO"));
  currOdo = 0;
  EEPROM.put(eepromIdx[1], currOdo);
}
void welcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont29_tr);
  u8g2.setCursor(5, 21);
  u8g2.print(F("Welcome!"));
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(5, 110);
  u8g2.print(F("By: Alexandre Boudreault"));
  u8g2.sendBuffer();
}
void problemConnectingScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(10, 21);
  u8g2.print(F("Problem connecting"));
  u8g2.sendBuffer();
}
//sending to slave at startup
void sendSlaveStartingData() {
  EEPROM.get(eepromIdx[2], currWheelCirc);
  EEPROM.get(eepromIdx[3], lightAuto);

  Serial1.print('<');
  Serial1.print(CMD_WHEEL_DATA);
  Serial1.print(currWheelCirc);
  Serial1.print('>');

  Serial1.print('<');
  Serial1.print(CMD_LIGHT_DATA);
  Serial1.print(lightAuto);
  Serial1.print('>');
  Serial1.flush();

}

void printMenuList() {
  Serial.print(F("menu option: "));
  Serial.println(menuOption);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.userInterfaceSelectionListNB(
    "Menu",
    menuOption,
    main_menu_list);

    Serial.println(F("FFFFF"));
}

void menuWheelCirc(){
  sprintf(menu_wheel_size, "%s%d", menu_wheel_size_txt, currWheelCirc);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.userInterfaceSelectionListNB(
    "Size in CM",
    menuOption,
    menu_wheel_size);
  
}

void handleSerialRead() {

  if (newData == true) {
    Serial.print(F("new Data: "));
    Serial.println(receivedChars);

    newData = false;
    switch (receivedChars[0]) {
      case CMD_SLAVE_READY:
        slaveReady = true;
        break;
      case CMD_REQ_TO_DSP:  //5
        handleDisplayVar();
        break;
      case CMD_SAVE_ODO:
        saveOdo();
        break;
      case CMD_BTN1_RESET_ODO:
        resetCurrentOdo();
        break;
      case CMD_BTN2_SHORT: //up
        Serial.println(F("BTN2 short"));
        if (menuOption == 0) {
          menuOption = 2;
        } else {
          menuOption --;
        }
        printMenuList();
        break;
      case CMD_BTN1_MENU:
        Serial.println(F("CMD_BTN1_MENU"));
        if (menuOption == 2) {
          inMenuMode = false;
          menuOption = 0;
        }else if (menuOption == 1){
          menuWheelCirc();
        } else {
          inMenuMode = true;
          printMenuList();
        }

        break;
      case CMD_BTN3_SHORT: //down
        Serial.println(F("CMD_BTN3_SHORT"));
        if (menuOption == 2) {
          menuOption = 0;
        } else {
          menuOption ++;
        }
        printMenuList();
        break;
      case 'Z': //DEBUG
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
