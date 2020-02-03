#include <JC_Button.h>
#include <Arduino.h>
#include "OdoEnums.h"

#define HALL_SWITCH  2
#define BTN_1        4
#define BTN_2        5
#define BTN_3        6
#define LIGHT_MOSFET 7
#define LDR A0     // V ------ LDR -- PIN(A0) --- /\/\/\ ---- GR  

// speedometer variables
boolean inMetric = true;
float start, elapsed;
uint8_t wheelCir = 209; //in cm.
unsigned long revolutionCount = 0;
int speedDisplay = 0;

//odometer//
float currentDistance = 0.0;

//serial//
const byte numChars = 8;
char receivedChars[numChars];
boolean newData = false;

//light//
uint8_t lightPower = 50;  //0 to 100
int ldrValue = 0;
boolean lightOn = false;
LightState lightState = LIGHT_AUTO;

//button debounce
//Button btnOne(BTN_1); // Instantiate a Bounce object
Button btnOne(BTN_1, 25, true, true);
Button btnTwo(BTN_2, 25, true, true); // Instantiate a Bounce object
Button btnThree(BTN_3, 25, true, true); // Instantiate a Bounce object

//commands -  Ex: mega ask for Speed , send: <1>  received <145>.
const char CMD_SLAVE_READY   = '2'; //when slave (uno) is ready , automatically sent. Send; n/a reply <2ok>  //DONE
const char CMD_LIGHT_STATUS = '4'; // light status automatically sent from uno. <4o10%> (ON) - <4n>(off) - <4b>(blink)   //TODO
const char CMD_SEND_TO_DSP  = '5'; //request all the require info to normal display. (Temp, speed, light status) from uno //DONE
const char CMD_BTN1_MENU = '6'; //  Options: menu, menu select. //TODO
const char CMD_BTN2_SHORT = '7'; // options: menu Up //TODO
const char CMD_BTN3_SHORT = '8'; // options: menu down //TODO
const char CMD_BTN1_RESET_ODO     = '9'; //  Options: odo reset command from slave //DONE
const char CMD_MENU_END     = '1'; // options: exit menu - Signal from MEGA telling Slave that it's out of the menu//TODO
const char CMD_SAVE_ODO     = 'A'; // options: command from slave to save ODO  //DONE
const char CMD_WHEEL_DATA   = 'B'; // options: Receive from master on startup for wheel circ data //DONe
const char CMD_LIGHT_DATA   = 'C'; // options: Send to slave on startup for light function//DONE

const char CMD_DUMP_DATA    = 'z'; // options:Debugging function, dump data UNO only.

boolean inMenu = false;  //specify if the buttons are in menu function
boolean saveSent = false;
boolean resetOdoSent = false;
boolean speedoStopped = false;
boolean started = false;

boolean triggerDSPRefresh = false; // triger a display refresh.

//timers
unsigned long countDown = 0;

void setup(void) {

  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(HALL_SWITCH), speedInt, FALLING );

  pinMode(HALL_SWITCH, INPUT);
  pinMode(LIGHT_MOSFET, OUTPUT);
  pinMode(LDR, INPUT);

  btnOne.begin();
  btnTwo.begin();
  btnThree.begin();

  delay(200);

  //send ready!
  Serial.print('<');
  Serial.print(CMD_SLAVE_READY);
  Serial.print('>');

  speedoStopped = true;
}

void loop(void) {


  processLdr(); //read LDR info

  turnOnOfflights();

  verifySpeed();

  processBtns();

  //  if (triggerDSPRefresh) {
  //    sendInfoToDsp();
  //    triggerDSPRefresh = false;
  //  }

  recvWithStartEndMarkers() ;

  handleSerialRead();

  //  Serial.println(ldrValue);
}

void speedInt() { //interrupt

  if (speedoStopped ) { //if we stopped we need to reset the start so the elapse won't be a big value.
    start = millis();
    saveSent = false;
    speedoStopped = false;
  }

  elapsed = millis() - start;
  start = millis();

  revolutionCount++;

  if (inMetric) {
    speedDisplay = (3600 * ((float)wheelCir / 100.00) ) / elapsed;
    currentDistance = revolutionCount * ((float)wheelCir / 100.00) / 1000.00;
    //speedDisplay = (3600 * (wheelCir * .62137) ) / elapsed //in MPH
  }

}
void verifySpeed() {

  long idleMillis = (millis() - start);
  if ( idleMillis  > 2000 ) { //idle more than 3 seconds. turn to 0

    if (speedDisplay != 0) {
      speedDisplay = 0;
      speedoStopped = true;
    }

    if (idleMillis > 5000 && !saveSent && speedDisplay == 0 && started) {
      //send message
      sendSaveOdoCommand();
      saveSent = true;
    }

  }
  if (speedDisplay > 1 &&  (millis() - countDown) > 300000) { //1 min
    //save
    sendSaveOdoCommand();
    countDown = millis();
  } else if (speedDisplay  < 2) {
    countDown = millis();
  }

  if (speedDisplay > 0) { //this just tell the system that it's the 1st start after boot up. will always be tru after speed is greater an 0.
    started = true;
  }
}
void sendSaveOdoCommand() {
  Serial.print('<');
  Serial.print(CMD_SAVE_ODO);
  Serial.print('>');
}

void sendInfoToDsp() {
  //send a string of information for the mega to display
  // <  0         ,   1 ,2, 3  ,4,     5     ,6,      7    >
  // <cmdSendToDsp,speed,-,lightStatus,-,current odo>
  Serial.print('<'); Serial.print(CMD_SEND_TO_DSP);
  Serial.print(speedDisplay); //speed
  Serial.print('-');
  Serial.print(lightOn  ? 'o' : 'd'); //o = ON / d= dark or off
  Serial.print(lightPower);
  Serial.print('-');
  Serial.print(currentDistance);
  Serial.print('>');
  Serial.flush();
}
void processBtns() {
  btnOne.read();

  if (btnOne.wasReleased()) {
    if (!resetOdoSent) {
      //short press, go to menu
      Serial.print('<');
      Serial.print(CMD_BTN1_MENU);
      Serial.print('>');
      Serial.flush();
      inMenu = true;
    } else {
      resetOdoSent = false;
    }

  } else if (btnOne.pressedFor(1000) && !inMenu && !resetOdoSent) {
    //long press, reset odo
    revolutionCount = 0;
    currentDistance = 0.0;
    Serial.print('<');
    Serial.print(CMD_BTN1_RESET_ODO);
    Serial.print('>');
    Serial.flush();
    resetOdoSent = true;
  }

  btnTwo.read();
  if (btnTwo.wasPressed()) {

    if (!inMenu) { //if not in menu, send the light power information
      if (lightPower < 100) {
        lightPower = lightPower + 10;
        triggerDSPRefresh = true;
      }
    } else {
      Serial.print('<');
      Serial.print(CMD_BTN2_SHORT);
      Serial.print('>');
    }
  }

  btnThree.read();
  if (btnThree.wasPressed()) {

    if (!inMenu) { //if not in menu, send the light power information
      if (lightPower > 0) {
        lightPower = lightPower - 10;
        triggerDSPRefresh = true;
      }
    } else {
      Serial.print('<');
      Serial.print(CMD_BTN3_SHORT);
      Serial.print('>');
    }

  }
}
void processLdr() {
  //  Serial.print(F("processLDR: ");
  //  Serial.println(lightState);

  if (lightState == LIGHT_AUTO) {
    //  boolean currLightStatus = lightOn;

    ldrValue = analogRead(LDR);

    //Serial.print("Analog reading = ");
    //Serial.print(ldrValue);     // the raw analog reading

    // We'll have a few threshholds, qualitatively determined
    if (ldrValue < 400) {
      lightOn = true;
    } else {
      lightOn = false;
    }

  } else if (lightState == LIGHT_ON) {
    lightOn = true;
  } else if (lightState == LIGHT_OFF) {
    lightOn = false;
  }
  //  delay(500);
}
void turnOnOfflights() {

  int mosfetStatus = digitalRead(LIGHT_MOSFET);

  if (lightState == LIGHT_AUTO) {
    if (lightOn && mosfetStatus == LOW) {
      digitalWrite(LIGHT_MOSFET, HIGH);
    } else if (!lightOn && mosfetStatus == HIGH) {
      digitalWrite(LIGHT_MOSFET, LOW);
    }
  } else if (lightState == LIGHT_ON && mosfetStatus == LOW) {
    digitalWrite(LIGHT_MOSFET, HIGH);
  } else if (lightState == LIGHT_OFF && mosfetStatus == HIGH) {
    digitalWrite(LIGHT_MOSFET, LOW);
  }

}
void handleSerialRead() {
  if (newData == true) {
    newData = false;

    switch (receivedChars[0]) {
      case CMD_SEND_TO_DSP:  //request to send display information
        sendInfoToDsp();
        break;
      case CMD_MENU_END: //command out of menu option.
        inMenu = false;
        break;
      case CMD_WHEEL_DATA:
        char bufferCirc[3];
        bufferCirc[0] = receivedChars[1];
        bufferCirc[1] = receivedChars[2];
        bufferCirc[2] = receivedChars[3];
        wheelCir = atoi(bufferCirc); //convert to int
        break;
      case CMD_LIGHT_DATA:
        char bufferLight[3];
        bufferLight[0] = receivedChars[1];
        bufferLight[1] = '\0';
        lightState = atoi(bufferLight);
        break;
      case CMD_DUMP_DATA:  //request to send display information
        Serial.println("INDUMP");
        debugDump();
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
    } else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void debugDump() {
  Serial.println(F("---Debug Dump---"));
  Serial.print(F("Wheel circ: "));
  Serial.println(wheelCir);
  Serial.print(F("Speed: "));
  Serial.println(speedDisplay);
  Serial.print(F("currentDistance: "));
  Serial.println(currentDistance);
  Serial.print(F("revolutionCount: "));
  Serial.println(revolutionCount);
  Serial.print(F("lightPower: "));
  Serial.println(lightPower);
  Serial.print(F("inMenu: "));
  Serial.println(inMenu);
  Serial.print(F("saveSent: "));
  Serial.println(saveSent);
  Serial.print(F("started: "));
  Serial.println(started);
  Serial.print(F("Light state: "));
  Serial.println(lightState);

  Serial.print(F("receivedChars: "));
  Serial.println(receivedChars);


}