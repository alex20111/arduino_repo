//  WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
//    WL_IDLE_STATUS      = 0,
//     WL_NO_SSID_AVAIL    = 1,
//  WL_SCAN_COMPLETED   = 2,
//   WL_CONNECTED        = 3,
//    WL_CONNECT_FAILED   = 4,
//     WL_CONNECTION_LOST  = 5,
//     WL_DISCONNECTED     = 6

#include <FS.h>                   //this needs to be first, or it all crashes and burns... 
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino  

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager 

#include <OneWire.h>         //https://github.com/PaulStoffregen/OneWire/blob/master/library.json
#include <DallasTemperature.h>   //https://github.com/milesburton/Arduino-Temperature-Control-Library/blob/master/library.json

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <Arduino.h>
#include <TM1637Display.h>

TM1637Display display(0, 12);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(2);

//flag for saving data
boolean shouldSaveConfig = false;

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature DS18B20(&oneWire);

char temperatureFString[6]; //temperature in string
char voltage[6];       //temperature in string
float tempRounded       = 0.0; //temperature rounded up
float battVolt        = 0.0;
unsigned long deepSleep   = 10 * 60 * 1000000; // 10 min (min * secInMinute * microsec)

String message; //message to send to server

//default custom static IP
char static_ip[16] = "0";
char static_gw[16] = "0";
char static_sn[16] = "0";

ADC_MODE(ADC_VCC);

//comment to de-activate debug
//#define DEBUG

void setup(void)
{
  #ifdef DEBUG
    Serial.begin(115200);
    delay(300);
  #endif
  
  pinMode(16, INPUT);
  pinMode(14, INPUT);
  int displaTempBtn = digitalRead(16); //basically if the button is low, then it's a manual reset.
  int startAPConfigBtn = digitalRead(14); //

  message.reserve(220);
  display.setBrightness(0x0f);

#ifdef DEBUG
  Serial.println(F("Begin"));
  Serial.print(F("Reset reason:"));
  Serial.println(ESP.getResetReason());
#endif

  //get battery voltage
  battVolt = (ESP.getVcc() / 1000.0); //get voltage for battery
  dtostrf(battVolt, 2, 2, voltage); //double to string

#ifdef DEBUG
  Serial.print(F("Voltage: "));
  Serial.println(voltage);
  Serial.print(F("Button gpio 16 readout "));
  Serial.println(displaTempBtn);
  Serial.print(F("Button startAPConfigBtn "));
  Serial.println(startAPConfigBtn);
#endif

  if (displaTempBtn == 0 && startAPConfigBtn == 0) { //display by pressing button
#ifdef DEBUG
    Serial.println(F("Manual Temperature display"));
#endif

    //display temperature and then sleep
    getTemperature();
    display.setBrightness(7, true);
    display.showNumberDec((int) tempRounded);

    delay(8000);
    display.setBrightness(7, false);
    display.showNumberDec(0);

    ESP.deepSleep(deepSleep); //10 minute
  } else if (displaTempBtn == 0 && startAPConfigBtn == HIGH) {
    staticIpManager(); //mount operating system.
    //start the AP in config mode
    startPortal();
  } else if (WiFi.SSID().length() > 0) { //check if we previously connected to an SSId
#ifdef DEBUG
    Serial.print(F("Previously connected to: "));
    Serial.print(WiFi.SSID());Serial.print(" ");    
    Serial.print(F("length: " ));
    Serial.println(WiFi.SSID().length());
#endif

    int retryConnect = 0; //try 4 times to reconnect.

    WiFi.mode(WIFI_STA);

    staticIpManager(); //get ip form file.

    boolean connected = false;

    //convert char to int for the allowed ip address.
    char buffer[3];
    buffer[0] = static_ip[0];
    buffer[1] = static_ip[1];
    buffer[2] = static_ip[2];

    int ipsuffix ;
    ipsuffix = atoi(buffer);
#ifdef DEBUG
    Serial.print("ip suffix: ");
    Serial.print(ipsuffix);
    Serial.print("\n");
#endif
   
    if (ipsuffix == 192) {
      //load static ip from disk
      IPAddress staticIP, gateway, subnet;
      staticIP.fromString(static_ip);
      gateway.fromString(static_gw);
      subnet.fromString(static_sn);

      WiFi.config(staticIP, gateway, subnet);

      WiFi.begin();

#ifdef DEBUG
      Serial.print(F("trying to connect static address(IP, Gateway, Subnet): "));
      Serial.println(staticIP); Serial.println(gateway); Serial.println(subnet);
#endif

      //wait until connected
      while (retryConnect < 5) {
        for (int i = 0 ; i < 20; i++) {
          if (WiFi.status() != WL_CONNECTED) {
            delay(300);
#ifdef DEBUG
            Serial.print(".");
#endif
          } else if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
          }
        }
        if (connected) {
          //got connection, break the loop
          break;
        } else if (!connected) { //for loop finished and not connected, try to reconnect.
          WiFi.reconnect();

          retryConnect++;
#ifdef DEBUG
          Serial.print(F("Connection failed, to "));
          Serial.print(WiFi.SSID());
          Serial.print(F(" with error code: "));
          Serial.println(WiFi.status());
          Serial.print(F("Retrying count: "));
          Serial.print(retryConnect);
#endif
          delay(1000);
        }
      }
      //if we have a successful connection
      if (connected) {
#ifdef DEBUG
        Serial.println(F("Connected . Ip:"));
        Serial.print(WiFi.localIP());
#endif

        //get temperature
        getTemperature();

        //send it
        sendTemperature();

        //then sleep
        ESP.deepSleep(deepSleep); //10 min
      } else {
#ifdef DEBUG
        Serial.println(F("All retries failed, entering sleep."));
#endif
        ESP.deepSleep(30 * 60 * 1000000); //30 min
      }
    }else{
      #ifdef DEBUG
        Serial.println(F("Disk ip address invalid, starting AP"));
      #endif

      startPortal();
    }
  } else { //not previously connected, start AP config.
    //start AP config so we can do the configuration
#ifdef DEBUG
    Serial.print(F("NOT Previously connected (Initial), starting portal. "));
#endif
    staticIpManager(); //mount operating system. needed to initialize the spiff.
    startPortal();
  }
}

void loop() {}

//start the Ap portal
void startPortal() {
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

#ifdef DEBUG
  wifiManager.setDebugOutput(true); //do not display if debug is off
#endif

  //reset settings - for testing
  wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);

  IPAddress _ip = IPAddress(10, 0, 1, 78);
  IPAddress _gw = IPAddress(10, 0, 1, 1);
  IPAddress _sn = IPAddress(255, 255, 255, 0);
  //end-block2

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //it starts an access point
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.startConfigPortal("PoolAP", "password1")) {
#ifdef DEBUG
    Serial.println("failed to connect and hit timeout, sleep for all time");
#endif
    delay(3000);
    ESP.deepSleep(0); //sleep forever until reset.
  } else {
#ifdef DEBUG
    Serial.println("Connected");
    Serial.print(F("local ip: "));
    Serial.println(WiFi.localIP());
#endif
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      saveConfig();
    }
  }
  ESP.deepSleep(1000000); //sleep 1 sec and restrart.
}

void sendTemperature() {

  int retry = 0;

  while (retry < 3) {
#ifdef DEBUG
    Serial.print(F("Sending message . Nbr of retry"));
    Serial.println(retry);
#endif

    WiFiClient client;
    if (client.connect("192.168.1.110", 8081))
    {
      message = "GET /web/service.action?cmd=add&type=temp&jsonObject={'tempC':";
      message.concat(temperatureFString);
      message.concat(",'recorderName':'pool','batteryLevel':'");
      message.concat(voltage);
      message.concat("'} HTTP/1.1\r\nHost: 192.168.1.110:8081\r\nConnection: close\r\n\r\n");
#ifdef DEBUG
      Serial.print(F("Sent Message"));
      Serial.println(message);
      Serial.println(F("[Response:]"));
#endif

      client.print(message);

      while (client.connected())
      {
        if (client.available())
        {
          String line = client.readStringUntil('\n');
#ifdef DEBUG
          Serial.println(line);
#endif
        }
      }
      client.stop();
#ifdef DEBUG
      Serial.println(F("\n[Disconnected]"));
#endif
      retry = 5;
    }
    else
    {
#ifdef DEBUG
      Serial.println(F("connection failed!]"));
#endif
      retry ++;
      client.stop();
    }
  }
}
void getTemperature() {

  float tempF;
  do {
    DS18B20.requestTemperatures();
    tempF = DS18B20.getTempFByIndex(0);

  } while (tempF >= 185.0 || tempF == (-196.6));

  dtostrf(tempF, 2, 2, temperatureFString);

#ifdef DEBUG
  Serial.print(F("Temp from function:"));
  Serial.println(tempF);
#endif

  tempRounded = tempF + 0.4;
}

void staticIpManager() {
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
#ifdef DEBUG
  Serial.println(F("mounting FS..."));
#endif

  if (SPIFFS.begin()) {
#ifdef DEBUG
    Serial.println(F("mounted file system"));
#endif
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
#ifdef DEBUG
      Serial.println(F("reading config file"));
#endif
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
#ifdef DEBUG
        Serial.println(F("opened config file"));
#endif
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
#ifdef DEBUG
        json.printTo(Serial);
#endif
        if (json.success()) {
#ifdef DEBUG
          Serial.println(F("\nparsed json"));
#endif

          if (json["ip"]) {
#ifdef DEBUG
            Serial.println(F("setting custom ip from config"));
#endif

            //static_ip = json["ip"];
            strcpy(static_ip, json["ip"]);
            strcpy(static_gw, json["gateway"]);
            strcpy(static_sn, json["subnet"]);

#ifdef DEBUG
            Serial.println(static_ip);
#endif

          } else {
#ifdef DEBUG
            Serial.println(F("no custom ip in config"));
#endif
          }
        } else {
#ifdef DEBUG
          Serial.println(F("failed to load json config"));
#endif
        }
      }
    }
  } else {
#ifdef DEBUG
    Serial.println(F("failed to mount FS"));
#endif
  }
}

void saveConfig() {
#ifdef DEBUG
  Serial.println(F("saving confi to disk"));
#endif

  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["ip"] = WiFi.localIP().toString();
  json["gateway"] = WiFi.gatewayIP().toString();
  json["subnet"] = WiFi.subnetMask().toString();

  File configFile = SPIFFS.open("/config.json", "w");
#ifdef DEBUG
  if (!configFile) {
    Serial.println(F("failed to open config file for writing"));
  }

  json.prettyPrintTo(Serial);
#endif

  json.printTo(configFile);
  configFile.close();
  //end save
}

//callback notifying us of the need to save config
void saveConfigCallback () {
#ifdef DEBUG
  Serial.print(F("saving cfg"));
#endif
  shouldSaveConfig = true;
}
