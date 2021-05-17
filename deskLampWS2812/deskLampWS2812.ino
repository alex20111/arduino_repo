//TEENSY LC
#include <FastLED.h>
#define LED_PIN     17
#define NUM_LEDS    7
CRGB leds[NUM_LEDS];

//int SAMPLES = 6;

#define UP_ARROW_BTN 0
#define DOWN_ARROW_BTN  4
#define BRIGHTNESS_BTN  3
#define MODE_BTN  1
#define TOUCH_LED_PILOT 9

#define TOUCH_THRESHOLD 1200
#define BTN_BOUNCE  50

/////// btn timers and flags

boolean btnBrightnessStarted = false;
boolean btnBrightnessPressed = false;
unsigned long prevReadBrightness = 0;

boolean btnUpStarted = false;
boolean btnUpPressed = false;
unsigned long prevReadUp = 0;

boolean btnDownStarted = false;
boolean btnDownPressed = false;
unsigned long prevReadDown = 0;

boolean btnModeStarted = false;
boolean btnModePressed = false;
unsigned long prevReadMode = 0;

///////

int lightBrightness = 0;

int mode = 1; // 1 though 4

int modeOneColors = 0;
int rainbowSpeed = 0;


void setup() {
  pinMode(TOUCH_LED_PILOT, OUTPUT);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(lightBrightness);
  //  modeFunc();
  FastLED.show();
  Serial.begin(9600);

}
void loop() {

  if (isBtnBrightnessPressed()) {
//    Serial.println("Br");
    digitalWrite(TOUCH_LED_PILOT, HIGH);
    lightBrightness += 51;
    if (lightBrightness > 255) {
      lightBrightness = 0;
    }
//    Serial.println(lightBrightness);
    FastLED.setBrightness(lightBrightness);
    FastLED.show();
  }
  if (isBtnBrightnessReleased()) {
    digitalWrite(TOUCH_LED_PILOT, LOW);
  }

  if (isBtnUpPressed()) {
    digitalWrite(TOUCH_LED_PILOT, HIGH);
//    Serial.println("Up pressed");
    if (mode == 1 || mode == 2) {
      modeOneColors ++;
      if (modeOneColors > 7)
      {
        modeOneColors = 0;
      }
    } else if (mode == 3 || mode == 4) {
      rainbowSpeed += 12;
      if (rainbowSpeed > 60)
      {
        rainbowSpeed = 0;
      }
    }
  }
  if (isBtnUpReleased()) {
    digitalWrite(TOUCH_LED_PILOT, LOW);
//    Serial.println("Up Released");
  }

  if (isBtnDownPressed()) {
    digitalWrite(TOUCH_LED_PILOT, HIGH);
//    Serial.println("Down pressed");
    if (mode == 1 || mode == 2) {
      modeOneColors --;
      if (modeOneColors < 0)
      {
        modeOneColors = 7;
      }
    } else if (mode == 3 || mode == 4) {
      rainbowSpeed -= 12;
      if (rainbowSpeed < 0)
      {
        rainbowSpeed = 60;
      }
    }
  }
  if (isBtnDownReleased()) {
    digitalWrite(TOUCH_LED_PILOT, LOW);
//    Serial.println("Down Released");
  }

  if (isBtnModePressed()) {
    digitalWrite(TOUCH_LED_PILOT, HIGH);
//    Serial.print("Mode pressed: ");
    mode ++;
    if (mode > 4) {
      mode = 1;
    }
    Serial.println(mode);
  }
  if (isBtnModeReleased()) {
    digitalWrite(TOUCH_LED_PILOT, LOW);
//    Serial.println("Mode Released");
  }

  if (mode == 1 || mode == 2) {
    modeSelectColors();
  } else if (mode == 4 || mode == 3) {
    rainbow_wave(rainbowSpeed, 10);
    FastLED.show();
  }
}

//-----------------------//
//  Mode 1 colors //
//-----------------------//
void modeSelectColors() {

  int colors = CRGB::White;

  switch (modeOneColors) {
    case 0: colors = CRGB::White;   break;
    case 1: colors = CRGB::Blue;    break;
    case 2: colors = CRGB::Green;   break;
    case 3: colors = CRGB::DeepPink;    break;
    case 4: colors = CRGB::Purple;  break;
    case 5: colors = CRGB::Orange;  break;
    case 6: colors = CRGB::Red;     break;
    case 7: colors = CRGB::Yellow;  break;

    default: colors = CRGB::White;     break;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = colors;
  }

  FastLED.show();
}

//------------------------//
//    RAINBOW COLORs
//------------------------//
void rainbow_wave(uint8_t thisSpeed, uint8_t deltaHue) {     // The fill_rainbow call doesn't support brightness levels.

  // uint8_t thisHue = beatsin8(thisSpeed,0,255);                // A simple rainbow wave.
  uint8_t thisHue = beat8(thisSpeed, 255);                    // A simple rainbow march.

  fill_rainbow(leds, NUM_LEDS, thisHue, deltaHue);            // Use FastLED's fill_rainbow routine.

}



//---------------------
// Buttons definitions
//---------------------

boolean isBtnBrightnessPressed() {
  int btn  = touchRead(BRIGHTNESS_BTN);

  if (btn > TOUCH_THRESHOLD && !btnBrightnessStarted) {
    btnBrightnessStarted = true;
    prevReadBrightness = millis();
  } else  if (btn < TOUCH_THRESHOLD && btnBrightnessStarted) {
    btnBrightnessStarted = false;
  }
  if (btnBrightnessStarted) {
    if (millis() - prevReadBrightness > BTN_BOUNCE && !btnBrightnessPressed) {
      btnBrightnessPressed = true;
      return true;
    }
  }
  return false;
}
boolean isBtnBrightnessReleased() {
  int btn  = touchRead(BRIGHTNESS_BTN);
  if (btn < TOUCH_THRESHOLD && btnBrightnessPressed) {
    btnBrightnessPressed = false;
    btnBrightnessStarted = false;
    return true;
  }
  return false;
}
//BTN UP
boolean isBtnUpPressed() {
  int btn  = touchRead(UP_ARROW_BTN);

  if (btn > TOUCH_THRESHOLD && !btnUpStarted) {
    btnUpStarted = true;
    prevReadUp = millis();
  } else  if (btn < TOUCH_THRESHOLD && btnUpStarted) {
    btnUpStarted = false;
  }
  if (btnUpStarted) {
    if (millis() - prevReadUp > BTN_BOUNCE && !btnUpPressed) {
      btnUpPressed = true;
      return true;
    }
  }
  return false;
}
boolean isBtnUpReleased() {
  int btn  = touchRead(UP_ARROW_BTN);
  if (btn < TOUCH_THRESHOLD && btnUpPressed) {
    btnUpPressed = false;
    btnUpStarted = false;
    return true;
  }
  return false;
}

//BTN Down
boolean isBtnDownPressed() {
  int btn  = touchRead(DOWN_ARROW_BTN);

  if (btn > TOUCH_THRESHOLD && !btnDownStarted) {
    btnDownStarted = true;
    prevReadDown = millis();
  } else  if (btn < TOUCH_THRESHOLD && btnDownStarted) {
    btnDownStarted = false;
  }
  if (btnDownStarted) {
    if (millis() - prevReadDown > BTN_BOUNCE && !btnDownPressed) {
      btnDownPressed = true;
      return true;
    }
  }
  return false;
}
boolean isBtnDownReleased() {
  int btn  = touchRead(DOWN_ARROW_BTN);
  if (btn < TOUCH_THRESHOLD && btnDownPressed) {
    btnDownPressed = false;
    btnDownStarted = false;
    return true;
  }
  return false;
}

//BTN Mode
boolean isBtnModePressed() {
  int btn  = touchRead(MODE_BTN);

  if (btn > TOUCH_THRESHOLD && !btnModeStarted) {
    btnModeStarted = true;
    prevReadMode = millis();
  } else  if (btn < TOUCH_THRESHOLD && btnModeStarted) {
    btnModeStarted = false;
  }
  if (btnModeStarted) {
    if (millis() - prevReadMode > BTN_BOUNCE && !btnModePressed) {
      btnModePressed = true;
      return true;
    }
  }
  return false;
}
boolean isBtnModeReleased() {
  int btn  = touchRead(MODE_BTN);
  if (btn < TOUCH_THRESHOLD && btnModePressed) {
    btnModePressed = false;
    btnModeStarted = false;
    return true;
  }
  return false;
}

//------------------------
// Buttons definitions END
//------------------------
