#include <EEPROM.h>
#include <SystemStatus.h>
#include <DS3232RTC.h>
// arduino with internal christal 8mhz
#include <JC_Button.h>  // https://github.com/JChristensen/JC_Button
#include "LowPower.h"
#include <SI7021.h>  //SI7021


// Module connection pins (Digital Pins)
const uint8_t BTN1_PIN = 3;              // connect a button switch from this pin to ground
const uint8_t INTERRUPT_PIN = 2;
const uint8_t INTERRUPT_PIN2 = 3;
const uint8_t hc12SetPin = 8;
const uint8_t SI7021Power = 9;

const char START_MARKER = '<';
const char END_MARKER = '>';
const char SENSOR_TYPE = 't'; //the tyoe of sensor..
const char DATA_CMD = 'd'; //
const char INIT_CMD = 'i'; //
const char START_CMD = 's'; //
const char SEPERATOR = ','; //

char identifier[4] = ""; //increment everytime a new one is created..
const byte numChars = 36;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing
boolean newData = false;

time_t ALARM_INTERVAL(1 * 60);    // alarm interval in seconds
int intervals = 1; //minutes
int psStartHour = 0; //variables for power save
int psEndHour = 0;
int psInterval = 0;

boolean ackRecieved = false;
unsigned long startWait = 0;
unsigned long waitUntil = 120000;

Button btn1(BTN1_PIN);       // define the button

float temperatureF = 0.0;

// these values are taken from the HC-12 documentation v2 (+10ms for safety)
const unsigned long hc12setHighTime = 90;
const unsigned long hc12setLowTime = 50;
const unsigned long hc12cmdTime = 100;

SI7021 sensor; //SI7021

uint8_t numberOfRetries = 0; // number of retries on start up to connect to host

void interruptHandler() {
}
void bntInterrupt() {
  detachInterrupt(1);
}

void(* resetFunc) (void) = 0;

void setup() {
  //H12 transmitter
  Serial.begin(9600);

  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(INTERRUPT_PIN2, INPUT_PULLUP);
  pinMode(hc12SetPin, OUTPUT);
  
  pinMode(SI7021Power, OUTPUT);  //SI7021

  digitalWrite(hc12SetPin, HIGH);
  digitalWrite(SI7021Power, HIGH); //SI7021

  attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2

  btn1.begin();

  randomSeed(analogRead(0));

  delay(2000);

  sensor.begin();

  start();
}

void loop() {

  setSleepIntervals();

  reInitAlarm(); //re initialize alarm and go to sleep

  sendTemperature();

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

  digitalWrite(SI7021Power, LOW); //SI7021

  //sleep
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  digitalWrite(SI7021Power, HIGH); //SI7021
  detachInterrupt(0);

  HC12Wake();
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

  // set the current time
  RTC.set(timeInSeconds);

  // clear the alarm flag
  RTC.alarm(ALARM_1);
  // configure the INT/SQW pin for "interrupt" operation (disable square wave output)
  RTC.squareWave(SQWAVE_NONE);
  // enable interrupt output for Alarm 1
  RTC.alarmInterrupt(ALARM_1, true);
}

void sendTemperature() {
  ackRecieved = false;
  si7021_env data = sensor.getHumidityAndTemperature();

  Serial.print(START_MARKER);
  Serial.print(DATA_CMD);  //telling the recieved that we are sending data.
  Serial.print(SENSOR_TYPE);
  Serial.print(identifier);
  Serial.print(SEPERATOR);
  Serial.print(data.celsiusHundredths);
  Serial.print(SEPERATOR);
  Serial.print(data.humidityBasisPoints);
  Serial.print(SEPERATOR);
  Serial.print(SystemStatus().getVCC());
  Serial.print(END_MARKER);
  Serial.flush();

  // wait for Ack or more than 30 seconds, then continue
  unsigned long wait = millis();
  while (!ackRecieved && millis() - wait < 30000 ) {
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

  if (numberOfRetries == 0 || rstId) { //itf we are not retrying , then get the initial data.. if not we already have it..
    numberOfRetries = 0;
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
      identifier[3] = '\0';
      idExist = false;
    } else {
      EEPROM.get(address, identifier);
    }
    address = sizeof(identifier);

    int intv = EEPROM.read(address);
    if (intv == 255) {
      //using 5 min as default
      intervals = 5; //minutes
    } else {
      EEPROM.get(address, intervals);
    }

  } else if (numberOfRetries > 0 && identifier[0] == 'A' && identifier[1] == 'A') {
    idExist = false;
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

  startWait = millis();
//  waitUntil = 10000; //alex

  while ( millis() - startWait < waitUntil && !ackRecieved) { //2 min wait time
    recvWithStartEndMarkers();
    handleData();
  }

//  Serial.println(F("aft"));
  
  if (!ackRecieved) {
    if (numberOfRetries < 8) {
      numberOfRetries ++ ;
      start();
    } else {

      digitalWrite(SI7021Power, LOW); //SI7021
      HC12Sleep();
      attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      resetFunc();
    }
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

  if (newData == true) {

    char cmd;
    int eeAddr = 0;
    char tempId[6];

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
        identifier[3] = '\0';
        EEPROM.put(eeAddr, identifier);
        waitUntil = 600000;
        startWait = millis();
        sendOk(START_CMD);
      }

    } else   if (cmd == INIT_CMD && receivedChars[1] == SENSOR_TYPE) { //init command recieved

      strcpy(tempChars, receivedChars);
      char * strtokIndx; // this is used by strtok() as an index

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
        delay(2000);// to leave time for the OK to be sent before sleeping..
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
void setSleepIntervals() {

  time_t myTime = RTC.get();

  if (psStartHour > 0 && psEndHour > 0 &&
      ( psStartHour < psEndHour && hour(myTime) >= psStartHour && hour(myTime) < psEndHour ) ||
      (psStartHour > psEndHour && ( hour(myTime) >= psStartHour || hour(myTime) < psEndHour) ) ) {

    ALARM_INTERVAL = psInterval * 60;
  } else {
    ALARM_INTERVAL = intervals * 60;
  }
}
