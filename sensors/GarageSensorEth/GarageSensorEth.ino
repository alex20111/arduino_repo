#include <UIPEthernet.h>

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
const char postUrlData[]  = "POST /web/sensors/garage/ HTTP/1.1";
const char postUrlPing[]  = "POST /web/sensors/ping/ HTTP/1.1";

uint8_t ledPowerOn = 3;// GREEN LED
uint8_t ledEthConn = 4; //ethernet active //WHITE LED
uint8_t ledConnected = 5; //connected to server // BLUE
uint8_t ledDoorStatus = 6; // YELLOW
uint8_t ledRespNotOk = 8; //RED LED

uint8_t doorPin = 7;

uint8_t doorStatus = 0;
uint8_t prevDoorStatus = 0;

const byte numChars = 8;
boolean newData = false;
char serialReceivedChars[8];

void setup() {
  data.reserve(40);
  //  Serial.begin(9600);
  //  Serial.println(F("Starting"));

  //  if (  Ethernet.begin(mac)  == 0) {
  //    Serial.println(F("Failed to configure Ethernet using DHCP"));
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  pinMode(ledPowerOn, OUTPUT);
  pinMode(ledEthConn, OUTPUT);
  pinMode(ledConnected, OUTPUT);
  pinMode(ledDoorStatus, OUTPUT);
  pinMode(ledRespNotOk, OUTPUT);
  pinMode(doorPin, INPUT);

  delay(2000);
  //  }

  //  Serial.println(F("Ready!!!"));

  prevTimer = millis() + 1800000;

  digitalWrite(ledPowerOn, HIGH);
//  Serial.println(Ethernet.localIP());
}

void loop() {

  doorStatus = digitalRead(doorPin);

  if (doorStatus == HIGH) {
    digitalWrite(ledDoorStatus, LOW);
  } else {
    digitalWrite(ledDoorStatus, HIGH);
  }

  if (Ethernet.linkStatus() != LinkOFF) {

    if (digitalRead(ledEthConn) != HIGH) { //Has ethernet connection
      digitalWrite(ledEthConn, HIGH);
    }

    checkIfConnectedToServer();

    readEthernetReply();

    if (prevDoorStatus != doorStatus) {
      prevDoorStatus = doorStatus;
      //send info
      data = "{ \"garageDoorStatus\":";
      data += doorStatus;
      data += "}";
      sendToClient(false);
    }

    if (millis() - prevTimer > 1800000) {  //3600000  // this will be the heart beat
      prevTimer = millis();
      pingServer();

    }
    //    //DEBUG
    //    if (millis() - prevTim > 10000) {
    //      prevTim = millis();
    //      Serial.print(F("is connected? 1= yes : "));
    //      Serial.println(client.connected());
    //    }

    //DEBUG

  } else {
    client.stop();
    //    Serial.println("Link off");
    if (digitalRead(ledEthConn) != LOW) {
      digitalWrite(ledEthConn, LOW);
    }
    if (digitalRead(ledConnected) != LOW) {
      digitalWrite(ledConnected, LOW);
    }
    delay(500);
  }
}

void sendToClient(boolean ping) {
  //  Serial.println(F("SendingToClient"));
  boolean connectedToServer = client.connected();
  if (!connectedToServer) {
    //    Serial.println(F("Connecting! to server"));
    if (client.connect(server, 8081)) {
//      Serial.println(F("connected!"));
      connectedToServer = true;
    } else {
      connectedToServer = false;
//      Serial.println(F("error, not conn!"));
    }
  }

  if (connectedToServer) {
    //    Serial.println(F("Connected sending data"));
    if (ping) {
      client.println(postUrlPing);
    } else {
      client.println(postUrlData);
    }
    client.println(F("Host: 192.168.1.110:8081"));
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: "));
    client.println(data.length());
    client.println();
    client.println(data);
    client.println();
    //    client.flush();
    //    client.stop();
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
    //    Serial.println(rc);
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
    if(strcmp(serialReceivedChars,"respOk") == 0){
      digitalWrite(ledRespNotOk, LOW);
    }else{
       digitalWrite(ledRespNotOk, HIGH);
    }
//    if (serialReceivedChars != 'respOk') {
//     
//      //     Serial.println(F("response not OK.. prolblem"));
//    } else {
//      
//    }

  }
}

void pingServer() {
  //  Serial.println(F("Ping sent"));
  prevTimer = millis();
  data = "{\"SensorId\":\"Garage Sensor\"}";
  sendToClient(true);
}
