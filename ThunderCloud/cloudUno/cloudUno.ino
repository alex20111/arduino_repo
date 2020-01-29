#include <LowPower.h>
#include <IRremote.h>
#include "SoftwareSerial.h"
#include <DFMiniMp3.h>
#include "FastLED.h"

#include <modeEnums.h> //POWER_ON, POWER_OFF, THUNDER, RED, YELLOW, BLUE, STARTAP, ESP_READY

//DEBUG--------------
//#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_FLUSH() Serial.flush()
#define DEBUG_PRINTHEX(x,z) Serial.println(x,z)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#define DEBUG_FLUSH()
#endif
//DEBUG--------------
boolean startRelaxMp3 = false;
//MP3 variables--------------------

class Mp3Notify
{
public:
  static void OnError(uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }

  static void OnPlayFinished(uint16_t globalTrack)
  {
    Serial.println();
    Serial.print("Play finished for #");
    Serial.println(globalTrack);   
     
  }

  static void OnCardOnline(uint16_t code)
  {
  }

  static void OnCardInserted(uint16_t code)
  {
  }

  static void OnCardRemoved(uint16_t code)
  {
  }
};
SoftwareSerial secondarySerial(10, 11); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(secondarySerial);

uint8_t  initMp3 = 1; //1=not init, 2=on, 3 = sleeping
boolean mp3Busy = false;
//MP3 variables end ---------------
//APA102 variables ----------------
#define NUM_LEDS 10
CRGB leds[NUM_LEDS];

// used to make basic mood lamp colour fading feature
int fade_h;
int fade_direction = 1;
//APA102 variables end ------------
//EPS comm---------------
SoftwareSerial espSerial(5, 4); //RX TX communication to arduino
boolean espAwake = false;
//ESP comm end-----------

uint8_t RECV_PIN = 2; //Remote pin
IRrecv irrecv(RECV_PIN);

decode_results results;

// Use pin 2 as wake up pin
const uint8_t wakeUpPin 	  = 2; //when arduino sleep
const uint8_t powerLed 		  = 6; //to show power
const uint8_t ESP_RESET_PIN   = 8;
const uint8_t FIVE_VOLT_POWER = 7;
const uint8_t MP3_POWER       = 9;

//sleep variables
long sleepInterval = 1000 * 10; //10 seconds pr IR
unsigned long prevSleepMillis = 0;

boolean powerOn = false;

Mode mode;
Mode commMode;  //communication mode to ESP
Mode lastSavedMode;

//vars for the lightningThunder Show
boolean initThunderPlay = true;
uint8_t countLoop       = 0;
uint8_t totalCountLoop  = 0;
uint8_t mp3ThunderSoundStart = 0;
unsigned long prevPlayMillis = 0;
uint8_t lightningMode = 0; //1 = lightinig strike, 2=rolling, 3=crack

//sleepEnabled
boolean sleepEnabled = false;
unsigned long prevCheckSleepMillis = 0;

boolean toRemove = true;
void setup()
{
    //espSerial
  espSerial.begin(9600);
  
#ifdef DEBUG
  Serial.begin(9600);
#endif

  FastLED.addLeds<APA102, A4, A5, RBG>(leds, NUM_LEDS);//.setCorrection(TypicalSMD5050);
  // FastLED.addLeds<APA102, GRB>(leds, NUM_LEDS); //use A5, A4 pins automatically

  irrecv.enableIRIn(); // Start the receiver

  // Configure wake up(Interrupt) pin as input.
  pinMode(wakeUpPin, INPUT);

  //power LED
  pinMode(powerLed, OUTPUT);
  pinMode(ESP_RESET_PIN, INPUT_PULLUP );
  pinMode(FIVE_VOLT_POWER, OUTPUT);
  pinMode(MP3_POWER, OUTPUT);

  digitalWrite(MP3_POWER, LOW);
  digitalWrite(FIVE_VOLT_POWER, LOW);
  digitalWrite(ESP_RESET_PIN, HIGH);

  DEBUG_PRINTLN(F("READY"));
}

void loop() {

  unsigned long currentMillis = millis();
  if (!powerOn) {
    if (currentMillis - prevSleepMillis >= sleepInterval) {
      DEBUG_PRINTLN(F("SLEEP"));
      DEBUG_FLUSH();
      // Allow wake up pin to trigger interrupt on low. TODO, try with HIGH
      attachInterrupt(0, wakeUp, LOW);

      // Enter power down state with ADC and BOD module disabled.
      // Wake up when wake up pin is low.
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

      // Disable external pin interrupt on wake up pin.
      detachInterrupt(0);

      // save the last time
      prevSleepMillis = millis();
    }

  } else if (powerOn) {
    startShow();
    checkMp3Event();
    
    if (sleepEnabled){
      checkSleep();  
    }
  }

  if (irrecv.decode(&results)) {
    DEBUG_PRINTHEX(results.value, HEX);

    //set default value
    startRelaxMp3 = false;
    if (results.value != 0xFFFFFFFF) {

      if (results.value == 0xFF629D && !powerOn) {
        powerTurnedOn();
      } else if (results.value == 0xFF629D && powerOn) {
        turnPowerOff();
      } else if (results.value == 0xFF30CF && powerOn) {
        mode  = THUNDER;
      } else if (results.value == 0xFF18E7 && powerOn) {
        mode  = THUNDER_RAIN;
      } else if (results.value == 0xFF7A85 && powerOn) {
        mode  = RED;
      } else if (results.value == 0xFF10EF && powerOn) {
        mode  = YELLOW;
      } else if (results.value == 0xFF38C7 && powerOn) {
        mode  = BLUE;
      } else if (results.value == 0xFF5AA5 && powerOn) {
        mode  = ALT_COLORS;
      } else if (results.value == 0xFF42BD && powerOn) {
        mode  = RELAX;
        startRelaxMp3 = true;
      } else if (results.value == 0xFFE21D && powerOn) {
        if (initMp3 == 2) {
          DEBUG_PRINTLN(F("Volume up"));
          mp3.increaseVolume();
        }
      } else if (results.value == 0xFFA25D && powerOn) {
        if (initMp3 == 2) {
          DEBUG_PRINTLN(F("Volume Down"));
          mp3.decreaseVolume();
        }
      }
      else if (results.value == 0xFF22DD && powerOn) {
        startApServer();
      }
      else if (results.value == 0xFFC23D && powerOn) { //STOP command
        mode = NONE;
        sleepMp3();
        reset();
      }else if (results.value == 0xFFB04F && powerOn){
        DEBUG_PRINTLN(F("sleep enabled"));
        sleepEnabled = true;
        prevCheckSleepMillis = millis();
        digitalWrite(powerLed, LOW); //ON
        delay(1500);
        digitalWrite(powerLed, HIGH); //ON
        
        
      }
    }
    irrecv.resume(); // Receive the next value
    delay(100);
    prevSleepMillis = millis(); // to start sleep timer.
  }
  //listen to ESP serial
  if (espSerial.available() > 0) {
    int evnt = espSerial.read();
    eventFromEsp(evnt);
  }
}

void turnPowerOff() {
  DEBUG_PRINTLN(F("Turn OFF: "));
  powerOn = false;
   sleepMp3();
  digitalWrite(FIVE_VOLT_POWER, LOW); //5 volt line off

  if (espAwake) {
    commMode = POWER_OFF;
    espSerial.write(commMode);
    espSerial.flush();
  }
  reset();
  digitalWrite(powerLed, LOW); //OFF
  mode = NONE;
  

}
void powerTurnedOn() {
  DEBUG_PRINTLN(F("Turn on: "));

  digitalWrite(FIVE_VOLT_POWER, HIGH);  //turn on LEDS
  digitalWrite(powerLed, HIGH);

  powerOn = true;

}
void startShow() {
  switch (mode) {
    case THUNDER:
      if (toRemove) {
        DEBUG_PRINTLN(F("Thunder"));
      }
      lightningThunderShow();
      toRemove = false;
      break;
    case THUNDER_RAIN:  //rain then timed thunder with lights
      break;
    case RED:
      sleepMp3();
      single_colour(0);
      break;
    case YELLOW:
      sleepMp3();
      single_colour(65);
      break;
    case BLUE:
      sleepMp3();
      single_colour(155);
      break;
    case ALT_COLORS:
      sleepMp3();
      colour_fade();
      break;
    case RELAX:

      if (startRelaxMp3) {
        startRelaxMp3 = false;
        DEBUG_PRINTLN(F("RELAX"));
        playRelaxMusic();
        toRemove = false;
      }
      colour_fade();
      break;
  }

}
void lightningThunderShow() {

  //lightningMode
  if (initThunderPlay) {
    //here initialize before so it will be timed
    initializeMp3();

    initThunderPlay = false;
    countLoop = 0;
    totalCountLoop = random(9, 11); //generate the total loop before stopping
    mp3ThunderSoundStart = random(6, totalCountLoop); //generate when the sound is going to start in the show
    lightningMode = random(2, 4); //generate lightning mode
    handleLightningMode();
    countLoop++;
  }
  else if (!initThunderPlay) {
    if (countLoop < totalCountLoop) {
      countLoop++;
      if (countLoop == mp3ThunderSoundStart) { //start sound
        playShortThunderBurst();
      }
      handleLightningMode();
    }
    else if (countLoop == totalCountLoop ) {
      prevPlayMillis = millis(); //start the counddownd to the next play
      countLoop = 120; //increment to go to the pause for the next iteration.
      reset();
    } else if (millis() - prevPlayMillis > random(16000, 22000)) {
      initThunderPlay = true; //reset and restart between 16 and 20 sec
    }
  }
}
void handleLightningMode() { // it will set the right method for the lightning
  DEBUG_PRINT(F("handleLightningMode: "));
  DEBUG_PRINTLN(lightningMode);
  if (lightningMode == 99) {
    //lightningStrike(random(NUM_LEDS));
  } else if (lightningMode == 2) {
    rolling();
  } else if (lightningMode == 3) {
    crack();
  }
}
void startApServer() {

  DEBUG_PRINT(F("Starting AP: "));
  DEBUG_PRINTLN(espAwake);

  boolean ready = true;

  //wake up ESP
  if (!espAwake) {
    wakeUpEsp();

    delay(500);
    DEBUG_PRINTLN(F("Sending power on"));
    commMode = POWER_ON;
    espSerial.write(commMode); //send power mode ON to esp
    espSerial.flush();

    //  wait for acknowledge from ESP to be ready(ESP_READY);
    int waitFor = 3000; //3 sec
    unsigned long previousMillis = millis();
    delay(100);
    while (true) {
      if (espSerial.available() > 0) {
        int evnt = espSerial.read();
        mode = evnt;
        DEBUG_PRINT(F("recivevd ack: "));
        DEBUG_PRINTLN(evnt);
        if (mode == ESP_READY) { //received acknowledgement from ESP.
          ready = true;
          espAwake = true;
          break;
        }
      }
      if (millis() - previousMillis >= waitFor)
      {
        for (int i = 0 ; i < 4; i++) {
          digitalWrite(powerLed, LOW); //ON
          delay(500);
          digitalWrite(powerLed, HIGH); //ON
          delay(500);
        }
        ready = false;
        break;
      }
    }
  }

  if (ready) {
    DEBUG_PRINT(F("esp rdy all awake"));
    commMode = STARTAP;
    espSerial.write(commMode);
    espSerial.flush();
  }
}
void eventFromEsp(int event) {
  //if verify if esp wants to sleep. Basically to power down everything
  DEBUG_PRINT(F("Event from ESP"));
  DEBUG_PRINTLN(event);

  if (event == AUTO_SHUTDOWN) {
    espAwake = false;
    turnPowerOff();
  } else if (event == ESP_SLEEPING) {
    espAwake = false;
  }
}
void wakeUpEsp() {
  pinMode( ESP_RESET_PIN, OUTPUT );
  digitalWrite( ESP_RESET_PIN, LOW );
  delay( 10 );
  digitalWrite( ESP_RESET_PIN, HIGH );
  pinMode( ESP_RESET_PIN, INPUT_PULLUP );
}
void single_colour(int H) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV( H, 255, 255);
  }
  //avoid flickr which occurs when FastLED.show() is called - only call if the colour changes
  if (lastSavedMode != mode) {
    FastLED.show();
    lastSavedMode = mode;
  }
  delay(50);
}

void colour_fade() {
  if (toRemove) {
    toRemove = false;
    DEBUG_PRINT(F("NUM_LEDS: "));
    DEBUG_PRINTLN(NUM_LEDS);
  }
  //mood mood lamp that cycles through colours
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV( fade_h, 255, 255);
  }
  if (fade_h > 254) {
    fade_direction = -1; //reverse once we get to 254
  }
  else if (fade_h < 0) {
    fade_direction = 1;
  }

  fade_h += fade_direction;
  FastLED.show();
  delay(100);
}
void crack() {
  //turn everything white briefly
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV( 0, 0, 255);
  }
  FastLED.show();
  delay(random(10, 150));
  reset();
}
void rolling() {
  // a simple method where we go through every LED with 1/10 chance
  // of being turned on, up to 10 times, with a random delay wbetween each time
  for (int r = 0; r < random(2, 10); r++) {
    //iterate through every LED
    for (int i = 0; i < NUM_LEDS; i++) {
      if (random(0, 100) > 90) {
        leds[i] = CHSV( 0, 0, 255);
      }
      else {
        //dont need reset as we're blacking out other LEDs her
        leds[i] = CHSV(0, 0, 0);
      }
    }
    FastLED.show();
    delay(random(5, 150));
    reset();
  }
}
// utility function to turn all the lights off.
void reset() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV( 0, 0, 0);
  }
  FastLED.show();
}
void initializeMp3() {
  DEBUG_PRINT(F("initializing mp3 "));
  DEBUG_PRINTLN(initMp3);
  if (initMp3 == 1) {
    digitalWrite(MP3_POWER, HIGH);  // enableMp3
    delay(1000);
    mp3.begin();

    mp3.setVolume(10);
    initMp3 = 2;
     DEBUG_PRINTLN(F("Mp3 Initialized"));
     delay(100);
  }

}
void sleepMp3() { //turn off mosfett
  if (initMp3 == 2) {
    digitalWrite(MP3_POWER, LOW); //  enableMp3
    initMp3 = 1;
  }
}
void checkMp3Event() { //turn off mosfett
  if (initMp3 == 2) {
     mp3.loop();
  }
}

void playShortThunderBurst() {
  DEBUG_PRINTLN(F("playShortThunderBurst"));

  initializeMp3();

  mp3.playFolderTrack(2, random(1, 4) );
}
void playThunderWithRain() { //thunder with timed lights.
  DEBUG_PRINTLN(F("playThunderWithRain"));
  initializeMp3();
  mp3.playFolderTrack(1, 1 );
}
void playRelaxMusic() { //thunder with timed lights.
  DEBUG_PRINTLN(F("playRelaxMusic"));
  initializeMp3();
  mp3.playFolderTrack(3, random(1, 5 ));
}


void checkSleep(){
   if (millis() - prevCheckSleepMillis >= 3600000) {//
      DEBUG_PRINT(F("sleeping auto"));
      prevCheckSleepMillis = millis();
      sleepEnabled = false;
      turnPowerOff();
   }
}

void wakeUp()
{
  // Just a handler for the pin interrupt.
}


