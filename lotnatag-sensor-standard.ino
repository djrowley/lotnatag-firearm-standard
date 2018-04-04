/*-----------------------------------------------------------------------
  
  Project: LOTNAtag Sensor 
  Version: 1.2
  Description: A basic laser tag sensor system for Arduino, compatible with the Starlyte
  
  Copyright (c) 2017 David Rowley
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  
  ---------------------------------------------------------------------------*/

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include <Arduino.h>
#include <pins_arduino.h>

//========================================================================================================
// CONFIGURATION SETTINGS
//========================================================================================================

// Change these to balance the sensor

const byte config_npc_mode_max_hits_upper = 21;         //maximum number of hitpoints when in NPC mode
const byte config_player_mode_max_hits_upper = 21;       //maximum number of hitpoints when in player mode
const byte config_max_hits_lower = 1;                   //minimum number of hitpoints a player can set 

const unsigned int deathTonePeriodSecs = 60;            // how long the sensor alerts for after being taken down (seconds)
const unsigned int deathBleedoutTimerSecs = 900;        // how long does the bleedout time last for (seconds)

const unsigned int startupTimerMilliseconds = 5000;     // how long does the startup timer give you before locking in settings

const unsigned int gracePeriodBetweenDetections = 1500; // how long does sensor take to recycle between hits


// COLOUR SETTINGS
const bool rgbLed = true;  // change to false if on legacy device with only one LED

const byte hitDetectR = 255;
const byte hitDetectG = 0;
const byte hitDetectB = 0;

const byte hitDetectShieldsR = 0;
const byte hitDetectShieldsG = 0;
const byte hitDetectShieldsB = 255;

const byte reportHpR = 255;
const byte reportHpG = 0;
const byte reportHpB = 0;

const byte reportHpShieldsR = 0;
const byte reportHpShieldsG = 0;
const byte reportHpShieldsB = 255;

const byte startupR = 255;
const byte startupG = 255;
const byte startupB = 255;

const byte regenR = 0;
const byte regenG = 255;
const byte regenB = 0;

const byte rechargeR = 0;
const byte rechargeG = 0;
const byte rechargeB = 255;


// SHIELD SETTINGS

bool shieldsEnabled = true;

const unsigned int shieldHitpoints = 3;           // how many hitpoints does the shield hold
const unsigned int shieldHealFrequencySecs = 3;   // how rapidly should regeneration happen (every X seconds)
const unsigned int shieldRecoveryPeriodSecs = 10; // how many seconds need to elapse before regeneration can start
const unsigned int shieldHealAmount = 1;          // how many hp are regenerated on each regen pulse (0 for 'to full')


// REGENERATION SETTINGS

bool regenerationEnabled = true;

const unsigned int regenerationHealFrequencySecs = 3;   // how rapidly should regeneration happen (every X seconds)
const unsigned int regenerationRecoveryPeriodSecs = 10; // how many seconds need to elapse before regeneration can start
const unsigned int regenerationHealAmount = 1;          // how many hp are regenerated on each regen pulse (0 for 'to full')



//========================================================================================================
// PIN SETTINGS
//========================================================================================================

/*
 *      2  - INPUT used for the sensor button.  Normally HIGH through pullup, goes LOW when pressed (i.e. trigger on FALLING interrupt)
 *      3  - INPUT used for the Infrared detector chip.  56khz carrier is removed by chip - expect 
 *      4  - unused - TODO DEVICE CHAINING 
 *      5  - OUTPUT used for buzzer/speaker
 *      6  - OUTPUT - OPTIONAL - used if not in RGB mode for single output LED
 *      7  - unused
 *      8  - unused
 *      9  - OUTPUT - OPTIONAL - PWM output for RED LED
 *      10 - OUTPUT - OPTIONAL - PWM output for GREEN LED
 *      11 - OUTPUT - OPTIONAL - PWM output for BLUE LED
 *      12 - unused
 *      13 - unused
 */


const int buttonPin = 2;  // INPUT
const int signalPin = 3;  // INPUT
const int buzzerPin = 5;  // OUTPUT
const int oneLedPin = 6;  // OUTPUT

const int pwmRedLedPin = 9;  // OUTPUT
const int pwmGrnLedPin = 10;  // OUTPUT
const int pwmBluLedPin = 6;  // OUTPUT - cannot use 11 as interferes with TONE function


//========================================================================================================
// INTERRUPT CONFIGURATION
//========================================================================================================
const int buttonInt = 0;  // ie pin 2
const int signalInt = 1;  // ie pin 3

bool buttonPressed = false;
bool beenShot = false;
bool clockTick = false;

//========================================================================================================
// SYSTEM VALUES
//========================================================================================================

// hit point setting memory locations
const int max_hits_address = 0;
const int mode_address = 16;

// current settings of device
byte current_max_hits = 0;
byte current_hits = 0;
byte current_shield_hits = 0;
bool npcMode = false;

// used for clock tracking of events
unsigned long systemClockSeconds = 0;
unsigned long time_lastShotReceived = 0;
unsigned long time_lastShotReceivedOnShields = 0;
unsigned long time_enteredDyingState = 0;

// used during startup config period
unsigned long startupEndTime;
unsigned long buttonHeldDuration = 0;

// used for shot detection
unsigned long lastDetectionTime = 0;
int detectionTracker = 0;



String logMessage = "";

// SYSTEM STATE

int systemState = 0;
const int STATE_STARTUP = 0;
const int STATE_ACTIVE = 1;
const int STATE_DYING = 2;
const int STATE_DEAD = 3;



// ----------------------------------------------------------------------------------------------------------------
// STARTUP
// ----------------------------------------------------------------------------------------------------------------
/*
 * This code runs when the device is first turned on.  It sets up all the pins to the correct states,
 * loads the previous hit point settings from memory and enables the button for 'set hit points'
 */
void setup() {
  Serial.begin(9600);
  Serial.println("Sensor initialisation");
  // set variables
  pinMode(buzzerPin, OUTPUT);
  
  if(rgbLed)
  {
    pinMode(pwmRedLedPin, OUTPUT);
    pinMode(pwmGrnLedPin, OUTPUT);
    pinMode(pwmBluLedPin, OUTPUT);
  }
  else
  {
    pinMode(oneLedPin, OUTPUT);
  }
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(signalPin, INPUT_PULLUP);
  
  //disable led
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  initialiseOneSecondClock();
  
  // load mode first!
  loadModeFromMemory();
  
  if(npcMode)
    playNpcModeEnter();

  current_hits = getMaxHitpoints();
  
  logMessage = "Initial max hits (from memory): ";
  logMessage = logMessage + current_hits;
  Serial.println(logMessage);
  
  // flash leds and sound buzzer for number of hits
  reportCurrentHits();
  
  // start timer for system startup
  startupEndTime = millis() + startupTimerMilliseconds;
  buttonPressed = false;
  
  // set interrupt
  EIFR = 1;
  EIFR = 2;
  
  // ENable the button for 'decrease max hit points'
  attachInterrupt(buttonInt, buttonInterrupt, FALLING); // button interrupt should be enabled early
  
  //enter timer - if button is not pressed, will end after startupTimerMilliseconds ms.
  // if button is pressed, resets end of timer time to startupTimerMilliseconds ms after button pressed. 
  do
  {
    EIFR = 1;
    
    // button pressed is set by interrupt when the button is pressed (duh).
    if (buttonPressed)
    {
      
      // record how long the button was held for (up to 5 seconds)
      buttonHeldDuration = 0;
      do
      {
        tone(buzzerPin,523);
        delay(10);
        buttonHeldDuration +=10;
      } while (digitalRead(buttonPin) == LOW && buttonHeldDuration <= 5000);
      noTone(buzzerPin);
      
      // log press duration
      logMessage = "Button held duration: ";
      logMessage = logMessage + buttonHeldDuration;
      Serial.println(logMessage);
      
      // more than 5 seconds - switch between player mode and NPC mode
      if (buttonHeldDuration > 5000)
      {
        setNpcMode(!npcMode, false);
      }
      else if (buttonHeldDuration > 1000)
      {
        incrementMaxHits();
      }
      else
      {
        // press was not a bounce, and was for less than a second - decrement
        decrementMaxHits();
      }
      
      reportCurrentHits();
        
      delay(200);
      
      startupEndTime = millis() + startupTimerMilliseconds;
      
      logMessage = "Max hits changed to ";
      logMessage = logMessage + current_hits;
      Serial.println(logMessage);     
      
      // record that button is no longer pressed.
      buttonPressed = false;
    }
    else
    {
      delay(100);
    }
    
      
  } while (millis() < startupEndTime);
  // wait until count expires

  saveState();
  Serial.println("State saved to memory");

  playSystemStartup();

  logMessage = "Final hit selection: ";
  logMessage = logMessage + current_hits;
  Serial.println(logMessage);

 
  // set sensor interrupt;
  attachInterrupt(signalInt, signalInterrupt, FALLING); // shot detection interrupt only allowed after setup
  
  systemState = STATE_ACTIVE;
  Serial.println("Shot detection online");
  Serial.println("SENSOR READY");
  Serial.println("------------------------------------------------");
  
}


// ----------------------------------------------------------------------------------------------------------------
// MAIN PROGRAM
// ----------------------------------------------------------------------------------------------------------------

void loop() {

  //clear any pending interrupts lurking in the buffers (likely due to switch bounce)
  EIFR = 1;
  EIFR = 2;
      
  if(beenShot)
    shotDetected();
  else if(buttonPressed)
    buttonPushed();
  else if(clockTick)
    doTimerAction();

  // wait here until an interrupt occurs
  waitUntilInput();
}


// ----------------------------------------------------------------------------------------------------------------
// ON BEING SHOT...
// ----------------------------------------------------------------------------------------------------------------

// decrement hit points by one, report to player, and put into downed state if needed
void shotDetected()
{
    Serial.println("** HIT RECEIVED **");

    unsigned long internalGracePeriodClock = millis();
   
    bool stillAlive = removeHitpoint();

    if(stillAlive)
    {
      do
      {
        delay(10); 
      } while (millis() < (internalGracePeriodClock + gracePeriodBetweenDetections));
      EIFR = 2; // ignore shots during this time
    }

    logMessage = "2khz Pulse Count = ";
    logMessage = logMessage + detectionTracker;
    Serial.println(logMessage);
    
    beenShot = false;
}

// removes a hitpoint from the player, returns true if still alive
bool removeHitpoint()
{
  
  if(shieldsEnabled && current_shield_hits > 0)
  {
    current_shield_hits--;

    time_lastShotReceivedOnShields = systemClockSeconds;

    logMessage = "Shields remaining: ";
    logMessage = logMessage + String(current_shield_hits);
    Serial.println(logMessage);

    ledOn(hitDetectShieldsR,hitDetectShieldsG,hitDetectShieldsB);
    playSensorHitOnShields();
    ledOff();
  }
  
  else if(current_hits > 0)
  {
    current_hits --;

    time_lastShotReceived = systemClockSeconds;
    time_lastShotReceivedOnShields = systemClockSeconds; // don't want shields to recover whilst still getting shot
    
    logMessage = "Hits remaining: ";
    logMessage = logMessage + String(current_hits);
    Serial.println(logMessage);

    ledOn(hitDetectR,hitDetectG,hitDetectB);
    playSensorHit();
    ledOff();
  }

   if(current_hits <= 0)
   {
      // Srt up death systems
      systemState = STATE_DYING;
      time_enteredDyingState = time_lastShotReceived;
      detachInterrupt(signalInt);
    
      Serial.println(" ****** BLEEDING OUT!  ****** ");
      logMessage = "Death in ";
      logMessage = logMessage + secondsUntilDeath();
      Serial.println(logMessage);
    
      tone(buzzerPin, 432, 5000);
      return false;
    }
    return true;
}

// adds a hit point to the player (i.e. healing)
bool addHitpoint()
{
  if(current_hits < current_max_hits)
    current_hits ++;
    
  logMessage = "Hits remaining: ";
  logMessage = logMessage + String(current_hits);
  Serial.println(logMessage);
  
  return true;
}

// adds a hit point to the player's shields (i.e. recharge)
bool addShieldHitpoint()
{
  if(current_shield_hits < shieldHitpoints)
    current_shield_hits ++;
    
  logMessage = "Shield Hits remaining: ";
  logMessage = logMessage + String(current_shield_hits);
  Serial.println(logMessage);
  
  return true;
}

// ----------------------------------------------------------------------------------------------------------------
// ON BUTTON PRESSED...
// ----------------------------------------------------------------------------------------------------------------

void buttonPushed()
{
  // check current state
  switch(systemState)
  {
    case STATE_STARTUP:
          break;
    case STATE_ACTIVE:
          reportCurrentHits();
          break;
    case STATE_DYING:
          reportBleedoutStatus();
          break;
    case STATE_DEAD:
          // nothing
          break;
  }
  buttonPressed = false;
}

void reportCurrentHits()
{
  logMessage = "Current hits: ";
  logMessage = logMessage + current_hits;
  Serial.println(logMessage);
  for(int i = 0;i < current_hits; i++)
  {
      tone(buzzerPin,784+(100*(i%3)), 75);
      ledOn(reportHpR,reportHpG,reportHpB);
      delay(75);
      ledOff();
      delay(75);
      
      if((i+1)%3 == 0 || i == current_hits - 1)
        delay(150);
  }
}

void reportBleedoutStatus()
{ 
    logMessage = "Reporting Death Time";
    Serial.println(logMessage);
    
    if(secondsUntilDeath() <= 30)
    {
      // more urgent warning
      for(int j=0; j<5;j++)
      {
         for(int i =0; i<151;i+=10)
         {
           tone(buzzerPin,432+(10*i),15);
           delay(20);
         }
       }
    }
    else
    {
      logMessage = "Minutes remaining: ";
      logMessage = logMessage + getMinutesRemainingToBleedout();
      Serial.println(logMessage);
      for (int i = 0; i< getMinutesRemainingToBleedout(); i++)
      {
        tone(buzzerPin, 432, 150);
        delay(300);
              
        if((i+1)%3 == 0 || i == getMinutesRemainingToBleedout() - 1)
          delay(250);
      }
    }
}

 int getMinutesRemainingToBleedout()
 {
    int minutesRemaining = (secondsUntilDeath() / 60);
    if(secondsUntilDeath() % 60 > 0)
        minutesRemaining ++;

    return minutesRemaining;
 }



// ----------------------------------------------------------------------------------------------------------------
// ON TIMER EVENT...
// ----------------------------------------------------------------------------------------------------------------

void doTimerAction()
{
  switch(systemState)
  {
    case STATE_STARTUP:
          break;
    case STATE_ACTIVE:
          if(shieldsEnabled)
            rechargeShields();

          if(regenerationEnabled)
            regenerate();
          
          break;
    case STATE_DYING:
            // alternate LEDs
            digitalWrite(oneLedPin, digitalRead(oneLedPin) ^ 1);

            // if we are now dead, prepare to DIE!
            if(secondsUntilDeath() <= 0)
                systemState = STATE_DEAD;

            // if we've just hit a minute milestone, report it.
            else if(secondsUntilDeath() % 60 == 0)
                reportBleedoutStatus();

            // if we are in the first notification period of death...
            else 
            {
              if (secondsUntilDeath() > deathBleedoutTimerSecs - deathTonePeriodSecs)
              {
                // play alternating tone two seconds on two seconds off
                if(secondsUntilDeath() % 4 == 0)
                  tone(buzzerPin, 432, 2000);
              }
              logMessage = "Death in: ";
              logMessage = logMessage + secondsUntilDeath();
              Serial.println(logMessage);
            }
            break;
    case STATE_DEAD:
          dieHorribly();
          break;
  }

  clockTick = false;
}

void regenerate()
{
    //check regeneration enabled
    if(!regenerationEnabled)
      return; //nope
  
    // if injured
    if(current_hits < current_max_hits)
    {
      // how long until we start
      int timeUntilRegeneration = time_lastShotReceived + regenerationRecoveryPeriodSecs - systemClockSeconds; 
      // how long have we been healing
      
      if(timeUntilRegeneration > 0)
      {
          logMessage = "Regeneration begins in: ";
          logMessage = logMessage + timeUntilRegeneration;
          Serial.println(logMessage);
      }
      else 
      {
          // how long have we been healing
          int timeSinceRegenerationStarted = systemClockSeconds - (time_lastShotReceived + regenerationRecoveryPeriodSecs);
          // regenerate every X seconds
          if (timeSinceRegenerationStarted % regenerationHealFrequencySecs == 0)
          {
            addHitpoint();
            ledOn(regenR,regenG,regenB);
            playHealReceived();
            ledOff();
          }
      }           
    }
}

void rechargeShields()
{
    //check regeneration enabled
    if(!shieldsEnabled)
      return; //nope

    // if injured
    if(current_shield_hits < shieldHitpoints)
    {
      // how long until we start
      int timeUntilRecharge = time_lastShotReceivedOnShields + shieldRecoveryPeriodSecs - systemClockSeconds; 
      // how long have we been healing
      
      if(timeUntilRecharge > 0)
      {
          logMessage = "Shield Recharge begins in: ";
          logMessage = logMessage + timeUntilRecharge;
          Serial.println(logMessage);
      }
      else 
      {
          // how long have we been healing
          int timeSinceRechargeStarted = systemClockSeconds - (time_lastShotReceivedOnShields + shieldRecoveryPeriodSecs);
          // regenerate every X seconds
          if (timeSinceRechargeStarted % shieldHealFrequencySecs == 0)
          {
            addShieldHitpoint();
            ledOn(rechargeR,rechargeG,rechargeB);
            playShieldRechargeReceived();
            ledOff();
          }
      }           
    }

    delay(100);
  
    
}

// player has bled out and is now DEAD.
void dieHorribly()
{
    logMessage = "YOU ARE DEAD!";
    Serial.println(logMessage);
        
    detachInterrupt(buttonInt); 

    tone(buzzerPin,432);
    do 
    {
      // this will never end...
      delay(60000);
    } while(true);
}

int secondsUntilDeath()
{
  return time_enteredDyingState + deathBleedoutTimerSecs - systemClockSeconds;
}



// ----------------------------------------------------------------------------------------------------------------
// STARTUP BEHAVIOUR
// ----------------------------------------------------------------------------------------------------------------


void setNpcMode(bool set_npc_mode, bool suppressAlert)
{
  if(set_npc_mode)
  {
    logMessage = "NPC MODE!";
    current_hits = config_npc_mode_max_hits_upper;
    current_max_hits = config_npc_mode_max_hits_upper;
    npcMode = true;
    
    if(!suppressAlert)
      playNpcModeEnter();
  }
  else
  {
    logMessage = "PLAYER MODE!";
    current_hits = config_player_mode_max_hits_upper;
    current_max_hits = config_player_mode_max_hits_upper;
    npcMode = false;
    
    if(!suppressAlert)
      playPlayerModeEnter();
  }
  Serial.println(logMessage);
}


bool maxHitpointsInitialisedFromMemory = false;
int getMaxHitpoints()
{
  //if we have a number, we know it's already been set
  if (current_max_hits > 0)
    return current_max_hits;

  // otherwise load previous from memory
  int maxHitpoints = 0;
  if(!maxHitpointsInitialisedFromMemory)
  {
    // read from EEPROM
    maxHitpoints = EEPROM.read(max_hits_address);
    maxHitpointsInitialisedFromMemory = true;
  }

  // sanity check

  // value not below minimum
  if(maxHitpoints < config_max_hits_lower)
    maxHitpoints = config_max_hits_lower;
  
  // check maximum hits for current mode:
  int possibleMaxHitpointsForMode = npcMode ? config_npc_mode_max_hits_upper : config_player_mode_max_hits_upper;
    
  if(maxHitpoints > possibleMaxHitpointsForMode)
    maxHitpoints = possibleMaxHitpointsForMode;
  
  current_max_hits = maxHitpoints;

  return current_max_hits;
}

void loadModeFromMemory()
{
  // read from EEPROM
  unsigned long npc_mode_store = EEPROM.read(mode_address);
  
  logMessage = "Mode from memory: ";
  logMessage = logMessage + npc_mode_store;
  Serial.println(logMessage);
  
  if(npc_mode_store == 1)
    npcMode = true;
  else
    npcMode = false;
  
  setNpcMode(npcMode, true);
}

void saveState()
{
  saveMaxHitsToMemory();
  saveModeToMemory();
}


void saveMaxHitsToMemory()
{
  // write to permanent
  EEPROM.write(max_hits_address, current_max_hits);
}

void saveModeToMemory()
{
  unsigned long npc_mode_store = 0;
  
  if(npcMode)
    npc_mode_store = 1;
  
  // write to permanent
  EEPROM.write(mode_address, npc_mode_store);
}

void decrementMaxHits (){
    // if not at lower bound, decrement
    unsigned long config_max_hits_upper = npcMode ? config_npc_mode_max_hits_upper : config_player_mode_max_hits_upper;
    if(current_max_hits > config_max_hits_lower)
      {  
        current_max_hits --;
      }
      else
      {
        // or reset to max if already at lower
        current_max_hits = config_max_hits_upper;
      }
      current_hits = current_max_hits;
}

void incrementMaxHits (){
    // if not at lower bound, decrement
    unsigned long config_max_hits_upper = npcMode ? config_npc_mode_max_hits_upper : config_player_mode_max_hits_upper;
    if(current_max_hits < config_max_hits_upper)
      {  
        current_max_hits ++;
      }
      else
      {
        // or reset to max if already at lower
        current_max_hits = config_max_hits_lower;
      }
      current_hits = current_max_hits;
}


// --------------------------------------------------------------------------------------------------------------------------
// BUZZER SIGNALS
// --------------------------------------------------------------------------------------------------------------------------


void playSensorHit()
{
  for(int i =0; i<51;i+=5){
    tone(buzzerPin,500+(50*i),18);
    delay(22);
  }
  for(int i =0; i<51;i+=5){
    tone(buzzerPin,500+(50*i),18);
    delay(22);
  }
  for(int i =0; i<51;i+=5){
    tone(buzzerPin,500+(50*i),18);
    delay(22);
  }
}

void playSensorHitOnShields()
{
  for(int i =0; i<51;i+=5){
    tone(buzzerPin,200+(20*i),18);
    delay(22);
  }
  for(int i =0; i<51;i+=5){
    tone(buzzerPin,200+(20*i),18);
    delay(22);
  }
  for(int i =0; i<51;i+=5){
    tone(buzzerPin,200+(20*i),18);
    delay(22);
  }
}

void playHealReceived()
{
    for(int i =0; i<51;i+=3){
      tone(buzzerPin,500+(50*i),18);
      delay(22);
  }
}

void playShieldRechargeReceived()
{
    for(int i =0; i<51;i+=6){
      tone(buzzerPin,200+(50*i),18);
      delay(22);
      tone(buzzerPin,500+(50*i),18);
      delay(22);
  }
}

void playNpcModeEnter()
{
  for(int i =0; i<51;i++){
    tone(buzzerPin,500+(50*i),18);
      delay(22);
  }
  delay(250);
}

void playPlayerModeEnter()
{
  for(int i=51; i>0;i--){
    tone(buzzerPin,500+(50*i),18);
    delay(22);
  }
  delay(250);
}

void playSystemStartup()
{
  // System active siren
  for (int i=0;i<4;i++)
  {
    ledOn(startupR,startupG,startupB);
    tone(buzzerPin,1046, 400);
    delay(400);
    ledOff();
    tone(buzzerPin,523, 400);
    delay(400);
  }
}


// --------------------------------------------------------------------------------------------------------------------------
// INTERRUPT MANAGEMENT
// --------------------------------------------------------------------------------------------------------------------------

int timer1_counter;
void initialiseOneSecondClock()
{
  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  // Set timer1_counter to the correct value for our interrupt interval
  //timer1_counter = 64911;   // preload timer 65536-16MHz/256/100Hz
  //timer1_counter = 64286;   // preload timer 65536-16MHz/256/50Hz
  timer1_counter = 34286;   // preload timer 65536-16MHz/256/2Hz
  
  TCNT1 = timer1_counter;   // preload timer
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts
}



// one second clock interrupt timer
ISR(TIMER1_OVF_vect)        // interrupt service routine 
{
  TCNT1 = timer1_counter;   // preload timer
  clockTick = true;
  systemClockSeconds ++;
}

// waits until an interrupt is detected on the button pin
void buttonInterrupt()
{
  buttonPressed = true;
}

// waits until an interrupt is detected on the sensor pin
void signalInterrupt()
{
  //detachInterrupt(signalInt);

  unsigned long now = micros();
  
  // if within the period to be a shot pulse
  if(lastDetectionTime > (now - 1500))
  {
    // increment count
    detectionTracker ++;
    if(detectionTracker > 60)
    {
      beenShot = true;
    }
  }
  else
  {
    // too far away
    detectionTracker = 0;
  }
  
  lastDetectionTime = now;    
}

// Sleeps until an interrupt is triggered
void waitUntilInput()
{ 
  set_sleep_mode(SLEEP_MODE_IDLE);           // can't use any other as we want to use falling state for interrupt, can only use IDLE at lower awareness levels
  sleep_enable();                            // allow sleep
  sleep_cpu();

  // Re-enter here after interrupt
  sleep_disable();                           // unset the flag allowing cpu sleep   
}

// -------------------------------------------------------------------------------------------------------------
// LED Management
// -------------------------------------------------------------------------------------------------------------

void ledOn(byte red, byte grn, byte blue)
{
  if(rgbLed)
  {
    analogWrite(pwmRedLedPin, red);
    analogWrite(pwmGrnLedPin, grn);
    analogWrite(pwmBluLedPin, blue);
  }
  else
  {
    digitalWrite(oneLedPin, HIGH);
  }
}

void ledOff()
{
  if(rgbLed)
  {
    analogWrite(pwmRedLedPin, 0);
    analogWrite(pwmGrnLedPin, 0);
    analogWrite(pwmBluLedPin, 0);
  }
  else
  {
    digitalWrite(oneLedPin, LOW);
  }
}



