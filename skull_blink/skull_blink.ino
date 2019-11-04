#include <avr/sleep.h>
#include <avr/wdt.h>

const uint8_t leftLedPin = 0;
const uint8_t rightLedPin = 1;
const uint8_t ldrPin = A1;

int ldrValue = 0;

int delayMillis = 1000;
int offRandomMillis = 400; //how long they stay off.

//uint8_t count = 0;

ISR(WDT_vect) {

}

void setup() {
  pinMode(leftLedPin, OUTPUT);
  pinMode(rightLedPin, OUTPUT);
  pinMode(ldrPin, INPUT);

  randomSeed(analogRead(0));

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  ldrValue = analogRead(ldrPin);

  digitalWrite(leftLedPin, HIGH);
  delay(500);
  digitalWrite(leftLedPin, LOW);
  delay(500);
  digitalWrite(rightLedPin, HIGH);
  delay(500);
  digitalWrite(rightLedPin, LOW);
  delay(500);

}

void loop() {

  ldrValue = analogRead(ldrPin);

  if (ldrValue > 40) {
    ADCSleep(true); // set the ADC to sleep
    setup_watchdog(9);
    sleep_mode();
    //waling up
    ADCSleep(false);
  } else {

    blinkFade();
    delayMillis = random(4000, 9000);
    offRandomMillis = random(200, 500);
  }
}

void blinkFade() {
  for (uint8_t i = 0 ; i < 255 ; i++) {
    analogWrite(leftLedPin, i);
    analogWrite(rightLedPin, i);
    delayMicroseconds(250);
  }

  delay(delayMillis);
  for (uint8_t i = 255 ; i > 0 ; i--) {
    analogWrite(leftLedPin, i);
    analogWrite(rightLedPin, i);
    delayMicroseconds(250);
  }
  digitalWrite(leftLedPin, LOW);
  digitalWrite(rightLedPin, LOW);
  delay(offRandomMillis);
}

void ADCSleep(boolean toSleep) {
  if (toSleep) {
    ADCSRA &= ~(1 << ADEN);
  } else {
    ADCSRA |= (1 << ADEN);
  }
}
//set the watchdog timer to wake us up but not reset
//0=16ms, 1=32ms, 2=64ms, 3=128ms, 4=250ms, 5=500ms, 6=1sec, 7=2sec, 8=4sec, 9=8sec
void setup_watchdog(int timerPrescaler) {


  if (timerPrescaler > 9 ) timerPrescaler = 9; //Correct incoming amount if need be

  byte bb = timerPrescaler & 7;
  if (timerPrescaler > 7) bb |= (1 << 5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1 << WDRF); //Clear the watch dog reset
  WDTCR |= (1 << WDCE) | (1 << WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int
}
