#include <StopWatch.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include "bitmap.h"
#include "OdoEnums.h"
#include <Wire.h>
#include "Adafruit_MCP9808.h"
//ENUM LINKS: https://forum.arduino.cc/index.php?topic=88087.0

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

//PINS
//
#define LDR A0
#define U8g2_CS 10
#define U8g2_DC 9
#define U8g2_RESET 8

U8G2_SSD1327_MIDAS_128X128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ U8g2_CS, /* dc=*/ U8g2_DC, /* reset=*/ U8g2_RESET);

//EEPROM IDX
uint8_t eepromIdx[] = {0, 4, 8, 9, 11}; //saves in EEPROM. 0=totalOdo(4byte), 4=currOdo(4b),8=currWheelCirc(1b), 9=lightOption(1b), 11 timeStore , 15--> to be next

// speedometer display
long totalOdo = 0;   //Saved in EEPROM eepromIdx[0]
float currOdo = 0.0; //saved in EEPROM eepromIdx[1]
int prevOdo = 0;
char currOdoBuffer[10];
char odoDspFormatted [14]; //odo on oled.
//time
StopWatch chrono;
boolean timerStarted = false;
char timeBuffer[10];
unsigned long prevTime = 0l;
unsigned long storedTime = 0l;

//speed
char speedBuffer[5];
//light
char lightBuffer[6];
char lightDisplay[5];
boolean lightOn = false;
unsigned long ldrPrevReading = 0;

//temperature//
// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
float temperature;
unsigned long prevTempReading = 0;

//delays
unsigned long mainScrPrevMillis = 0;//

//serial
const byte numChars = 20;
char receivedChars[numChars];
boolean newData = false;
boolean slaveReady = false;

//commands -  Ex: mega ask for Speed , send: <1>  received <145>.
const char CMD_SLAVE_READY   = '2'; //when slave (uno) is ready , automatically sent. Send; n/a reply <2ok>
//const char CMD_LIGHT_STATUS = '4'; // light status automatically sent from uno. <4o10%> (ON) - <4n>(off) - <4b>(blink)
const char CMD_REQ_TO_DSP   = '5'; //request all the require info to normal display. (Temp, speed, light status) from uno
const char CMD_BTN1_MENU = '6'; //  Options: menu, menu select. command from slave
const char CMD_BTN2_SHORT  = '7'; // options: menu Up command from slave
const char CMD_BTN3_SHORT  = '8'; // options: menu down command from slave
const char CMD_BTN1_RESET_ODO   = '9'; //  Options: odo reset command from slave
const char CMD_MENU_END     = '1'; // options: exit menu
const char CMD_SAVE_ODO     = 'A'; // options: command from slave to save ODO
const char CMD_WHEEL_DATA   = 'B'; // options: Send to slave on startup for wheel circ data
const char CMD_LIGHT_DATA   = 'C'; // options: Send to slave on startup for light function
const char CMD_WHEEL_REVOLUTION   = 'W'; // options: Send the number of wheel revolution if we have some data in the curr odo. To restart calculation at the right spot
const char CMD_LDR           = 'R';
const char CMD_LIGHT_POWER        = 'U'; //light power by %

//menu options
boolean inMenuMode = false;
const uint8_t WHEEL_CIR_MIN = 191; //in cm
const uint8_t WHEEL_CIR_MAX = 233; //in cm
uint8_t menuOption = 99; //Select wheel circumference = 0,1, Select menu Lights = 2, menu exit = 3;
uint8_t viewingScreen = 0;// screen 0 = main menu, screen 1 = wheel circ, screen 3 = light option
uint8_t currWheelCirc = 209; //in CM, Saved in EEPROM[2]
uint8_t lightOption = 1; //1 = AUTO (Turn on/off automatically) , 2 = ON (Always ON) , 3 = off (Lights off);  //alex
const char *main_menu_list =
  "Wheel Size\n"
  "Light\n"
  "Exit";
const char *menu_wheel_size_txt =
  "UP / DOWN to change\n"
  "BTN 1 for select/exit"; //add PROGMEM

const char *menu_light_txt = //add PROGMEM
  "AUTO\n"
  "ON\n"
  "OFF\n";

char menu_temp_storage[46];  ///add PROGMEM

//BATTERY
uint8_t batteryPercent = 0;
char battBuffer[6];

void setup(void) {
  Serial.begin(9600); // to debug
  Serial1.begin(115200); //communication between mcu  250000bps
  Serial.print(EEPROM.get(eepromIdx[2], currWheelCirc));


  //  u8g2.setBusClock(2000000);  //div 8
  //  u8g2.setBusClock(1000000); //div 16
  //  u8g2.setBusClock(500000); //div 32
  //  u8g2.setBusClock(250000); //div 64
  //  u8g2.setBusClock(125000); //div 128

  ////////////////////to INIT EEPROM, to remove after 1st flash /////////////////
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
  //
  //  storedTime = 0;
  //  EEPROM.put( eepromIdx[4], storedTime);
  /////////////////////////////////////////////////

  pinMode(LDR, INPUT);

  u8g2.begin();
  u8g2.setContrast(255);

  welcomeScreen();

  EEPROM.get(eepromIdx[0], totalOdo);
  EEPROM.get(eepromIdx[1], currOdo);
  EEPROM.get(eepromIdx[4], storedTime);

  unsigned long waitMillis = millis();
  //wait from answer from uno and then send the saved data. 5 sec
  while (!newData && (millis() - waitMillis) < 15000 ) {
    recvWithStartEndMarkers() ;
  }
  handleSerialRead();

  if (!slaveReady) {
    problemConnectingScreen(0);
    while (true) {} //do not start
  }
  if (!tempsensor.begin(0x18)) {
    temperature = -99.99;
    problemConnectingScreen(2);

  }

  tempsensor.setResolution(1);
  //send  saved info
  sendSlaveStartingData();

  Serial.print(F("Curr odo: ")); //debug
  Serial.print(currOdo);
  Serial.print(F(" Total odo: ")); //debug
  Serial.println(totalOdo);
  Serial.print(F(" Stored time: ")); //debug
  Serial.println(storedTime);
  mainScrPrevMillis = millis();

  chrono.reset();
  if (storedTime > 0) {
    prevTime = storedTime;
    timerCalculation(true); //timer bypass to display time at startup
  } else {
    resetTimeBuffer();
  }

}
//MAIN LOOP//
void loop(void) {

  readTemperatureTherm();

  recvWithStartEndMarkers();

  recvWithStartEndMarkersSerial(); //debugger

  handleSerialRead();

  readLdr();

  displayMainScreen();
}

void readLdr() {
  if (millis() - ldrPrevReading > 1000 && !inMenuMode) {

    int ldr = analogRead(LDR);

    Serial1.print('<');
    Serial1.print(CMD_LDR);
    Serial1.print(ldr);
    Serial1.print('>');

    ldrPrevReading = millis();
  }

}
void displayMainScreen() {

  if (millis() - mainScrPrevMillis > 500 && !inMenuMode) {
    mainScrPrevMillis = millis();

    requestToDisplay();
    timerCalculation(false);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    //u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

    if (batteryPercent > 75) {
      u8g2.drawXBMP( 110, 0, 16, 16, battery_100);
    } else if (batteryPercent <= 75 && batteryPercent > 50) {
      u8g2.drawXBMP( 110, 0, 16, 16, battery_75);
    } else if (batteryPercent <= 50 && batteryPercent > 25) {
      u8g2.drawXBMP( 110, 0, 16, 16, battery_50);
    } else {
      u8g2.drawXBMP( 110, 0, 16, 16, battery_25);
    }
    //    if (strlen(battBuffer) == 4) {
    if (batteryPercent == 100) {
      u8g2.setCursor(107, 21);
    } else {
      u8g2.setCursor(111, 21);
    }
    u8g2.print(battBuffer);

    if (lightOn) {
      u8g2.drawXBMP( 0, 0, 16, 16, light_bitmap);
      u8g2.setCursor(0, 21);
      u8g2.print(lightDisplay);
    }

    u8g2.setCursor(50, 10); //temperature
    u8g2.print(temperature); //temperature

    u8g2.setCursor(25, 110);
    u8g2.print(F("Odo"));
    u8g2.setCursor(5, 120);
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
    if (strlen(speedBuffer) < 2) {
      u8g2.setCursor(50, 82);
    } else {
      u8g2.setCursor(30, 82);
    }
    u8g2.print(speedBuffer);
    u8g2.sendBuffer();
  }
}
void timerCalculation(boolean bypass) {

  //bypass is used at startup to set the time at startup if any in storage.
  if (!bypass) {
    int currSpeed = atoi(speedBuffer);

    if (currSpeed > 0 && !timerStarted) {

      timerStarted = true;
      chrono.start();
    } else if (currSpeed <= 1 && timerStarted) {

      timerStarted = false;
      chrono.stop();
    }
  }
  if (timerStarted || bypass) {
    unsigned long timeInMillis = chrono.value() + prevTime;
    //calculate the time stince started.
    unsigned long over;
    int runHours = int(timeInMillis / 3600000);
    over = (timeInMillis % 3600000);
    int runMinutes = int(over / 60000);
    over = over % 60000;
    int runSeconds = int(over / 1000);

    storedTime = ( (runHours * 3600) + (runMinutes * 60) + runSeconds) * 1000;
    sprintf(timeBuffer, "%02d:%02d:%02d", runHours, runMinutes, runSeconds); ///user a lot.. may if running out , replace
  }
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
  Serial.print(F("From UNO: "));
  Serial.println(receivedChars);
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
          if (receivedChars[i + 1] == '-') {
            currOdoBuffer[charIdx] = '\0';
          }
          break;
        case 3:
          battBuffer[charIdx] = receivedChars[i];
          charIdx++;
          break;
      }
    }
  }

  battBuffer[charIdx] = '\0';
  //get the int value of battery
  batteryPercent = atoi(battBuffer);

  //when we have the int value, format it for display.
  battBuffer[charIdx] = '%';
  battBuffer[charIdx + 1] = '\0';

  prevOdo = (int)currOdo; //save previous current odo before incrementing
  //add current odo to total odo
  currOdo = atof(currOdoBuffer);

  if ( (int)currOdo > prevOdo) { //Ex if
    totalOdo++;
  }

  sprintf(odoDspFormatted, "%s/%06ld", currOdoBuffer, totalOdo);

  //format light
  if (lightBuffer[0] == 'o') {
    lightOn = true;

  } else {
    lightOn = false;
  }

  if (lightOn && strlen(lightBuffer) == 4) {  //o100  = 4 , o30 = 3, o0
    //    Serial.println("YESSS");
    lightDisplay[0] = lightBuffer[1];
    lightDisplay[1] = lightBuffer[2];
    lightDisplay[2] = lightBuffer[3];
    lightDisplay[3] = '%';
    lightDisplay[4] = '\0';
  } else   if (lightOn && strlen(lightBuffer) == 3) {  //o100  = 4 , o30 = 3, o0
    lightDisplay[0] = lightBuffer[1];
    lightDisplay[1] = lightBuffer[2];
    lightDisplay[2] = '%';
    lightDisplay[3] = '\0';
  } else   if (lightOn && strlen(lightBuffer) == 2) {  //o100  = 4 , o30 = 3, o0
    lightDisplay[0] = lightBuffer[1];
    lightDisplay[1] = '%';
    lightDisplay[2] = '\0';
  }
}

void readTemperatureTherm() {

  if (millis() - prevTempReading > 5000 && !inMenuMode) {
    tempsensor.wake();

    temperature = tempsensor.readTempC();

    tempsensor.shutdown_wake(1);

    prevTempReading = millis();
  }
}
void saveOdo() {
  Serial.print(F("Received command to save ODO/time"));
  Serial.println(totalOdo);
  Serial.print(F("Curr odo"));
  Serial.println(currOdo);
  Serial.print(F("Time: "));
  Serial.println(storedTime);
  EEPROM.put(eepromIdx[0], totalOdo);
  EEPROM.put(eepromIdx[1], currOdo);
  EEPROM.put(eepromIdx[4], storedTime);
}
void resetCurrentOdo() {
  Serial.println(F("Received command to RESET ODO"));
  currOdo = 0;
  EEPROM.put(eepromIdx[1], currOdo);
  chrono.reset();
  resetTimeBuffer();
  storedTime  = 0;
  prevTime    = 0;
  EEPROM.put(eepromIdx[4], storedTime);

}
void resetTimeBuffer() {
  timeBuffer[0] = '0'; timeBuffer[1] = '0'; timeBuffer[2] = ':'; timeBuffer[3] = '0'; timeBuffer[4] = '0'; timeBuffer[5] = ':';
  timeBuffer[6] = '0'; timeBuffer[7] = '0'; timeBuffer[8] = '\0';
}
void welcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont29_tr);
  u8g2.setCursor(5, 21);
  u8g2.print(F("Welcome!"));
  u8g2.setFont(u8g2_font_5x8_tr);
u8g2.drawXBMP( 32, 32, 64, 64, bike_bitmap);
  
  u8g2.setCursor(5, 110);
  u8g2.print(F("By: Alexandre Boudreault"));
  u8g2.sendBuffer();
}
void problemConnectingScreen(int device) { //0-slave, 1- current, 2- temperature
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(10, 21);
  u8g2.print(F("Problem connecting"));
  u8g2.setCursor(10, 60);
  if (device == 0) {
    u8g2.print(F("Error with uno"));
  } else if (device == 1) {
    u8g2.print(F("Error, current sensor"));
  } else if (device == 2) {
    u8g2.print(F("Error temp sensor"));
  } else {
    u8g2.print(F("Error Other"));
  }
  u8g2.sendBuffer();
}
//sending to slave at startup
void sendSlaveStartingData() {
  EEPROM.get(eepromIdx[2], currWheelCirc);
  EEPROM.get(eepromIdx[3], lightOption);

  if (lightOption == 0) {
    lightOption = 1;
  }

  Serial1.print('<');
  Serial1.print(CMD_WHEEL_DATA);
  Serial1.print(currWheelCirc);
  Serial1.print('>');

  Serial1.print('<');
  Serial1.print(CMD_LIGHT_DATA);
  Serial1.print(lightOption);
  Serial1.print('>');

  //send wheel revolution information if currOdo is > than 0
  if (currOdo > 0) {
    int revolutionCount =  ( currOdo / ((float)currWheelCirc / 100.00) ) * 1000  ;
    Serial1.print('<');
    Serial1.print(CMD_WHEEL_REVOLUTION);
    Serial1.print(revolutionCount);
    Serial1.print('>');
  }
  Serial1.flush();
}

void handleBtn1Menus() {

  if (viewingScreen == 0) {
    if (menuOption == 3) {  //exit menu
      inMenuMode = false;
      menuOption = 99;
      viewingScreen = 0;

      Serial1.print('<');
      Serial1.print(CMD_MENU_END);
      Serial1.print(inMenuMode);
      Serial1.print('>');

    } else if (menuOption == 0 || menuOption == 1) {  //display Wheel circ menu
      viewingScreen = 1;
      menuWheelCirc();
    } else if (menuOption == 2) { //display lights menu
      viewingScreen = 2;
      menuLightStatus();
    } else {    //displaying menu (init)
      inMenuMode = true;
      menuOption = 0;
      printMenuList();
    }
  } else if (viewingScreen == 1) {
    viewingScreen = 0;
    menuOption = 1;
    //save wheel circ
    EEPROM.put(eepromIdx[2], currWheelCirc);
    Serial1.print('<');
    Serial1.print(CMD_WHEEL_DATA);
    Serial1.print(currWheelCirc);
    Serial1.print('>');
    printMenuList();
  } else if (viewingScreen == 2) {
    viewingScreen = 0;
    menuOption = 2;

    EEPROM.put(eepromIdx[3], lightOption);
    Serial1.print('<');
    Serial1.print(CMD_LIGHT_DATA);
    Serial1.print(lightOption);
    Serial1.print('>');
    printMenuList();

  }
}

void handleMenuBtn2() {

  if (viewingScreen == 0) {
    if (menuOption == 3) {  //if on exit, go to light option
      menuOption = 2;
    } else if (menuOption == 2) { //if on light , got to wheel option
      menuOption = 0;
    } else {
      menuOption = 3;
    }
    printMenuList();

  } else if (viewingScreen == 1) {
    if (currWheelCirc == 220) {
      currWheelCirc = 150;
    } else {
      currWheelCirc ++;
    }
    menuWheelCirc();
  } else if (viewingScreen == 2) {
    if (lightOption == 1) {
      lightOption = 3;
    } else if (lightOption == 3) {
      lightOption = 2;
    } else if (lightOption == 2) {
      lightOption = 1;
    }
    menuLightStatus();
  }
}

void handleBtn3() {
  if (viewingScreen == 0) {
    if (menuOption == 3) {
      menuOption = 0;
    } else if (menuOption == 0 || menuOption == 1) {
      menuOption = 2;
    } else {
      menuOption = 3;
    }
    printMenuList();
  } else if (viewingScreen == 1) {
    if (currWheelCirc == 150) {
      currWheelCirc = 220;
    } else {
      currWheelCirc --;
    }
    menuWheelCirc();
  } else if (viewingScreen == 2) {
    if (lightOption == 1) {
      lightOption = 2;
    } else if (lightOption == 2) {
      lightOption = 3;
    } else if (lightOption == 3) {
      lightOption = 1;
    }
    menuLightStatus();
  }
}

void printMenuList() {
  //  Serial.print(F("menu option: "));
  //  Serial.println(menuOption);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.userInterfaceSelectionListNB(
    "Menu",
    menuOption,
    main_menu_list);
}

void menuWheelCirc() {
  //  Serial.println(F("In menuWheelCirc"));
  sprintf(menu_temp_storage, "%s\n%d", menu_wheel_size_txt, currWheelCirc);  //alex
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.userInterfaceSelectionListNB(
    "Size in CM",
    3,
    menu_temp_storage);  //alex
}

void menuLightStatus() {   //alex

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.userInterfaceSelectionListNB(
    "Light Option",
    lightOption,
    menu_light_txt);
}   //alex

void handleSerialRead() {

  if (newData == true) {

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
        if (inMenuMode) {
          handleMenuBtn2();
        }
        break;
      case CMD_BTN1_MENU:
        Serial.println(F("CMD_BTN1_MENU"));
        handleBtn1Menus();
        break;
      case CMD_BTN3_SHORT: //down
        Serial.println(F("CMD_BTN3_SHORT"));
        if (inMenuMode) {
          handleBtn3();
        }
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

    //    Serial.print("->: ");
    //    Serial.println(rc);

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
