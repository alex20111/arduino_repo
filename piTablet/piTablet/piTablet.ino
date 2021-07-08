#include <JC_Button.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

#define POWER_ON_EN   3  //power enable to the 5v regulator
#define POWER_BUTTON  4  //power button used to turning on/off 
#define PI_SHUTDOWN_SIG 5 //Silgnal (high) that will tell that the PI is completly shut down

#define POWER_LED     22 //Led will tell the user that Power is ON or OFF (green)
#define BATT_LOW_LED   6 // Battery low LED (Red)

//serial data
const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

const char START_MARKER = '<';
const char END_MARKER = '>';
const char SERIAL_READY = 'r';
const char PI_READY = 'b';
const char PI_SHUTDOWN = 's';
const char SEPERATOR = ':';
const char POWER_READING = 'p';
//---------------
enum powerStatus {
  unknown,
  PI_POWER_OFF,
  PI_BOOTED,
  PI_BOOTING,
  SHUTDOWN_REQ,
  PI_SHUTTING_DOWN
};
powerStatus pwrStatus = unknown;

//-- power led
boolean pulseLed = true;
long time = 0;

//----
Adafruit_INA219 ina219;
unsigned long prevPowerReading = 0;
boolean inaWorking = true;

Button powerBtn(POWER_BUTTON);       // define the button

unsigned long displayDelayPrev = 0;

void setup() {

  powerBtn.begin();

  Serial.begin(115200);
  pinMode(POWER_ON_EN, OUTPUT);
  pinMode(PI_SHUTDOWN_SIG, INPUT);
  pinMode(POWER_LED, OUTPUT);

  pinMode(13, OUTPUT);

  digitalWrite(POWER_ON_EN, HIGH); // turn Raspberry pi power.
  //  digitalWrite(13, HIGH); //Turn teensy led on
  //
  //  delay(500);
  //  digitalWrite(13, LOW);
  displayDelayPrev = millis();

  pwrStatus = PI_BOOTING;
  handlePowerLed();

  if (!ina219.begin()) {
    inaWorking  = false;
  }

}

void loop() {

  //    debugDisplay();

  handlePowerButtonAction();

  handlePowerLed();

  verifyPiShutdownPin();

  if (millis() - prevPowerReading > 5000) {
    handlePowerReading();
    prevPowerReading = millis();
  }

  recvWithStartEndMarkers();
  handleData();
}

void handlePowerButtonAction() {

  powerBtn.read();

  if (powerBtn.pressedFor(8000) ) {
    digitalWrite(POWER_ON_EN, LOW);
    analogWrite(POWER_LED, 0); // turn Raspberry pi power.
  } else if (powerBtn.pressedFor(2000) && pwrStatus != SHUTDOWN_REQ) {
    Serial.print(START_MARKER);
    Serial.print(PI_SHUTDOWN);
    Serial.print(END_MARKER);
    pwrStatus = SHUTDOWN_REQ;
  }
}

void handlePowerLed() {

  if (pwrStatus == PI_BOOTING) {
    time = millis();
    int value = 128 + 127 * cos(2 * PI / 2000 * time);
    analogWrite(POWER_LED, value);
    pulseLed = true;

  } else if (pwrStatus == PI_BOOTED && pulseLed) {
    pulseLed = false;
    analogWrite(POWER_LED, 255);
  }
}

void handlePowerReading() {
  if (inaWorking) {
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;
    float power_mW = 0;

    shuntvoltage = ina219.getShuntVoltage_mV();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    loadvoltage = busvoltage + (shuntvoltage / 1000);

    if (pwrStatus == PI_BOOTED) {
      Serial.print(START_MARKER);
      Serial.print(busvoltage);
      Serial.print(SEPERATOR);
      Serial.print(shuntvoltage);
      Serial.print(SEPERATOR);
      Serial.print(loadvoltage);
      Serial.print(SEPERATOR);
      Serial.print(current_mA);
      Serial.print(SEPERATOR);
      Serial.print(power_mW);
      Serial.print(END_MARKER);


      //      Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
      //      Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
      //      Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
      //      Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
      //      Serial.print("Power:         "); Serial.print(power_mW); Serial.println(" mW");
      //      Serial.println("");
    }
  } else {
    Serial.print(F("<INA-OUT>"));
  }
}
void verifyPiShutdownPin() {
  //  PI_SHUTDOWN_SIG
  //SHUTDOWN_REQ
  int sig = digitalRead(PI_SHUTDOWN_SIG);

  if (millis() - displayDelayPrev > 5000) {
    Serial.print(START_MARKER);
    Serial.print(sig);
    Serial.print(END_MARKER);
    displayDelayPrev = millis();
  }

  if (sig == HIGH && (  pwrStatus == PI_BOOTED || pwrStatus == SHUTDOWN_REQ ) ) {
    analogWrite(POWER_LED, 0);
    digitalWrite(POWER_ON_EN, LOW);
  }
}

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  //  char startMarker = '<';
  //  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != END_MARKER) {
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

    else if (rc == START_MARKER) {
      recvInProgress = true;
    }
  }
}

void handleData() {
  if (newData == true) {
    newData = false;
    if (receivedChars[0] == PI_READY && pwrStatus != SHUTDOWN_REQ) {
      pwrStatus = PI_BOOTED;
      Serial.print(START_MARKER);
      Serial.print(SERIAL_READY);
      Serial.print(END_MARKER);
      Serial.flush();

    }
  }
}




void debugDisplay() {

  if (millis() - displayDelayPrev > 500) {

    Serial.print(F("PWR btn read: "));
    Serial.println(powerBtn.read());
    if (powerBtn.read() == HIGH) {
      digitalWrite(13, HIGH);
    } else {
      digitalWrite(13, LOW);
    }

    displayDelayPrev = millis();
  }
}
