#include <EEPROM.h>
#include <SystemStatus.h>
#include <JC_Button.h>  // https://github.com/JChristensen/JC_Button
#include "LowPower.h"

// Module connection pins (Digital Pins)
const uint8_t BTN1_PIN = 3;              // connect a button switch from this pin to ground
const uint8_t hc12SetPin = 8;
const uint8_t POWER_LED_PIN = 7;
const uint8_t WATERING_LED_PIN = 6;
const uint8_t TROUBLE_LED_PIN = 4;
const uint8_t CONNECTED_LED_PIN = 5;
const uint8_t MOSFET_PIN = 9;

const char START_MARKER = '<';
const char END_MARKER   = '>';
const char SENSOR_TYPE  = 'g'; //the tyoe of sensor..
const char COMMAND_CMD  = 'c'; //
const char INIT_CMD     = 'i'; //
const char START_CMD    = 's'; //
const char SEPERATOR    = ','; //

char identifier[4] = ""; //increment everytime a new one is created..
const byte numChars = 36;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing
boolean newData = false;

boolean ackRecieved = false;
unsigned long startWait = 0;
unsigned long waitUntil = 120000;

Button btn1(BTN1_PIN);       // define the button

// these values are taken from the HC-12 documentation v2 (+10ms for safety)
const unsigned long hc12setHighTime = 90;
const unsigned long hc12setLowTime = 50;
const unsigned long hc12cmdTime = 100;

unsigned long connLedBlinkPrevMillis = 0;
int connLedState = LOW;


uint8_t numberOfRetries = 0; // number of retries on start up to connect to host

void(* resetFunc) (void) = 0;

void bntInterrupt() {
  detachInterrupt(1);
}

void setup() {
  //H12 transmitter
  Serial.begin(9600);
  
  attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
  pinMode(hc12SetPin, OUTPUT);  
  pinMode(POWER_LED_PIN, OUTPUT);  
  pinMode(WATERING_LED_PIN, OUTPUT);  
  pinMode(TROUBLE_LED_PIN, OUTPUT);  
  pinMode(CONNECTED_LED_PIN, OUTPUT); //led blinking , trying to connect.. solid LED, connected. 
  pinMode(MOSFET_PIN, OUTPUT);

  digitalWrite(hc12SetPin, HIGH);
  digitalWrite(POWER_LED_PIN, HIGH);
  digitalWrite(MOSFET_PIN, LOW);

  btn1.begin();

  randomSeed(analogRead(0));

  delay(2000);

  start();
}

void loop() {

    recvWithStartEndMarkers();
    handleData();


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

  waitUntil = 120000; // 2 min

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
  connLedBlinkPrevMillis = millis();

  while ( millis() - startWait < waitUntil && !ackRecieved) { //2 min wait time
    recvWithStartEndMarkers();
    handleData();
    connectionBlink();// blink led while connecting.
  }
  
  if (!ackRecieved) {
    if (numberOfRetries < 8) {
      numberOfRetries ++ ;
      start();
    } else {
      attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
      digitalWrite(TROUBLE_LED_PIN, HIGH);
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); //maybe loop and wait until button presses or reset.. hummm,..
      resetFunc();
    }
  }else if (ackRecieved){
    connLedState = HIGH;
    digitalWrite(CONNECTED_LED_PIN, connLedState);
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
        ackRecieved = true;
      }

    } else   if (cmd == INIT_CMD && receivedChars[1] == SENSOR_TYPE) { //init command recieved

      strcpy(tempChars, receivedChars);
      char * strtokIndx; // this is used by strtok() as an index

      strtokIndx = strtok(tempChars, ",");     // get the first part -  the identifier
      strcpy(tempId, strtokIndx); // copy it to  the identifier

      if (idMatch()) {

        digitalWrite(MOSFET_PIN, LOW);
        digitalWrite(WATERING_LED_PIN, LOW);
        ackRecieved = true;
        sendOk(INIT_CMD);
      }
    } else if (cmd == 'o' && receivedChars[1] == SENSOR_TYPE && idMatch()) { //ok command recieved
      ackRecieved = true;  //master acknowledge that it recieved the data.
    }else if (cmd == COMMAND_CMD && receivedChars[1] == SENSOR_TYPE && idMatch()) { 
      //possible commands --> cg0911 ( turn on water )
      //                      cg0910 ( turn off water )
      //                      cg091s ( status )
            switch (receivedChars[5]) {
              case '1':
              // Serial.println(F("WATERING"));
                digitalWrite(MOSFET_PIN, HIGH);
                digitalWrite(WATERING_LED_PIN, HIGH);
                sendOk(COMMAND_CMD);
                break;
              case '0':
              // Serial.println(F("STOP WATERING"));
                digitalWrite(MOSFET_PIN, LOW);
                digitalWrite(WATERING_LED_PIN, LOW);
                sendOk(COMMAND_CMD);
                break;
              case 's':
              int stat = digitalRead(MOSFET_PIN);
                  Serial.print(START_MARKER);
                  Serial.print(cmd);
                  Serial.print(SENSOR_TYPE);
                  Serial.print(identifier);
                  Serial.print(stat);
                  Serial.print(END_MARKER);
                  Serial.flush();
                break;
              default:
                digitalWrite(MOSFET_PIN, LOW);
                digitalWrite(WATERING_LED_PIN, LOW);
                break;
            }
      

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

  int stat = digitalRead(MOSFET_PIN);

  Serial.print(START_MARKER);
  Serial.print(cmd);
  Serial.print(SENSOR_TYPE);
  Serial.print(identifier);
  Serial.print(F("ok"));
  Serial.print(stat);
  Serial.print(END_MARKER);
  Serial.flush();
}

void connectionBlink(){
    //blink led for connection
    if (millis() - connLedBlinkPrevMillis > 1000 ){
     
      //get led status
      if (connLedState == LOW){
        connLedState = HIGH;
      }else if (connLedState == HIGH){
        connLedState = LOW;
      }
   
      digitalWrite(CONNECTED_LED_PIN, connLedState);
      connLedBlinkPrevMillis  = millis();
    }
}
