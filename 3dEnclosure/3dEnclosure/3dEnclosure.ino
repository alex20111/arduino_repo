// First we include the libraries
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SparkFun_SGP30_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_SGP30
#include <Wire.h>

/********************************************************************/
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);


SGP30 mySensor; //create an object of the SGP30 class
SGP30ERR error;

char cmd;
//Varibles used for calculations
int NbTopsFan;
int Calc;
boolean reading = true;

//serial
const byte numChars = 8;
char receivedChars[numChars];
boolean newData = false;
const char startMarker = '<';
const char endMarker = '>';

//pins
uint8_t lightPin = 12;
uint8_t fanPin = 11;
uint8_t hallsensor = 2;
uint8_t smokeSensor = A3;
uint8_t flame1Sensor = 3;
uint8_t flame2Sensor = 6;
uint8_t flame3Sensor = 5;

//timers
unsigned long prevReading = 0;
unsigned long prevAirQualityReading = 0;

void rpm ()
{
  NbTopsFan++;

  if (millis() - prevReading > 1000 && reading) {
    prevReading = millis();
    Calc = NbTopsFan * 60;
    NbTopsFan = 0;
  }
}

//This is the setup function where the serial port is initialised,
//and the interrupt is attached
void setup()
{
  Serial.begin(19200);
  pinMode(hallsensor, INPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(smokeSensor, INPUT);
  pinMode(flame1Sensor, INPUT);
  pinMode(flame2Sensor, INPUT);
  pinMode(flame3Sensor, INPUT);
  pinMode(fanPin, OUTPUT);


  attachInterrupt(0, rpm, RISING);

  setPwmFrequency(fanPin, 1024);

  analogWrite(fanPin, 255);

  digitalWrite(lightPin, LOW);

  sensors.begin();

  Wire.begin();
  boolean spg30Init = true;
  //Initialize sensor
  if (mySensor.begin() == false) {
    Serial.println("No SGP30 Detected.");
    spg30Init = false;

  }
  if (spg30Init) {
    //Initializes sensor for air quality readings
    //measureAirQuality should be called in one second increments after a call to initAirQuality
    mySensor.initAirQuality();
  }

  Serial.println("Ready");
}

void loop ()
//Set NbTops to 0 ready for calculations
{
  recvWithStartEndMarkers();

  handleSerialRead();

  if (millis() - prevAirQualityReading > 1000) {
    prevAirQualityReading  = millis();
    error = mySensor.measureAirQuality();
  }

}

void fanSpeed(byte spd) {
  if (spd == 255) {
    reading = false;
    Calc = 0;
  } else {
    reading = true;
  }

  analogWrite(fanPin, spd);
}

void sendRpm() {
  //  Calc = ((NbTopsFan * 60) / fanspace[fan].fandiv);
  Serial.print(startMarker);
  Serial.print('r');
  Serial.print(Calc);
  Serial.print(endMarker);
  Serial.flush();


}

void getAirQuality() {

  if (error == SUCCESS) {
    Serial.print(startMarker);
    Serial.print('a');
    Serial.print(mySensor.CO2);
    Serial.print("-");
    Serial.print(mySensor.TVOC);
    Serial.print(endMarker);
    Serial.flush();
  } else {
    Serial.print(startMarker);
    Serial.print('a');
    Serial.print("err");
    Serial.print(endMarker);
    Serial.flush();
  }
}

void fetchTemperature() {
  sensors.requestTemperatures(); // Send the command to get temperature readings
  Serial.print(startMarker);
  Serial.print('t');
  Serial.print(sensors.getTempCByIndex(0));
  Serial.print(endMarker);
  Serial.flush();
}

void getAllSensors() {
  sensors.requestTemperatures();

  Serial.print(startMarker);
  Serial.print('m');
  Serial.print(Calc);
  Serial.print("-");
  Serial.print(sensors.getTempCByIndex(0));
  Serial.print("-");
  Serial.print(mySensor.CO2);
  Serial.print("-");
  Serial.print(mySensor.TVOC);
  Serial.print(endMarker);
  Serial.flush();
}

void handleSerialRead() {
  if (newData == true) {
    newData = false;

    //Serial.println(receivedChars);
    switch (receivedChars[0]) {

      case 'r':  //request to send display information<s
        sendRpm();
        break;
      case 's':  //request to send display information<s
        char bufferSpeed[5];

        for (uint8_t i = 1; i < strlen(receivedChars) + 1 ; i++) { //the +1 is to include the \0 char.
          bufferSpeed[i - 1] = receivedChars[i];
        }
        fanSpeed(atoi(bufferSpeed));
        break;
      case 'l':  //request to send display information<s
        handleLight();
        break;
      case 't':
        fetchTemperature();
        break;
      case 'a':
        getAirQuality();
        break;
      case 'm':
        getAllSensors();
        break;

    }
  }
}

void handleLight() {
  char bufferLight[3];

  bufferLight[0] = receivedChars[1];
  bufferLight[1] = '\0';

  uint8_t on = atoi(bufferLight);

  if (on) {
    digitalWrite(lightPin, HIGH);
  } else {
    digitalWrite(lightPin, LOW);
  }
}
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;

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
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if (pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch (divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if (pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if (pin == 3 || pin == 11) {
    switch (divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
