  /*-----------------------------------------------------------------------
  
  Project: LOTNAtag Firearm (Standard Version)
  Version: 2.0
  Description: A basic laser tag system for Arduino, compatible with the Starlyte
  
  Copyright (c) 2018 David Rowley
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  
  ---------------------------------------------------------------------------*/
  
  #include <avr/interrupt.h>
  #include <avr/sleep.h>
  #include <avr/pgmspace.h>
  #include <Arduino.h>
  #include <pins_arduino.h>
  #include <TagTone.h>
  
  // audio
  #include "SoftwareSerial.h"
  #include "DFRobotDFPlayerMini.h"
  

  // output pins
  const int pulsePin = 5;
  const int muzzlePin = 6;
  const int redLedPin = 7;
  const int greenLedPin = 8;
  // 10 and 11 are used for software serial
  
  
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
  
  
  // behaviour variables
  const int startupDelayMs = 1500;
  const int reloadDurationMs = 5000;
  const int shotDuration = 250;

  
  
  const int mode_auto = HIGH;
  const int mode_single = LOW;  // note that single will be default behaviour if pin 4 is unconnected due to use of pullup
  const int maxShots = 30;

 // Sound files for each action should be in the following locations:

 // File Structure:

 // disk://01/001.wav = Startup
 // disk://02/001.wav = Fire
 // disk://03/001.wav = Reload
 // disk://04/001.wav = Out of Ammo
  
  // sound settings

  SoftwareSerial dfSerial(10, 11); // RX, TX
  DFRobotDFPlayerMini dfPlayer;
  
  // program behaviour values
  volatile boolean triggerPressed = false;
  volatile boolean reloadPressed = false;
  int remainingShots = maxShots;  

  
  // Program Code

  void setup() {
    // serial set up
    //Serial.begin(115200);
    
    // set up notification pins
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
       
    // set up fire pins
    pinMode(pulsePin, OUTPUT);   
    pinMode(muzzlePin, OUTPUT);
   
    // set up audio
    dfSerial.begin(9600);

    if(!dfPlayer.begin(dfSerial))
    {
      flashPin(redLedPin, 5, 2500);
    }
  
    dfPlayer.setTimeOut(500); //Set serial communication time out 500ms
    dfPlayer.volume(30);  //Set volume value (0~30).
    dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);
   
    // set up input pins
    pinMode(triggerPin, INPUT_PULLUP);
    pinMode(reloadPin, INPUT_PULLUP);
    pinMode(modePin, INPUT_PULLUP);
  
    carrierTone.beginCarrier(pulsePin);
    
    // fill the clip
    remainingShots = maxShots;
    
    playStartupSound();
    flashPin(greenLedPin, 5, 2500);
  }
  
  void loop() { 
    // wipe the interrupt flags so we don't get repeated presses from last time, or other spurious interrupts
    EIFR = 1;  // clear flag .for interrupt 0
    EIFR = 2;  // clear flag for interrupt 1
    
    // wait here until a button is pressed
    waitUntilInput();
    
    // now do the appropriate action  
  
    if(triggerPressed)
    {
      //pinMode(audioPin, OUTPUT);
      fireWeapon();
    }
    else if (reloadPressed)
    {
      //pinMode(audioPin, OUTPUT);
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
    // Detach interrupts ASAP!
    detachInterrupt(fireInt); 
    detachInterrupt(reloadInt);
  
    triggerPressed = true;
  }
  
  void setReload()
  {
    // Detach interrupts ASAP!
    detachInterrupt(reloadInt);
    detachInterrupt(fireInt);  
    
    reloadPressed = true;
  }
  
  void reloadWeapon()
  {
      flashPin(greenLedPin, redLedPin, 2.5, reloadDurationMs);  
      remainingShots = maxShots; 
      playReloadSound();   
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
        delay(100);
        notifyOutOfAmmo();
        return;
      }
      else
      {
        digitalWrite(muzzlePin, HIGH);
        carrierTone.playCarrier();
       
        delay(50);
        carrierTone.stop();
        playFireSound();
        
        digitalWrite(muzzlePin, LOW);

        delay(200);
        
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
  
  void notifyOutOfAmmo()
  {
    playOutOfAmmoSound();
    flashPin(redLedPin, 5, 2000);
  }
  

    
    
int flashPin(int pinId1, int pinId2, double frequency, int minDurationMs)
{
  double pulseDurationS = (1.0/ frequency) /2 ;
  int pulseDurationMs = 1000 * pulseDurationS;
  if(pulseDurationMs < 20)
     pulseDurationMs = 20;
  
  int timerMs = 0;
  
  do
  {
    digitalWrite(pinId1, HIGH);
    if(pinId2 != 0)
    {
      digitalWrite(pinId2, LOW);
    }
    
    delay(pulseDurationMs);
    digitalWrite(pinId1, LOW);
    if(pinId2 != 0)
    {
      digitalWrite(pinId2, HIGH);
    }
    delay(pulseDurationMs);
    
    timerMs += (pulseDurationMs *2);
  } while (timerMs < minDurationMs);
  
  digitalWrite(pinId1, LOW);
  if(pinId2 != 0)
  {
      digitalWrite(pinId2, LOW);
  }
    
  return timerMs;
}

int flashPin(int pinId, double frequency, int minDurationMs)
{
  return flashPin(pinId, 0, frequency, minDurationMs);
}

int flashPin(int pinId, double frequency)
{
  // note: always does at least one cycle.
  return flashPin(pinId, 0, frequency, 0);
}
  

void playStartupSound()
{
  dfPlayer.playFolder(1,1);
}

void playFireSound()
{
  dfPlayer.playFolder(2,1);
}

void playReloadSound()
{
  dfPlayer.playFolder(3,1);
}

void playOutOfAmmoSound()
{
  dfPlayer.playFolder(4,1);
}
  
  
  
  
  
  


