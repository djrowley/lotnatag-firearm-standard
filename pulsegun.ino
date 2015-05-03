#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <pins_arduino.h>
#include <TagTone.h>


// output pins
const int pulsePin = 5;
const int signalPin = 6;
const int redLedPin = 7;
const int greenLedPin = 8;

const int audioPin = 9;


// input pins
const int triggerPin = 2;
const int reloadPin = 3;
const int modePin = 4;


// debounce settings
unsigned long lastInterruptTime = 0;



// interrupts
const int fireInt = 0;   // pin 2
const int reloadInt = 1;     // pin 3


// fire pulse settings
const int carrierHz = 57600;
const int signalHz = 1800;
TagTone carrierTone;
TagTone signalTone;

// mode settings
const int mode_auto = HIGH; // note that auto will be default behaviour if pin 4 is unconnected due to use of pullup
const int mode_single = LOW;


// program behaviour values
volatile boolean triggerPressed;
volatile boolean reloadPressed;

const int maxShots = 40;
int remainingShots;

void setup() {

  // set up fire pins
  pinMode(pulsePin, OUTPUT);
  pinMode(signalPin, OUTPUT);
  pinMode(audioPin, OUTPUT);
  carrierTone.beginCarrier(pulsePin);
  signalTone.beginSignal(signalPin);
  
  // set up notification pins
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  
  digitalWrite(redLedPin, HIGH);
  
  // set up input pins
  pinMode(triggerPin, INPUT_PULLUP);
  pinMode(reloadPin, INPUT_PULLUP);
  pinMode(modePin, INPUT_PULLUP);
  
  triggerPressed = false;
  reloadPressed = false;
  
  // fill the clip
  remainingShots = maxShots;
  
  delay(1500);
  digitalWrite(redLedPin, LOW);
}

void loop() { 
  // wipe the interrupt flags so we don't get repeated presses from last time, or other spurious interrupts
  EIFR = 1;  // clear flag for interrupt 0
  EIFR = 2;  // clear flag for interrupt 1
  
  // wait here until a button is pressed
  waitUntilInput();
  
  // now do the appropriate action  

  if(triggerPressed)
  {
    fireWeapon();
  }
  else if (reloadPressed)
  {
    reloadWeapon();
  }

  // whatever we did, wipe the action flags
  triggerPressed = false;
  reloadPressed = false;
}

void waitUntilInput()
{ 
  set_sleep_mode(SLEEP_MODE_IDLE);           // can't use any other as we want to use falling state for interrupt, can only use IDLE at lower awareness levels
  sleep_enable();                            // allow sleep
  
  attachInterrupt(fireInt, setFire, FALLING);    // fire interrupt should be allowed from start
  attachInterrupt(reloadInt, setReload, FALLING);   // reload interrupt should be allowed from the start
  
  sleep_cpu();

  // Re-enter here after interrupt
  sleep_disable();                           // unset the flag allowing cpu sleep   
}

void setFire()
{
  // Detach fire interrupt ASAP!
  detachInterrupt(fireInt); 
  detachInterrupt(reloadInt);

  triggerPressed = true;
}

void setReload()
{
  // Detach reload interrupt ASAP!
  detachInterrupt(reloadInt);
  detachInterrupt(fireInt);  
  
  reloadPressed = true;
}

/// Try and fire the weapon - if there is enough ammo left fires a tag pulse, otherwise flashes red led to notify
void fireWeapon()
{
  bool continueFiring;
  do
  {
    continueFiring = false;
    
    if(remainingShots <= 0)
    {
      notifyOutOfAmmo();
    }
    else
    {
      emitTagPulse();
      //digitalWrite(audioPin, HIGH);
      delay(200);
      //digitalWrite(audioPin, LOW);
      remainingShots --;
    }
    
    int fireMode = digitalRead(modePin);
    if(fireMode == mode_auto)
    {  
      // check if triggerPin is LOW, if so fire again.
      int triggerState = digitalRead(triggerPin);
      if(triggerState == LOW)
      {
        continueFiring = true;
      }
    }
  } while (continueFiring);
  
  
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
    digitalWrite(signalPin, HIGH);
    carrierTone.playCarrier();
    
    delay(60);
    digitalWrite(signalPin, LOW);

    //digitalWrite(audioPin, HIGH);
    delay(200);

}











