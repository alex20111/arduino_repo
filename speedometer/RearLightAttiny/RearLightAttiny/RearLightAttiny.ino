

#define LED1 1
#define LED2 2
#define LED3 3
#define LED4 4

unsigned long fastLedDelayPrevMillis = 0;
boolean fastLedOn = false;

unsigned long slowLedDelayPrevMillis = 0;
boolean slowLedOn = false;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {


  if (millis() - fastLedDelayPrevMillis > 100) {
    if (fastLedOn) {
      fastLedOn = false;

      digitalWrite(LED3, HIGH);   // turn the LED on (HIGH is the voltage level)
      digitalWrite(LED4, HIGH);   // turn the LED on (HIGH is the voltage level)
    } else {
      fastLedOn  = true;

      digitalWrite(LED3, LOW);    // turn the LED off by making the voltage LOW
      digitalWrite(LED4, LOW);    // turn the LED off by making the voltage LOW
    }
    fastLedDelayPrevMillis = millis();
  }
  if (millis() - slowLedDelayPrevMillis > 300) {
    if (slowLedOn) {
      slowLedOn = false;

      digitalWrite(LED1, HIGH);   // turn the LED on (HIGH is the voltage level)
      digitalWrite(LED2, HIGH);   // turn the LED on (HIGH is the voltage level)
//      slowLedDelayPrevMillis = millis();
    } else {
      slowLedOn  = true;

      digitalWrite(LED1, LOW);    // turn the LED off by making the voltage LOW
      digitalWrite(LED2, LOW);    // turn the LED off by making the voltage LOW
      
    }
    slowLedDelayPrevMillis = millis();
  }
}
