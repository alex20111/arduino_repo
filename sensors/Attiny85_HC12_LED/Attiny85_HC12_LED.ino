#include <SoftwareSerial.h>

const byte HC12RxdPin = 2;                  // Recieve Pin on HC12
const byte HC12TxdPin = 3;                  // Transmit Pin on HC12

const byte numChars = 32;
char receivedChars[numChars];

boolean newData = false;

char identifier[] = "A1"; //increment everytime a new one is created..
boolean canProcessData = false; // Flag to see if it can process data.. Meaning that it has the correct identifier..


uint8_t ledPin = 1;

SoftwareSerial HC12(HC12TxdPin, HC12RxdPin); // Create Software Serial Port

void setup() {

  HC12.begin(9600);                         // Open serial port to HC12

  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);
}

void loop() {

  recvWithStartEndMarkers();
  handleRecv();

  if (canProcessData) {
    handleLed();
  }

}
void handleLed() {

  if (receivedChars[2] == '0')
  {
    digitalWrite(ledPin, LOW);
  } else if (receivedChars[2] == '1') {
    digitalWrite(ledPin, HIGH);
  }

}
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (HC12.available() > 0 && newData == false) {
    rc = HC12.read();

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
      canProcessData = false;
      recvInProgress = true;
    }
  }
}
//handle recieved data to see if it is assigned to that identifier
void handleRecv() {
  if (newData == true) {
    newData = false;

    if (sizeof(receivedChars) > 1 &&
        receivedChars[0] == identifier[0] &&
        receivedChars[1] == identifier[1]) {
      canProcessData = true;
    } else {
      canProcessData = false;
    }
  }
}
