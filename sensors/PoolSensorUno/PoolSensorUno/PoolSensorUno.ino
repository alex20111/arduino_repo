#include <EEPROM.h>
#include <SystemStatus.h>
#include <DS3232RTC.h>

// arduino with internal christal 8mhz
#include <JC_Button.h>  // https://github.com/JChristensen/JC_Button
#include <TM1637Display.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"

// Module connection pins (Digital Pins)
#define CLK 4
#define DIO 5
#define ONE_WIRE_BUS 7
const uint8_t BTN1_PIN = 3;              // connect a button switch from this pin to ground
const uint8_t INTERRUPT_PIN = 2;
const uint8_t INTERRUPT_PIN2 = 3;
const uint8_t hc12SetPin = 8;
const uint8_t displayPin = 9;

const uint8_t SEG_ERR[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_E | SEG_G,                         // r
  SEG_E | SEG_G,                         // r
  0,                                     // space
};
const char START_MARKER = '<';
const char END_MARKER = '>';
const char SENSOR_TYPE = 'p'; //the tyoe of sensor..
const char DATA_CMD = 'd'; //
const char INIT_CMD = 'i'; //
const char START_CMD = 's'; //
const char SEPERATOR = ','; //

char identifier[4] = ""; //increment everytime a new one is created..

const byte numChars = 36;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing
boolean newData = false;

int intervals = 60; //seconds
int psStartHour = 0; //variables for power save
int psEndHour = 0;
int psInterval = 0;

boolean ackRecieved = false;
unsigned long startWait = 0;
unsigned long waitUntil = 120000;

TM1637Display display(CLK, DIO);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Button btn1(BTN1_PIN);       // define the button

boolean displayTemp = false;
unsigned long prevTempDisplay = 0;
float temperatureF = 0.0;

// these values are taken from the HC-12 documentation v2 (+10ms for safety)
const unsigned long hc12setHighTime = 90;
const unsigned long hc12setLowTime = 50;
const unsigned long hc12cmdTime = 100;

time_t ALARM_INTERVAL(1 * 60);    // alarm interval in seconds

void interruptHandler() {
}
void bntInterrupt() {
  detachInterrupt(1);
  displayTemp = true;
}

void(* resetFunc) (void) = 0;

void setup() {

  //H12 transmitter
  Serial.begin(9600);

  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(INTERRUPT_PIN2, INPUT_PULLUP);
  pinMode(hc12SetPin, OUTPUT);
  pinMode(displayPin, OUTPUT);

  digitalWrite(displayPin, HIGH);

  digitalWrite(hc12SetPin, HIGH);

  attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
  //TM1637
  display.setBrightness(0x02);

  //dallas thermometer
  sensors.begin();
  if (sensors.getDS18Count() == 0)
    display.setSegments(SEG_ERR);

  btn1.begin();

  randomSeed(analogRead(0));

  //display 0;
  display.setBrightness(7, true);
  display.showNumberDec(0);
  delay(2000);
  display.setBrightness(7, false);
  display.showNumberDec(0);
  
  start();
}

void loop() {



  if (!displayTemp) {

    time_t myTime = RTC.get();

    if (psStartHour > 0 && psEndHour > 0 && ( hour(myTime) >= psStartHour || hour(myTime) < psEndHour ) ) {
      //    Serial.println("Power saving time");
      ALARM_INTERVAL = psInterval * 60;
    } else {
      //     Serial.println("Normal Time");
      ALARM_INTERVAL = intervals * 60;
    }

    reInitAlarm(); //re initialize alarm and go to sleep
    if (sensors.getDS18Count() != 0 ) {
      if (displayTemp) {
        handleDisplayTemp();
        prevTempDisplay = millis();
      }
      sendTemperature();
    }
  }

  //reset temp display and turn off
  if (millis() - prevTempDisplay > 20000 && displayTemp) { //20 seconds
    display.setBrightness(7, false);
    display.showNumberDec(0);
    displayTemp = false;
  }

}
void reInitAlarm() {
  HC12Sleep();

  time_t t = RTC.get();
  time_t alarmTime = t + ALARM_INTERVAL;
  //Set New Alarm
  RTC.setAlarm(ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
  // clear the alarm flag
  RTC.alarm(ALARM_1);

  attachInterrupt(0, interruptHandler, LOW);//attaching a interrupt to pin d2
  attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2

  digitalWrite(displayPin, LOW);

  //sleep
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  detachInterrupt(0);

  HC12Wake();

  //  printAlarm(t);

}
void initialiseClock(long timeInSeconds) {
  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, false);
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);

  //  time_t alarmTime = timeInSeconds + ALARM_INTERVAL;    // calculate the alarm time

  // set the current time
  RTC.set(timeInSeconds);
  //  RTC.setAlarm(ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);

  // clear the alarm flag
  RTC.alarm(ALARM_1);
  // configure the INT/SQW pin for "interrupt" operation (disable square wave output)
  RTC.squareWave(SQWAVE_NONE);
  // enable interrupt output for Alarm 1
  RTC.alarmInterrupt(ALARM_1, true);

}

void handleDisplayTemp() {
  digitalWrite(displayPin, HIGH);
  //  delay(10);

  display.setBrightness(7, true);
  display.showNumberDec((int)temperatureF);
}

void sendTemperature() {
  //  Serial.println("send temp"); //alex
  ackRecieved = false;
  sensors.requestTemperatures();
  temperatureF =     sensors.getTempFByIndex(0);
  Serial.print(START_MARKER);
  Serial.print(DATA_CMD);  //telling the recieved that we are sending data.
  Serial.print(SENSOR_TYPE);
  Serial.print(identifier);
  Serial.print(SEPERATOR);
  Serial.print(temperatureF);
  Serial.print(SEPERATOR);
  Serial.print(SystemStatus().getVCC());
  Serial.print(END_MARKER);
  Serial.flush();

  // wait for Ack or more than 30 seconds, then continue
  unsigned long wait = millis();
  while (!ackRecieved && (millis() - wait > 30000) ) {
    recvWithStartEndMarkers() ;
    handleData();
  }
}
// put HC-12 module into sleep mode
void HC12Sleep() {
  sendH12Cmd("AT+SLEEP");
}

void HC12Wake() {
  digitalWrite(hc12SetPin, LOW);
  delay(hc12setLowTime);
  digitalWrite(hc12SetPin, HIGH);
  // wait some extra time
  delay(250);
}
void sendH12Cmd(const char cmd[]) {
  digitalWrite(hc12SetPin, LOW);
  delay(hc12setLowTime);

  Serial.print(cmd);
  Serial.flush();
  delay(hc12cmdTime);

  digitalWrite(hc12SetPin, HIGH);
  delay(hc12setHighTime);
}
void start() {

  boolean rstId = false;
  boolean idExist = true;
  btn1.read();//function to reset the ID of the sensor if conflict or need to reset the ID

  if (btn1.isPressed()) {
    rstId = true;
  }

  //check if ID exist for sensor
  uint8_t address = 0;
  int result = EEPROM.read(address);
  if (result == 255 || rstId) {
    int randNumber = random(0, 10);
    char tmp[2];
    itoa(randNumber, tmp, 10);
    identifier[0] = 'A';
    identifier[1] = 'A';
    identifier[2] = tmp[0];
    idExist = false;
  } else {
    EEPROM.get(address, identifier);
  }
  address = sizeof(identifier);

  int intv = EEPROM.read(address);
  if (intv == 255) {
    //using 5 min as default
    intervals = 301;
  } else {
    EEPROM.get(address, intervals);
  }

  //send identifier and wait for reply (2 min)
  Serial.print(START_MARKER);
  if (!idExist) {
    Serial.print(START_CMD);
  } else {
    Serial.print(INIT_CMD);
  }
  Serial.print(SENSOR_TYPE);
  Serial.print(identifier);
  Serial.print(END_MARKER);
  Serial.flush();

  while ( millis() - startWait < waitUntil && !ackRecieved) { //2 min wait time
    recvWithStartEndMarkers();
    handleData();
  }

  if (!ackRecieved) {
    attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
    digitalWrite(displayPin, LOW);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    resetFunc();
  }

}

void recvWithStartEndMarkers() {
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
      }
      else {
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

void handleData() {
  char cmd;
  int eeAddr = 0;
  char tempId[6];

  if (newData == true) {
    //    Serial.print("This just in ... ");
    //    Serial.println(receivedChars);
    newData = false;
    cmd = receivedChars[0];

    if (cmd == START_CMD && receivedChars[1] == SENSOR_TYPE) {

      strcpy(tempChars, receivedChars);
      char * strtokIndx; // this is used by strtok() as an index

      strtokIndx = strtok(tempChars, ",");     // get the first part - the string
      strcpy(tempId, strtokIndx); // copy it to  the identifier

      if (identifier[0] == tempId[2] && identifier[1] == tempId[3] && identifier[2] == tempId[4]) {
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        strcpy(tempId, strtokIndx);

        identifier[0] = tempId[0];
        identifier[1] = tempId[1];
        identifier[2] = tempId[2];
        EEPROM.put(eeAddr, identifier);
        waitUntil = 600000;
        startWait = millis();
        sendOk(START_CMD);
      }

    } else   if (cmd == INIT_CMD && receivedChars[1] == SENSOR_TYPE) { //init command recieved

      strcpy(tempChars, receivedChars);
      char * strtokIndx; // this is used by strtok() as an index

      char tempId[6];
      strtokIndx = strtok(tempChars, ",");     // get the first part -  the identifier
      strcpy(tempId, strtokIndx); // copy it to  the identifier

      if (idMatch()) {
        eeAddr = sizeof(identifier);

        strtokIndx = strtok(NULL, ","); // date in milliseconds
        long dateInMillis = atol(strtokIndx);     // convert this part to an integer

        int tmpIntv = 0;
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        tmpIntv = atoi(strtokIndx);     // convert this part to an integer

        if (tmpIntv != intervals) {
          intervals = tmpIntv;
          EEPROM.put(eeAddr, intervals);
        }

        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        psStartHour = atoi(strtokIndx);     // convert this part to an integer

        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        psEndHour = atoi(strtokIndx);     // convert this part to an integer


        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        psInterval = atoi(strtokIndx);     // convert this part to an integer

        initialiseClock(dateInMillis);

        ackRecieved = true;
        sendOk(INIT_CMD);
      }
    } else if (cmd == 'o' && receivedChars[1] == SENSOR_TYPE && idMatch()) { //ok command recieved
      ackRecieved = true;
    }
  }
}

boolean idMatch() {
  if (identifier[0] == receivedChars[2] &&
      identifier[1] == receivedChars[3] &&
      identifier[2] == receivedChars[4] ) {

    return true;
  }

  return false;
}

void sendOk(char cmd) {
  Serial.print(START_MARKER);
  Serial.print(cmd);
  Serial.print(SENSOR_TYPE);
  Serial.print(identifier);
  Serial.print(F("ok"));
  Serial.print(END_MARKER);
  Serial.flush();
}



//void printAlarm(time_t t) {
//  Serial.print(F("year: "));
//  Serial.print(year(t));
//  Serial.print(F(" month: "));
//  Serial.print(month(t));
//  Serial.print(F(" day: "));
//  Serial.print(day(t));
//  Serial.print(F(" hour: "));
//  Serial.print(hour(t));
//  Serial.print(F(" minutes: "));
//  Serial.print(minute(t));
//  Serial.print(F(" seconds: "));
//  Serial.print(second(t));
//  Serial.print(F(" time T: "));
//  Serial.println(t);
//}
