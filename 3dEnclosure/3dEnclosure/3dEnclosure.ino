// First we include the libraries
#include <OneWire.h>
#include <DallasTemperature.h>
/********************************************************************/
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);



char cmd;
//Varibles used for calculations
int NbTopsFan;
int Calc;

//The pin location of the sensor
int hallsensor = 2;

typedef struct {

  //Defines the structure for multiple fans and
  //their dividers
  char fantype;
  unsigned int fandiv;
} fanspec;

//Definitions of the fans
//This is the varible used to select the fan and it's divider,
//set 1 for unipole hall effect sensor
//and 2 for bipole hall effect sensor
fanspec fanspace[3] = {{0, 1}, {1, 2}, {2, 8}};
char fan = 1;

//serial
const byte numChars = 8;
char receivedChars[numChars];
boolean newData = false;

//light pins
uint8_t lightPin = 12;
uint8_t fanPin = 11;

void rpm ()
//This is the function that the interupt calls
{
  NbTopsFan++;
}

//This is the setup function where the serial port is initialised,
//and the interrupt is attached
void setup()
{

  pinMode(hallsensor, INPUT);
  pinMode(lightPin, OUTPUT);

  Serial.begin(9600);
  attachInterrupt(0, rpm, RISING);

  setPwmFrequency(fanPin, 1024);
  pinMode(fanPin, OUTPUT);
  analogWrite(fanPin, 255);


  digitalWrite(lightPin, LOW);

  sensors.begin();
  Serial.println("Ready");
}

void loop ()
//Set NbTops to 0 ready for calculations
{

  calculateRPM();

  recvWithStartEndMarkers();

  handleSerialRead();

}

void fanSpeed(byte spd) {
  analogWrite(fanPin, spd);
}

void sendRpm() {
  Calc = ((NbTopsFan * 60) / fanspace[fan].fandiv);
  Serial.print('r');
  Serial.print(Calc);
  Serial.flush();

}

void calculateRPM() {
  //    Serial.println("Start");
  NbTopsFan = 0;

  //Enables interrupts
  sei();

  //Wait 1 second
  delay (1000);

  //Disable interrupts
  cli();
}

void fetchTemperature() {
  sensors.requestTemperatures(); // Send the command to get temperature readings
  Serial.print('t');
  Serial.print(sensors.getTempCByIndex(0));
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
//        Serial.print(bufferSpeed);
        //        ldrValue = atoi(bufferLDR);
        fanSpeed(atoi(bufferSpeed));
        break;
      case 'l':  //request to send display information<s
        handleLight();
        break;
      case 't':
        fetchTemperature();
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
