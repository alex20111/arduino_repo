//UNO
#include <Arduino.h>

#define HALL_SWITCH 2

// speedometer variables
boolean inMetric = true;
float start, elapsed;
float wheelCir = 2.09; //in meters
unsigned long revolutionCount = 0;
int speedDisplay = 0;

//odometer//
float currentDistance = 0.0;

//serial//
const byte numChars = 8;
char receivedChars[numChars];
boolean newData = false;

//light//
uint8_t lightPower = 100;  //0 to 100

//buttons //
#define btn1 5
#define btn2 6
#define btn3 7

//commands -  Ex: mega ask for Speed , send: <1>  received <145>. 
const char cmdSlaveReady = '2'; //when slave (uno) is ready , automatically sent. Send; n/a reply <2ok>
const char cmdLightStatus = '4'; // light status automatically sent from uno. <4o10%> (ON) - <4n>(off) - <4b>(blink) 
const char cmdSendToDsp = '5'; //request all the require info to normal display. (Temp, speed, light status) from uno
const char cmdBtn1 = '6'; //  Options: odo reset, menu, menu select.
const char cmdBtn2 = '7'; // options: menu Up
const char cmdBtn3 = '8'; // options: menu down


void setup(void) {
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(HALL_SWITCH), speedInt, FALLING );

  pinMode(HALL_SWITCH, INPUT);

  start = millis();

  delay(300);
  
  //send ready!
  Serial.print('<');
  Serial.print(cmdSlaveReady);
  Serial.print('>');
}


void loop(void) {	

	recvWithStartEndMarkers() ;
	
	handleSerialRead();

}
void speedInt() {
  elapsed = millis() - start;
  start = millis();
  revolutionCount++;

  if (inMetric) {
    speedDisplay = (3600 * wheelCir) / elapsed;
    currentDistance = revolutionCount * wheelCir / 1000;
    //speedDisplay = (3600 * (wheelCir * .62137) ) / elapsed //in MPH
  }
}

void sendInfoToDsp(){
	//send a string of information for the mega to display
	// <  0         ,   1 ,2, 3  ,4,     5     ,6,      7    >
	// <cmdSendToDsp,speed,-,lightStatus,-,current odo>
	Serial.print('<');
	Serial.print(cmdSendToDsp); 
	Serial.print(speedDisplay); //speed
	Serial.print('-');	
	Serial.print("o12");
	Serial.print('-');
	Serial.print(currentDistance);
	Serial.print('>');
	Serial.flush();
}

void handleSerialRead(){
	if (newData == true) {
		newData = false;
		
		 switch (receivedChars[0]) {
			case cmdSendToDsp:  //request to send display information
				sendInfoToDsp();
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
      }else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}
