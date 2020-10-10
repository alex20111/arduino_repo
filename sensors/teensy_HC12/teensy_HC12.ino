const byte numChars = 42;
char serialReceivedChars[numChars];
boolean serialData = false;

//HC-12 serial
char HC12ReceivedChars[numChars];
boolean serial1Data = false;

const char START_MARKER = '<';
const char END_MARKER = '>';

const int LED_POWER_PIN = 4;

void setup() {
  Serial.begin(9600);

  Serial1.begin(9600); //HC-12  -- send

  pinMode(LED_POWER_PIN, OUTPUT);

  while (!Serial) {
    // wait for serial port to connect. Needed for native USB
  }

  Serial.println("Ready");
  digitalWrite(LED_POWER_PIN, HIGH);
}

void loop() {

  //serial sending data to HC-12
  serialrecvWithStartEndMarkers();
  sendingToHC12();

  //from HC-12
  HC12recvWithStartEndMarkers();
  sendingToHost();
}

void serialrecvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char rc;

  while (Serial.available() > 0 && serialData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != END_MARKER) {
        serialReceivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        serialReceivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        serialData = true;
      }
    }

    else if (rc == START_MARKER) {
      recvInProgress = true;
    }
  }
}

void sendingToHC12() {
  if (serialData == true) {
    Serial1.print(START_MARKER);
    Serial1.print(serialReceivedChars);
    Serial1.print(END_MARKER);
    Serial1.flush();
    serialData = false;
  }
}

void HC12recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char rc;

  while (Serial1.available() > 0 && serial1Data == false) {
    rc = Serial1.read();

    if (recvInProgress == true) {
      if (rc != END_MARKER) {
        HC12ReceivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        HC12ReceivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        serial1Data = true;
      }
    }
    else if (rc == START_MARKER) {
      recvInProgress = true;
    }
  }
}

void sendingToHost() {
  if (serial1Data == true) {
    Serial1.print(START_MARKER);
    Serial.print(HC12ReceivedChars);
    Serial1.print(END_MARKER);
    Serial1.flush();
    serial1Data = false;
  }
}
