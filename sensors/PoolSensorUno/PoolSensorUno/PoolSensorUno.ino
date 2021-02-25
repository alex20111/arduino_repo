#include <DS3232RTC.h>

// arduino with internal christal 8mhz
#include <JC_Button.h>
#include <TM1637Display.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"

const uint8_t SEG_ERR[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_E | SEG_G,                         // r
  SEG_E | SEG_G,                         // r
  0,                                     // space
};

char identifier[] = "A2"; //increment everytime a new one is created..

// Module connection pins (Digital Pins)
#define CLK 4
#define DIO 5
#define ONE_WIRE_BUS 7
const uint8_t BTN1_PIN = 6;              // connect a button switch from this pin to ground
const uint8_t INTERRUPT_PIN = 2;
const uint8_t INTERRUPT_PIN2 = 3;
const uint8_t hc12SetPin = 8;

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

time_t ALARM_INTERVAL(5 * 60);    // alarm interval in seconds

void interruptHandler()
{

}
void bntInterrupt() {
  detachInterrupt(1);
  displayTemp = true;
}
void setup() {

  //H12 transmitter
  Serial.begin(9600);

  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(INTERRUPT_PIN2, INPUT_PULLUP);
  pinMode(hc12SetPin, OUTPUT);

  digitalWrite(hc12SetPin, HIGH);

  attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
  //TM1637
  display.setBrightness(0x02);

  //dallas thermometer
  sensors.begin();
  if (sensors.getDS18Count() == 0)
    display.setSegments(SEG_ERR);

  btn1.begin();

  displayConfig();

  initialiseClock();
  Serial.print('<');
  Serial.print(identifier);
  Serial.print(F("started"));
  Serial.print(ALARM_INTERVAL);
  Serial.print('>');
  Serial.flush();
  sendTemperature();
  delay(2000);
}

void loop() {

  if (!displayTemp) {

    reInitAlarm();
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
  
  attachInterrupt(0, interruptHandler, LOW);//attaching a interrupt to pin d2
  attachInterrupt(1, bntInterrupt, LOW);//attaching a interrupt to pin d2
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(0);

  HC12Wake();

  time_t t = RTC.get();
  time_t alarmTime = t + ALARM_INTERVAL;
  //Set New Alarm
  //  RTC.setAlarm(ALM1_MATCH_MINUTES , 0, minute(t) + time_interval, 0, 0);
  RTC.setAlarm(ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
  // clear the alarm flag
  RTC.alarm(ALARM_1);
  //  printAlarm(t);

}
void initialiseClock() {
  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, false);
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);

  tmElements_t tm;
  tm.Hour = 00;               // set the RTC to an arbitrary time
  tm.Minute = 00;
  tm.Second = 00;
  tm.Day = 4;
  tm.Month = 2;
  tm.Year = 2021 - 1970;      // tmElements_t.Year is the offset from 1970
  time_t t = makeTime(tm);        // change the tm structure into time_t (seconds since epoch)
  time_t alarmTime = t + ALARM_INTERVAL;    // calculate the alarm time

  // set the current time
  RTC.set(t);
  //  RTC.setAlarm(ALM1_MATCH_MINUTES , 0, minute(t) + time_interval, 0, 0); // Setting alarm 1 to go off 5 minutes from now
  RTC.setAlarm(ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);

  // clear the alarm flag
  RTC.alarm(ALARM_1);
  // configure the INT/SQW pin for "interrupt" operation (disable square wave output)
  RTC.squareWave(SQWAVE_NONE);
  // enable interrupt output for Alarm 1
  RTC.alarmInterrupt(ALARM_1, true);

}

void handleDisplayTemp() {

  display.setBrightness(7, true);
  display.showNumberDec((int)temperatureF);
}

void displayConfig() {
  unsigned long currMillis = millis();
  display.setBrightness(7, true);
  uint8_t intv = 1;

  display.showNumberDec(intv);
  currMillis = millis();

  while (true) {
    btn1.read();
    if (btn1.wasPressed()) {

      if (intv < 250) {
        intv++;
      }
      display.showNumberDec(intv);
      currMillis = millis();
    }

    if (millis() - currMillis > 5000) {
      break;
    }

  }
  ALARM_INTERVAL = intv * 60;

  display.setBrightness(7, false);
  display.showNumberDec(0);
}
void sendTemperature() {
  sensors.requestTemperatures();
  temperatureF =     sensors.getTempFByIndex(0);
  Serial.print('<');
  Serial.print(identifier);
  Serial.print(temperatureF);
  Serial.print('>');
  Serial.flush();

  delay(2000);//wait for the HC12 to have time to transmit before shutting down.. 
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
  
  awaitHC12Response();
}
void awaitHC12Response() {
  uint8_t counter = 0;
  boolean breakLoop = false;
  //wait
  while (counter < 10) { //wait for answer up to 1 second
    while (Serial.available()  > 0 ) {
      breakLoop = true;
      char r = Serial.read();
      if (r == 'O') {
        //we found the letter O.
        break;
      }
    }
    delay(100);
    if (breakLoop) {
      break;
    }
    counter++;
  }
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


//requirement
//take pool reading every 20 mintues then sleep
//if the display button is pressed, display temperature.
//ability to change sleep time..
//inside case
//2 buttons -- press button 1: mode 1 = run normal, mode 2= set time to sleep.
//            -- press button 2 - enable the prev selected mode ( if mode 1 selected, then nothing else to do) , mode 2 - display time to sleep
//          -- press button 1 - increase by 1 min until 60 minutes. press button 2 to enable.
//outside 1 button, display time when pressed.
//when turning on, display temp, send 1st reading then sleep. 20sec delay.
//when running . if button is presses, wake up, display temp then go back to sleep after 30 sec.
//when running , if confing button 1 pressed, wake up and enter cfg mode.




//// Arduino Button Library
//// https://github.com/JChristensen/JC_Button
//// Copyright (C) 2018 by Jack Christensen and licensed under
//// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
////
//// Example sketch to turn an LED on and off with a tactile button switch.
//// Wire the switch from the Arduino pin to ground.
//
//#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
//
//// pin assignments
//const byte
//BUTTON_PIN(5),              // connect a button switch from this pin to ground
//           LED_PIN(4);                // the standard Arduino "pin 13" LED
//
//Button myBtn(BUTTON_PIN);       // define the button
//
//void setup()
//{
//  myBtn.begin();              // initialize the button object
//  pinMode(LED_PIN, OUTPUT);   // set the LED pin as an output
//}
//
//void loop()
//{
//  static bool ledState;       // a variable that keeps the current LED status
//  myBtn.read();               // read the button
//
//  if (myBtn.wasPressed())    // if the button was released, change the LED state
//  {
//    //        ledState = !ledState;
//    digitalWrite(LED_PIN, HIGH);
//  } else if (myBtn.wasReleased())  {
//    digitalWrite(LED_PIN, LOW);
//  }
//}
