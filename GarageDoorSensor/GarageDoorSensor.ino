/*
   WebSocketClient.ino

    Created on: 24.05.2015

*/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Arduino.h>

#include <ESP8266WiFi.h>

#include <WebSocketsClient.h>

#include <Hash.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
WebSocketsClient webSocket;

//#define USE_SERIAL Serial

const int REED_SWITCH = 2;
const int RESET_SWITCH = 0;

unsigned long delayto = 20 * 60 * 1000;//millis  10 min
unsigned long prevDelay = 0;

boolean wifiOn = true;
int prevStatus = 0;

String message;

WiFiManager wifiManager;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
//      USE_SERIAL.printf("[WSc] Disconnected!\n");

      break;
    case WStype_CONNECTED: {
//        USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
        int doorStatus = digitalRead(REED_SWITCH);
        sendMessage(doorStatus);
      }
      break;
    case WStype_TEXT:
//      USE_SERIAL.printf("[WSc] get text: %s\n", payload);

      // send message to server
      // webSocket.sendTXT("message here");
      break;
    case WStype_BIN:
//      USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
  }

}

void setup() {
  message.reserve(100);
  pinMode(REED_SWITCH, INPUT);
  pinMode(RESET_SWITCH, INPUT);

  // USE_SERIAL.begin(921600);
//  USE_SERIAL.begin(115200);

  //Serial.setDebugOutput(true);
//  USE_SERIAL.setDebugOutput(true);

//  USE_SERIAL.println();
//  USE_SERIAL.println();
//  USE_SERIAL.println();

//  for (uint8_t t = 4; t > 0; t--) {
//    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
//    USE_SERIAL.flush();
//    delay(1000);
//  }

  //set static ip
  //block1 should be used for ESP8266 core 2.1.0 or newer, otherwise use block2

  //start-block2
  IPAddress _ip = IPAddress(192, 168, 1, 212);
  IPAddress _gw = IPAddress(192, 168, 1, 1);
  IPAddress _sn = IPAddress(255, 255, 255, 0);
  //end-block2

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  wifiConnect();

  // server address, port and URL
  webSocket.begin("192.168.1.110", 8081, "/events/");

  // event handler
  webSocket.onEvent(webSocketEvent);

  // use HTTP Basic Authorization this is optional remove if not needed
  //	webSocket.setAuthorization("user", "Password");

  // try ever 5000 again if connection has failed
  //	webSocket.setReconnectInterval(5000);

  // start heartbeat (optional)
  // ping server every 15000 ms
  // expect pong from server within 3000 ms
  // consider connection disconnected if pong is not received 2 times
  //  webSocket.enableHeartbeat(15000, 3000, 2);
  prevDelay = millis();

}

void loop() {
  webSocket.loop();

  int doorStatus = digitalRead(REED_SWITCH);

  if (prevStatus != doorStatus) {

    if (!wifiOn) {
      turnOnWifi();
    } else {

      prevStatus = doorStatus;

      sendMessage(doorStatus);
    }
    prevDelay = millis();
  }

  if (millis() - prevDelay > delayto && wifiOn) {
    prevDelay = millis();
//    Serial.println("Delay triggered");
//    Serial.print("Wifi: ");
//    Serial.println(wifiOn);

    if (wifiOn) {
      turnOffWifi();
    }
    //    else {
    //      turnOnWifi();
    //    }
  }

  resetToCaptivePortal();
}
void sendMessage(int gdoorStatus) {
  String data = String(gdoorStatus);

  message = "{ \"operation\":\"2\", \"garageDoorStatus\":";
  message.concat(data);
  message.concat("}");
//  USE_SERIAL.println("Sending text");

  webSocket.sendTXT(message);
}
void turnOffWifi() {
//  Serial.println("Turning wifi off");
  webSocket.sendTXT("{ \"operation\": 99 }");
  wifiOn = false;
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );
}
void turnOnWifi() {
//  Serial.println("Turning wifi on");
  wifiOn = true;
  WiFi.forceSleepWake();
  delay( 1 );

  wifiConnect();
}

void wifiConnect() {
//  Serial.println("WIFI CONNECT");
  // Bring up the WiFi connection
  WiFi.mode( WIFI_STA );
  //  WiFi.begin( "MaxPower", "1A384Xkl81#");
  //tries to connect to last known settings
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP" with password "password"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("garageDoorAp", "garage12345")) {
//    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
//  Serial.println("connected...yeey :)");


//  Serial.println("local ip");
//  Serial.println(WiFi.localIP());
}
void resetToCaptivePortal(){
    int resetBtn = digitalRead(RESET_SWITCH);

  if (resetBtn == 0) {
    //  reset settings -
    wifiManager.resetSettings();
    delay(1000);
    ESP.reset();
  }
}
