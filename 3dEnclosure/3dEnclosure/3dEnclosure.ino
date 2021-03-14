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

#define DEBOUNCE 0 //0 is fine for most fans, crappy fans may require 10 or 20 to filter out noise
#define FANSTUCK_THRESHOLD 500 //if no interrupts were received for 500ms, consider the fan as stuck and report 0 RPM

char cmd;

//serial
const byte numChars = 8;
char receivedChars[numChars];
boolean newData = false;
const char startMarker = '<';
const char endMarker = '>';

//pins
uint8_t lightPin = 12;
uint8_t fanPin = 9;
uint8_t hallsensor = 2;
uint8_t smokeSensor = A3;
uint8_t flame1Sensor = 3;
uint8_t pirSensor = 6;
uint8_t printerPower = 5;
uint8_t fanMosfet = 11;

//timers
unsigned long prevReading = 0;
unsigned long prevAirQualityReading = 0;

unsigned long volatile ts1 = 0, ts2 = 0;

void tachISR() {
  unsigned long m = millis();
  if ((m - ts2) > DEBOUNCE) {
    ts1 = ts2;
    ts2 = m;
  }
}
//Calculates the RPM based on the timestamps of the last 2 interrupts. Can be called at any time.
unsigned long calcRPM() {
  if (millis() - ts2 < FANSTUCK_THRESHOLD && ts2 != 0) {
    return (60000 / (ts2 - ts1)) / 2;
  } else return 0;
}

//This is the setup function where the serial port is initialised,
//and the interrupt is attached
void setup()
{

  pinMode(printerPower, OUTPUT);
  digitalWrite(printerPower, HIGH);

  Serial.begin(19200);
  pinMode(fanMosfet, OUTPUT);
  pinMode(hallsensor, INPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(smokeSensor, INPUT);
  pinMode(flame1Sensor, INPUT);
  pinMode(pirSensor, INPUT);

  pinMode(fanPin, OUTPUT);


  //  attachInterrupt(0, rpm, RISING);
  attachInterrupt(digitalPinToInterrupt(hallsensor), tachISR, FALLING); //set tachISR to be triggered when the signal on the sense pin goes low

  //  setPwmFrequency(fanPin, 1024);
  setupTimer1();

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
  digitalWrite(fanMosfet, LOW);
  setPWM1A(0);
  Serial.println(F("Ready"));
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

void fanSpeed(float spd) {

  if (spd == 0) {
    digitalWrite(fanMosfet, LOW);
  } else {
    digitalWrite(fanMosfet, HIGH);
  }
  setPWM1A(spd);
}

void sendRpm() {

  Serial.print(startMarker);
  Serial.print('r');
  Serial.print(calcRPM());
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

  int mq2 = analogRead(smokeSensor);
  uint8_t fire1 = digitalRead(flame1Sensor);
  uint8_t pir = digitalRead(pirSensor);
  //  uint8_t fire3 = digitalRead(flame3Sensor);

  Serial.print(startMarker);
  Serial.print('m');
  Serial.print(calcRPM());
  Serial.print("-");
  Serial.print(sensors.getTempCByIndex(0));
  Serial.print("-");
  Serial.print(mySensor.CO2);
  Serial.print("-");
  Serial.print(mySensor.TVOC);
  Serial.print("-");
  Serial.print(mq2);
  Serial.print("-");
  Serial.print(fire1);
  Serial.print("-");
  Serial.print(pir);
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
        fanSpeed(atof(bufferSpeed));
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

      case 'p':

        handlePrinterPower();
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

void handlePrinterPower() {

  char bufferPower[3];

  bufferPower[0] = receivedChars[1];
  bufferPower[1] = '\0';

  uint8_t printerPowerOn = atoi(bufferPower);

  if (printerPowerOn) {
    digitalWrite(printerPower, LOW);
  } else {
    digitalWrite(printerPower, HIGH);
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

void setPWM1A(float f) {
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  OCR1A = (uint16_t)(320 * f);
}
void setupTimer1() {
  //Set PWM frequency to about 25khz on pins 9,10 (timer 1 mode 10, no prescale, count to 320)
  TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
  TCCR1B = (1 << CS10) | (1 << WGM13);
  ICR1 = 320;
  OCR1A = 0;
  OCR1B = 0;
}
