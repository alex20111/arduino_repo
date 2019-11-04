#include <avr/sleep.h>
#include <avr/wdt.h>


ISR(WDT_vect){
  //put something when waking up
  wdt_disable(); 
}

void setup() {
  pinMode(3, OUTPUT);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  digitalWrite(3, HIGH);
  delay(2000);
  digitalWrite(3, LOW);
  delay(500);

}

void loop() {

  ADCSleep(true); // set the ADC to sleep
  setup_watchdog(8);
  sleep_mode();
  //waling up
  ADCSleep(false);

  //blink twice
  for(int i = 0 ; i < 3 ; i++){
  digitalWrite(3, HIGH);
  delay(500);
  digitalWrite(3, LOW);
  delay(500);
  }

}

void ADCSleep(boolean toSleep){
  if (toSleep){
    ADCSRA &= ~(1<<ADEN);
  }else{
    ADCSRA |= (1<<ADEN);
  }
}
//set the watchdog timer to wake us up but not reset
//0=16ms, 1=32ms, 2=64ms, 3=128ms, 4=250ms, 5=500ms, 6=1sec, 7=2sec, 8=4sec, 9=8sec
void setup_watchdog(int timerPrescaler){


  if (timerPrescaler > 9 ) timerPrescaler = 9; //Correct incoming amount if need be

  byte bb = timerPrescaler & 7; 
  if (timerPrescaler > 7) bb |= (1<<5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1<<WDRF); //Clear the watch dog reset
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int
}
