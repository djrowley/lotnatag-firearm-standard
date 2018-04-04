  
  /*-----------------------------------------------------------------------
  
  Project: LOTNAtag Firearm (Standard Version)
  Version: 1.1
  Description: A basic laser tag system for Arduino, compatible with the Starlyte
  
  Copyright (c) 2016 David Rowley
  
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
  #include <SD.h>
  #include <SPI.h>
  #define SD_ChipSelectPin 10
  #include <TMRpcm.h>
  
  TMRpcm tmrpcm;
  
  // output pins
  const int pulsePin = 5;
  const int signalPin = 6;
  const int redLedPin = 7;
  const int greenLedPin = 8;
  
  // sd pins for reference
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
  
  
  // behaviour variables
  const int mode_auto = LOW;
  const int mode_single = HIGH;  // note that single will be default behaviour if pin 4 is unconnected due to use of pullup
  
  const int startupDelayMs = 1500;
  const int reloadDurationMs = 5000;
  const int shotDuration = 250;
  const int maxShots = 16;

 
  
  // sound names
  
  char* soundStartup       = "ready.wav";
  char* soundFire          = "p90shot.wav";//"fire_a.wav";
  char* soundReloadStart   = "rld_long.wav";
  char* soundReloadEnd     = "rld_end.wav";
  char* soundOutOfAmmo     = "fire_ooa.wav";


  // program behaviour values
  volatile boolean triggerPressed = false;
  volatile boolean reloadPressed = false;
  int remainingShots = maxShots;  

  
  // Program Code

  void setup() {
    // set up notification pins
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
       
    // set up fire pins
    pinMode(pulsePin, OUTPUT);   
    pinMode(signalPin, OUTPUT);
   
    // set up speaker
    pinMode(audioPin, OUTPUT);
    tmrpcm.speakerPin = 9;
    tmrpcm.setVolume(5);
    tmrpcm.quality(1);
  
    // set up input pins
    pinMode(triggerPin, INPUT_PULLUP);
    pinMode(reloadPin, INPUT_PULLUP);
    pinMode(modePin, INPUT_PULLUP);
  
    carrierTone.beginCarrier(pulsePin);
    
    // fill the clip
    remainingShots = maxShots;
    
    bool sdReadSuccess = true;
    
    if(!SD.begin(SD_ChipSelectPin))
    {
      sdReadSuccess = false;
      flashPin(redLedPin, 5, 2500);
    }
    else
    {
      tmrpcm.play(soundStartup);
      flashPin(greenLedPin, 5, 2500);
    }
  }
  
  void loop() { 
    // wipe the interrupt flags so we don't get repeated presses from last time, or other spurious interrupts
    EIFR = 1;  // clear flag .for interrupt 0
    EIFR = 2;  // clear flag for interrupt 1
    
    if(tmrpcm.isPlaying())
    {
      // continue
    }
    else
    {
      
      tmrpcm.disable();
      pinMode(audioPin, INPUT);
      // wait here until a button is pressed
      waitUntilInput();
    }
    
    // now do the appropriate action  
  
    if(triggerPressed)
    {
      tmrpcm.disable();
      pinMode(audioPin, OUTPUT);
      fireWeapon();
    }
    else if (reloadPressed)
    {
      tmrpcm.disable();
      pinMode(audioPin, OUTPUT);
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
      tmrpcm.play(soundReloadStart);
      flashPin(greenLedPin, redLedPin, 2.5, reloadDurationMs);

        
        //if(tmrpcm.isPlaying() != 1)
        //{
        //  // stop playing sound after 800ms
        //  tmrpcm.disable();
        //  pinMode(audioPin, INPUT);
        //}
      
      
      //pinMode(audioPin, OUTPUT);
      
      remainingShots = maxShots; 
      tmrpcm.play(soundReloadEnd);      
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
        return;
      }
      else
      {
        tmrpcm.disable();
        digitalWrite(signalPin, HIGH);
        carrierTone.playCarrier();
       
        delay(50);
        carrierTone.stop();
        tmrpcm.play(soundFire);
        digitalWrite(signalPin, LOW);

        delay(80); // 50
      
        
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
          tmrpcm.disable();
          digitalWrite(signalPin, HIGH);
          delay(50);
          tmrpcm.play(soundFire);
          digitalWrite(signalPin, LOW);

          delay(80);
        }
      }
    } while (continueFiring);
    
  }
  
  void notifyOutOfAmmo()
  {
    tmrpcm.play(soundOutOfAmmo);
    do
    {
      flashPin(redLedPin, 5);
    }while(tmrpcm.isPlaying());
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
  
  
  
  
  
  
  
  
  
  


