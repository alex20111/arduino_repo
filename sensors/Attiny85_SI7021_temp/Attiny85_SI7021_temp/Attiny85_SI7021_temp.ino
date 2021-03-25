
#include <SystemStatus.h>
#include <SoftwareSerial.h>
#include <SI7021.h>
#include <DS3232RTC.h>

SI7021 sensor;

const byte HC12RxdPin = 3;                  // Recieve Pin on HC12
const byte HC12TxdPin = 4;                  // Transmit Pin on HC12
SoftwareSerial HC12(HC12TxdPin, HC12RxdPin); // Create Software Serial Port

time_t ALARM_INTERVAL(5 * 60);    // alarm interval in seconds


uint8_t hc12SetPin = 5;
// these values are taken from the HC-12 documentation v2 (+10ms for safety)
uint8_t hc12setHighTime = 90;
uint8_t hc12setLowTime = 50;
uint8_t hc12cmdTime = 100;

void setup() {
  sensor.begin();

  HC12.begin(9600);                         // Open serial port to HC12

  time_t t = RTC.get();

  HC12.print(F("<awake>"));

  SystemStatus().getVCC();
initialiseClock();
}

void loop() {
  
  SystemStatus().SleepWakeOnInterrupt(1);

  si7021_env data = sensor.getHumidityAndTemperature();
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
void HC12Sleep() {
  sendH12Cmd("AT+SLEEP");
}
void sendH12Cmd(const char cmd[]) {
  digitalWrite(hc12SetPin, LOW);
  delay(hc12setLowTime);

  HC12.print(cmd);
  HC12.flush();
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
    while (HC12.available()  > 0 ) {
      breakLoop = true;
      char r = HC12al.read();
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
