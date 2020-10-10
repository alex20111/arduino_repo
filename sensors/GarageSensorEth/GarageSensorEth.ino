#include <Bounce2.h>
#include <UIPEthernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>

enum urlEnum {
  GARAGE_STATUS,
  GARAGE_TEMP,
  PING
};

EthernetClient client; //dd:e4:98:ab:e7:59
uint8_t mac[6] = {0xdd, 0xe4, 0x98, 0xab, 0xe7, 0x59}; //Arduino Uno pins: 10 = CS, 11 = MOSI, 12 = MISO, 13 = SCK

IPAddress ip(192, 168, 1, 121);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

const char server[] = "192.168.1.110";

unsigned long prevTimer = 0;
//unsigned long prevTim = 0; //remove

String data = "";
const char postUrlData[] PROGMEM = "POST /web/sensors/garage/ HTTP/1.1";
const char postUrlPing[] PROGMEM = "POST /web/sensors/ping/ HTTP/1.1";
const char pingText[] PROGMEM = "{\"SensorId\":\"Garage Sensor\"}";
const char postSensorUpdUrl[] PROGMEM = "POST /web/sensors/sensorUpdate/ HTTP/1.1";

const uint8_t ledPowerOn = 3;// GREEN LED
const uint8_t ledEthConn = 4; //ethernet active //WHITE LED
const uint8_t ledConnected = 5; //connected to server // BLUE
const uint8_t ledDoorStatus = 6; // YELLOW
const uint8_t ledRespNotOk = 8; //RED LED
const uint8_t doorPin = 7;

uint8_t doorStatus = 0;
uint8_t currentState = 0; // current state of the door that was sent to the server
boolean sendMessage = true;

const byte numChars = 8;
boolean newData = false;
char serialReceivedChars[8];

//temperature
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
float temperatureCelcius = 0.0;
unsigned long sensorDataSentTimer = 0;

//Button
Bounce doorSwitch = Bounce(); // Instantiate a Bounce object

void setup() {
  data.reserve(40);
  //    Serial.begin(9600);
  //  Serial.println(F("Starting"));

  //  if (  Ethernet.begin(mac)  == 0) {
  //    Serial.println(F("Failed to configure Ethernet using DHCP"));
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  pinMode(ledPowerOn, OUTPUT);
  pinMode(ledEthConn, OUTPUT);
  pinMode(ledConnected, OUTPUT);
  pinMode(ledDoorStatus, OUTPUT);
  pinMode(ledRespNotOk, OUTPUT);

  delay(2000);

  prevTimer = millis() + 1800000;

  digitalWrite(ledPowerOn, HIGH);
  //  Serial.println(Ethernet.localIP());
  doorSwitch.attach ( doorPin , INPUT );
  doorSwitch.interval( 25 );

  // Start up the library
  sensors.begin(); //dallas temperature
}

void loop() {

  doorSwitch.update();

  if (doorSwitch.read() == 1) {
    digitalWrite(ledDoorStatus, LOW);
  } else if (doorSwitch.read() == 0) {
    digitalWrite(ledDoorStatus, HIGH);
  }

  if (currentState != doorSwitch.read()) {
    currentState = doorSwitch.read();
    sendMessage = true;
  }

  if (Ethernet.linkStatus() != LinkOFF) {

    if (digitalRead(ledEthConn) != HIGH) { //Has ethernet connection
      digitalWrite(ledEthConn, HIGH);
    }

    checkIfConnectedToServer();

    readEthernetReply();

    if (sendMessage) {
      sendMessage = false;
      //send info
      data = "{ \"garageDoorStatus\":";
      data += currentState;
      data += "}";
      sendToClient(GARAGE_STATUS);
    }

    if (millis() - prevTimer > 1800000) {  //3600000  // this will be the heart beat
      pingServer();
    }
    if (millis() - sensorDataSentTimer > 300000) { // 5 min temperature sensor
      sensorDataSentTimer = millis();
      requestTemperature();
    }

  } else {

    client.stop();
    //    Serial.println("Link off");
    if (digitalRead(ledEthConn) != LOW) {
      digitalWrite(ledEthConn, LOW);
    }
    if (digitalRead(ledConnected) != LOW) {
      digitalWrite(ledConnected, LOW);
    }
    delay(100);
  }
}
void sendToClient(urlEnum urlToGo) {
  //  Serial.println(F("SendingToClient"));
  boolean connectedToServer = client.connected();
  if (!connectedToServer) {
    if (client.connect(server, 8081)) {
      connectedToServer = true;
    } else {
      connectedToServer = false;
    }
  }

  if (connectedToServer) {

    if (urlToGo == PING) {
      client.println((__FlashStringHelper *)postUrlPing);
    } else if (urlToGo == GARAGE_STATUS) {
      client.println((__FlashStringHelper *)postUrlData);
    } else if (urlToGo == GARAGE_TEMP) {
      client.println((__FlashStringHelper *)postSensorUpdUrl);
    }
    client.println(F("Host: 192.168.1.110:8081"));
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: "));
    client.println(data.length()); //postUrlData
    client.println();
    client.println(data);
    client.println();
  }
}

void checkIfConnectedToServer() {
  if (client.connected()) {
    digitalWrite(ledConnected, HIGH);
  } else {
    digitalWrite(ledConnected, LOW);
  }
}

void readEthernetReply() {
  char endMarker = '>';
  char startMarker = '<';
  boolean recvInProgress = false;
  byte ndx = 0;
  char rc;

  while (client.available() && newData == false) {
    rc = client.read();
    //        Serial.print(rc);
    if (recvInProgress == true) {
      if (rc != endMarker) {
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
        newData = true;
      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
  if (newData) {
    newData = false;
    if (strcmp(serialReceivedChars, "respOk") == 0) {
      digitalWrite(ledRespNotOk, LOW);
    } else {
      digitalWrite(ledRespNotOk, HIGH);
    }
  }
}

void pingServer() {
  //  Serial.println(F("Ping sent"));
  prevTimer = millis();
  data = (__FlashStringHelper *)pingText; // "{\"SensorId\":\"Garage Sensor\"}";
  sendToClient(PING);
}
void requestTemperature() {
  sensors.requestTemperatures();
  temperatureCelcius = sensors.getTempCByIndex(0);

  data = "{'operation':4,'sensorValue':";
  data += temperatureCelcius;
  data += "}";
  sendToClient(GARAGE_TEMP);
}
