
#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>

#include "bitmap.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define HALL_SWITCH  3

U8G2_SSD1327_MIDAS_128X128_2_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// speedometer variables
boolean inMetric = true;
float start, elapsed;
float wheelCir = 2.09; //in meters
int speedDisplay;

//time
char timeBuffer[10];

//temperature
char tmpBuffer[] = {'4', '3', '.', '4', 'c'};

//odometer
unsigned long revolutionCount = 0;
float currentDistance;
unsigned int odoTotal = 0;

//delays
unsigned long mainScrRefInterval = 1000;// = 500 millisec
unsigned long mainScrPrevMillis = 0;//

boolean switchTest = true;

void setup(void) {
  //  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(HALL_SWITCH), speedInt, FALLING );

  pinMode(HALL_SWITCH, INPUT);

  u8g2.begin();
  u8g2.setContrast(255);

  start = millis();
}


void loop(void) {

  displayMainScreen();


}
void displayMainScreen() {

  if (millis() - mainScrPrevMillis > mainScrRefInterval) {

    detachInterrupt(digitalPinToInterrupt(HALL_SWITCH));

    mainScrPrevMillis = millis();
    timerCalculation();
    //    Serial.println(timeBuffer);
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_5x8_tr);
      //u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

      u8g2.drawXBMP( 110, 0, 16, 16, battery_bitmap);
      u8g2.setCursor(111, 21);
      u8g2.print(F("12%"));

      u8g2.drawXBMP( 0, 0, 16, 16, light_bitmap);
      u8g2.setCursor(50, 10); //temperature
      u8g2.print(tmpBuffer); //temperature

//      u8g2.setFont(u8g2_font_6x10_tr);

      u8g2.setCursor(25, 110);
      u8g2.print(F("Odo"));
      u8g2.setCursor(0, 120);
      u8g2.print(F("123.45/13323"));


      u8g2.setCursor(90, 110); //elapsed time
      u8g2.print(F("Time"));
      u8g2.setCursor(80, 120);
      u8g2.print(timeBuffer);

      u8g2.setCursor(105, 90);
      u8g2.print(F("KM/h"));

//      u8g2.setFont(u8g2_font_logisoso46_tr );
//  u8g2.setFont(u8g2_font_fub42_tr);
u8g2.setFont(u8g2_font_7Segments_26x42_mn);

      if (speedDisplay < 10) {
        u8g2.setCursor(50, 82);
      } else {
        u8g2.setCursor(30, 82);
      }
      u8g2.print(speedDisplay);

    } while ( u8g2.nextPage() );

    attachInterrupt(digitalPinToInterrupt(HALL_SWITCH), speedInt, FALLING );
  }
}

void speedInt() {
  switchTest = true;
  elapsed = millis() - start;
  start = millis();
  revolutionCount++;

  if (inMetric) {
    speedDisplay = (3600 * wheelCir) / elapsed;
    currentDistance = revolutionCount * wheelCir / 1000;
    //speedDisplay = (3600 * (wheelCir * .62137) ) / elapsed //in MPH
  }
  //        Serial.println(F("Display"));
  //    Serial.println(elapsed);
  //    Serial.println(start);
  //    Serial.println(revolutionCount);

  //    }
}
void timerCalculation() {
  unsigned long over;
  int runHours = int(millis() / 3600000);
  over = (millis() % 3600000);
  int runMinutes = int(over / 60000);
  over = over % 60000;
  int runSeconds = int(over / 1000);


  //
  //  Serial.println(runHours);
  //  Serial.println(runMinutes);
  //  Serial.println(runSeconds);
//
//    char bff[4];
//    if (runHours > 0) {
////      Serial.println("1");
//      itoa(bff, runHours, 10);
//      timeBuffer[0] = bff[0];
//      timeBuffer[1] = bff[1];
//    } else {
////      Serial.println("2");
//      timeBuffer[0] = '0';
//      timeBuffer[1] = '0';
//    }
//    timeBuffer[2] = ':';
////    Serial.println("3");
//    itoa(bff, runMinutes, 10);
//    if (runMinutes < 10) {
////      Serial.println("4");
//      timeBuffer[3] = '0';
//      timeBuffer[4] = bff[0];
//    } else {
////      Serial.println("5");
//      timeBuffer[3] = bff[0];
//      timeBuffer[4] = bff[1];
//    }
////     Serial.println("54");
//    timeBuffer[5] = ':';
////     Serial.println("55");
//    itoa(bff, runSeconds, 10);
//    if (runSeconds < 10) {
////      Serial.println("6");
//      timeBuffer[6] = '0';
//      timeBuffer[7] = bff[0];
//    } else {
////     Serial.println("522");
//      timeBuffer[6] = bff[0];
//      timeBuffer[7] = bff[1];
//    }
  //Serial.println("done");
  //  dtostrf(currentDistance, 7, 0, bff);

      sprintf(timeBuffer, "%02d:%02d:%02d", runHours, runMinutes, runSeconds); ///user a lot.. may if running out , replace
}

void distance() {

}

//
//char m_str[3];
//  strcpy(m_str, u8x8_u8toa(m, 2));    /* convert m to a string with two digits */
//  u8g2.firstPage();
//  do {
//    u8g2.setFont(u8g2_font_logisoso62_tn);
//    u8g2.drawStr(0,63,"9");
//    u8g2.drawStr(33,63,":");
//    u8g2.drawStr(50,63,m_str);
//
//     u8g2.drawXBMP( 10, 115, 16, 16, battery_bitmap);
//     u8g2.drawXBMP( 30, 115, 16, 16, light_bitmap);
//     u8g2.setFont(u8g2_font_5x8_mn);
//     u8g2.drawStr(30,107,"10%");
//
//
//
//  } while ( u8g2.nextPage() );
//  delay(100);
//  m++;
//  if ( m == 60 )
//    m = 0;
