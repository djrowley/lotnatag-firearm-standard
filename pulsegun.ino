#include <Tone.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// output pins
const int carrierPin = 5;
const int signalPin = 6;

const int redLedPin = 7;
const int greenLedPin = 8;

const int audioPin = 10;


// input pins
const int triggerPin = 2;
const int reloadPin = 3;
// debounce settings
unsigned long lastInterruptTime = 0;



// interrupts
const int fireInt = 0;   // pin 2
const int reloadInt = 1;     // pin 3


// fire pulse settings
const int carrierHz = 57660;
const int signalHz = 1800;
Tone carrierTone;
Tone signalTone;



// program behaviour values
volatile boolean triggerPressed;
volatile boolean reloadPressed;

const int maxShots = 40;
int remainingShots;

void setup() {
  // set up fire pins
  pinMode(carrierPin, OUTPUT);
  pinMode(signalPin, OUTPUT);
  pinMode(audioPin, OUTPUT);
  carrierTone.begin(carrierPin);
  signalTone.begin(signalPin);
  carrierTone.play(carrierHz);
  

  
  
  // set up notification pins
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  

  // set up input pins
  pinMode(triggerPin, INPUT_PULLUP);
  pinMode(reloadPin, INPUT_PULLUP);
  
  attachInterrupt(reloadInt, setReload, FALLING);   // reload interrupt should be allowed from the start

  
  triggerPressed = false;
  reloadPressed = false;
  
  // fill the clip
  remainingShots = maxShots;
}

void loop() {
  triggerPressed = false;
  reloadPressed = false;
  
  // put your main code here, to run repeatedly: 
  waitUntilInput();
  
  if(triggerPressed)
  {
    fireWeapon();
  }
  else if (reloadPressed)
  {
    reloadWeapon();
  }

  triggerPressed = false;
  reloadPressed = false;
}

void waitUntilInput()
{
  set_sleep_mode(SLEEP_MODE_IDLE);           // can't use any other as we want to use falling state for interrupt, can only use IDLE at lower awareness levels
  sleep_enable();                            // allow sleep
  
  attachInterrupt(fireInt, setFire, HIGH);    // fire interrupt should be allowed from start
  
  sleep_cpu();

  // Re-enter here after interrupt
  sleep_disable();                           // unset the flag allowing cpu sleep    
  
  // make sure both detached
  detachInterrupt(fireInt);  
}

void setFire()
{
  detachInterrupt(fireInt);
  // debounce interrupt
  unsigned long currentTime = millis();
  if(currentTime - lastInterruptTime > 150)
  {
    triggerPressed = true;
  }
  lastInterruptTime = currentTime;
}

void setReload()
{
  unsigned long currentTime = millis();
  if(currentTime - lastInterruptTime > 150)
  {
    reloadPressed = true;
  }
  lastInterruptTime = currentTime;
}

/// Try and fire the weapon - if there is enough ammo left fires a tag pulse, otherwise flashes red led to notify
void fireWeapon()
{
  if(remainingShots <= 0)
  {
    notifyOutOfAmmo();
  }
  else
  {
    emitTagPulse();
    remainingShots --;
  }
}

void reloadWeapon()
{
  for(int i =0; i<8; i++)
  {
    digitalWrite(redLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
    delay(200);
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    delay(200);
  }
  
  remainingShots = maxShots;
  
  for(int i =0; i<3; i++)
  {
    digitalWrite(greenLedPin, HIGH);
    delay(100);
    digitalWrite(greenLedPin, LOW);
    delay(100);
  }
  reloadPressed = false;
}

void notifyOutOfAmmo()
{
  for(int i =0; i<5; i++)
  {
    digitalWrite(redLedPin, HIGH);
    delay(100);
    digitalWrite(redLedPin, LOW);
    delay(100);
  }
}

void emitTagPulse() {
  signalTone.play(signalHz, 45);
  digitalWrite(audioPin, HIGH);
  delay(200);
  digitalWrite(audioPin, LOW);

}
